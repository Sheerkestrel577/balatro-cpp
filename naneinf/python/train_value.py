"""train_value.py — fit the value net V on collected self-play data.

Multi-head MLP: state -> (progress, win prob, fitness, money).
The auxiliary heads exist because early data has ZERO wins: y_fit is all
zeros and teaches nothing, while y_prog ("how far did this run get") is
dense and informative from run one. The heads share a trunk, so learning
"what does progress look like" shapes the same features the fitness head
will need once wins appear.

Pure numpy on purpose: the net is tiny (230 -> 256 -> 128 -> 4) and this
removes a dependency for iteration one. Swap in torch when you want GPU
batching for the MCTS inner loop -- the architecture is unchanged.

    python3 train_value.py --data data/gen0.npz --out models/v_gen0.npz
    python3 train_value.py --data "data/gen*.npz" --out models/v_gen1.npz

STREAMING (v2): the corpus is never held in RAM at once. Each generation
file is a CHUNK; every epoch visits every chunk in a fresh random order,
loading one, doing one pass of shuffled minibatches, dropping it. RAM is
bounded by the largest single file (~300 MB), independent of how many
generations exist -- so nothing is ever pruned and nothing is forgotten.
Global stats (mu/sd, TD targets, val split) come from a cheap streaming
pre-pass. TD targets are cached to disk per chunk so the champion net
runs once per file, not once per epoch.
"""
from __future__ import annotations
import argparse, glob, json, os, sys, time
import numpy as np

# autoloop runs phases as `sys.executable train_value.py` with stdout
# redirected to a log FILE, which block-buffers at 8 KB. Progress that
# sits in that buffer is invisible while the phase runs and is LOST
# outright if the phase is killed (see OPERATIONS.md 8). Line buffering
# makes every print below land in the log the moment it happens, with no
# dependence on the caller remembering `-u`.
try:
    sys.stdout.reconfigure(line_buffering=True)
except Exception:                       # pragma: no cover -- old python
    pass


def _hms(sec):
    sec = int(max(0, sec))
    h, m = divmod(sec, 3600)
    m, s_ = divmod(m, 60)
    return f"{h:d}:{m:02d}:{s_:02d}" if h else f"{m:d}:{s_:02d}"

HEADS = ["prog", "win", "fit", "money"]
# Base weights. These are ADAPTED at runtime by head_weights() below --
# a head whose labels are almost all zeros (y_fit with 4 wins in 500 runs)
# cannot be learned, and weighting it heavily just injects noise into the
# shared trunk and prints a meaningless R^2 of -8.
HEAD_W_BASE = np.array([1.0, 0.7, 1.5, 0.3], dtype=np.float32)
MIN_POSITIVE_RUNS = 20      # below this, a sparse head is not trainable


def head_weights(Y, rid, verbose=True):
    """Down-weight heads with too few positive RUNS to learn from."""
    w = HEAD_W_BASE.copy()
    for i, h in enumerate(HEADS):
        if h not in ("win", "fit"):
            continue
        pos_runs = len(np.unique(rid[Y[:, i] > 0]))
        if pos_runs < MIN_POSITIVE_RUNS:
            w[i] = np.float32(0.05)   # a whisper, not a shout
            if verbose:
                print(f"  head '{h}': only {pos_runs} positive runs "
                      f"(<{MIN_POSITIVE_RUNS}) -> weight {w[i]}; it is not "
                      f"learnable yet and its R^2 will read as noise")
    return w


class MLP:
    def __init__(self, d_in, h1=256, h2=128, n_out=4, seed=0):
        r = np.random.default_rng(seed)
        s = lambda a, b: r.normal(0, np.sqrt(2.0 / a), (a, b)).astype(np.float32)
        self.W1, self.b1 = s(d_in, h1), np.zeros(h1, np.float32)
        self.W2, self.b2 = s(h1, h2), np.zeros(h2, np.float32)
        self.W3, self.b3 = s(h2, n_out), np.zeros(n_out, np.float32)
        self.params = ["W1", "b1", "W2", "b2", "W3", "b3"]
        self.m = {k: np.zeros_like(getattr(self, k)) for k in self.params}
        self.v = {k: np.zeros_like(getattr(self, k)) for k in self.params}
        self.t = 0

    def forward(self, X):
        z1 = X @ self.W1 + self.b1; a1 = np.maximum(z1, 0)
        z2 = a1 @ self.W2 + self.b2; a2 = np.maximum(z2, 0)
        out = a2 @ self.W3 + self.b3
        return out, (X, z1, a1, z2, a2)

    def backward(self, cache, dout):
        X, z1, a1, z2, a2 = cache
        n = len(X)
        g = {}
        g["W3"] = a2.T @ dout / n; g["b3"] = dout.mean(0)
        da2 = dout @ self.W3.T; dz2 = da2 * (z2 > 0)
        g["W2"] = a1.T @ dz2 / n; g["b2"] = dz2.mean(0)
        da1 = dz2 @ self.W2.T; dz1 = da1 * (z1 > 0)
        g["W1"] = X.T @ dz1 / n; g["b1"] = dz1.mean(0)
        return g

    def adam(self, g, lr=1e-3, b1=0.9, b2=0.999, eps=1e-8, wd=1e-5):
        wd = getattr(self, 'wd', wd)
        self.t += 1
        for k in self.params:
            p = getattr(self, k)
            grad = g[k] + wd * p
            self.m[k] = b1 * self.m[k] + (1 - b1) * grad
            self.v[k] = b2 * self.v[k] + (1 - b2) * grad ** 2
            mh = self.m[k] / (1 - b1 ** self.t)
            vh = self.v[k] / (1 - b2 ** self.t)
            setattr(self, k, p - lr * mh / (np.sqrt(vh) + eps))

    def save(self, path, mu, sd):
        os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
        np.savez(path, mu=mu, sd=sd, heads=np.array(HEADS),
                 **{k: getattr(self, k) for k in self.params})

    @staticmethod
    def load(path):
        d = np.load(path, allow_pickle=False)
        net = MLP(d["W1"].shape[0], d["W1"].shape[1], d["W2"].shape[1],
                  d["W3"].shape[1])
        for k in net.params:
            setattr(net, k, d[k])
        return net, d["mu"], d["sd"]


def predict(net, mu, sd, X, chunk=16384):
    """Forward pass in chunks.

    forward() keeps X, z1, a1 (h1=256), z2, a2 (h2=128) alive at once, so
    calling it on the whole dataset allocates ~5x the data size in one
    go -- at window=4 that is ~5.4 GB on top of everything already
    resident, which is what starves an 8 GB box (and makes sshd fail to
    fork). Chunking is mathematically identical: no cross-row terms."""
    outs = []
    for i in range(0, len(X), chunk):
        xb = np.clip((X[i:i + chunk] - mu) / sd, -10, 10).astype(np.float32)
        o, _ = net.forward(xb)
        outs.append(o)
    return np.concatenate(outs) if outs else np.zeros((0, 4), np.float32)


def run_slices(rid):
    """(start, stop) per run, O(n) once.

    collect.py appends runs in order, so rid is non-decreasing after the
    per-file offsetting -- each run is a CONTIGUOUS block. That turns
    every per-run operation into a slice (a view, no scan) instead of a
    boolean mask over the whole array. The mask form costs one full pass
    over X per run: at ~63k runs and 1.15 GB that is tens of TB of
    memory traffic, i.e. hours of thrashing on a small box, which is
    what wedged gen 7. If rid ever arrives unsorted we sort it here.
    """
    if len(rid) and np.any(np.diff(rid) < 0):
        order = np.argsort(rid, kind="stable")
        return order, None
    starts = np.flatnonzero(np.concatenate(([True], np.diff(rid) != 0)))
    stops = np.concatenate((starts[1:], [len(rid)]))
    return None, list(zip(starts.tolist(), stops.tolist()))


def td_targets(X, Y, rid, target_path, n=5, beta=0.5, verbose=True):
    """Roadmap item 3: bootstrap labels from a TARGET NET.

    Monte-Carlo labels (what collect.py writes) give every state in a run
    the same final outcome: state 2 of a run that died at ante 6 is
    labelled "died at ante 6" even though the death was decided 40
    decisions later. High variance, zero credit assignment.

    TD(n) replaces the label of state t with what the PREVIOUS
    generation's net says about state t+n in the SAME run:

        y_t := beta * MC_outcome  +  (1-beta) * V_target(s_{t+n})

    i.e. "your score should match your own score n decisions deeper"
    (n decision points ~ n snapshots: one per shop entry + one per
    blind). The last n states of each run keep the pure MC label -- there
    the outcome IS nearby, and it anchors the bootstrap chain to ground
    truth so values can't drift off into self-confirming fiction.

    Applied to prog/win/fit heads; money stays MC (it's a garnish).
    Requires --td <prev model>; rows within a run are already in
    chronological order (collect.py appends as it plays).
    """
    net, mu, sd = MLP.load(target_path)
    P = predict(net, mu, sd, X)
    # bootstrap predictions must live in label space or the blend leaks
    # nonsense into training: clip to each head's achievable range.
    P[:, 0] = np.clip(P[:, 0], 0.0, 9.0 / 8.0)      # prog
    P[:, 1] = np.clip(P[:, 1], 0.0, 1.0)            # win
    P[:, 2] = np.clip(P[:, 2], 0.0, None)           # fit >= 0
    Yt = Y.copy()
    heads = [0, 1, 2]
    changed = 0
    _, spans = run_slices(rid)
    for a_, b_ in spans:                   # contiguous, chronological
        if b_ - a_ <= n:
            continue                       # short run: outcome is nearby
        src = slice(a_, b_ - n)            # states that get a bootstrap
        boot = P[a_ + n:b_, heads]
        Yt[src, 0:3] = (beta * Y[src, 0:3] + (1.0 - beta) * boot)
        changed += (b_ - n) - a_
    if verbose:
        print(f"  TD(n={n}, beta={beta}): bootstrapped {changed}/{len(X)} "
              f"states from {os.path.basename(target_path)} "
              f"(prog/win/fit heads; last {n} states of each run stay MC)")
    return Yt



# ============================================================ streaming core
class Chunk:
    """One generation file: loaded on demand, dropped after use.

    Holds the per-file arrays plus derived things computed once in the
    pre-pass (val mask, TD targets on disk). `load()` re-reads the npz
    each time it is needed; at ~50 MB per file that is ~1s from an
    NVMe and negligible against a minute of matmuls per epoch.
    """
    def __init__(self, path, file_idx):
        self.path, self.file_idx = path, file_idx
        with np.load(path) as d:
            self.n = len(d["run_id"])
            self.n_runs = int(d["run_id"].max()) + 1 if self.n else 0
        self.va = None            # bool mask, set in pre-pass
        self.td_path = None       # cached TD targets, set if --td
        self.d_in = None

    def load(self):
        d = np.load(self.path)
        X = d["X"].astype(np.float32, copy=False)
        Y = np.stack([d[f"y_{h}"] for h in HEADS], 1).astype(np.float32)
        rid = d["run_id"].astype(np.int64)
        return X, Y, rid


def discover(patterns):
    files = sorted({f for pat in patterns for f in glob.glob(pat)})
    missing = [p for p in patterns if not glob.glob(p)]
    if missing:
        print(f"  (skipping {len(missing)} absent: "
              f"{', '.join(os.path.basename(m) for m in missing)})")
    if not files:
        raise SystemExit(f"no data files matched: {patterns}")
    return files


def prepass(chunks, val_frac, seed, td_model, td_n, td_beta, cache_dir):
    """One streaming sweep over every chunk that produces everything the
    epoch loop needs WITHOUT ever holding two chunks at once:

      * dedupe across files (content hash per run; a checkpoint that
        matches a final file must not be double-counted)
      * mu / sd from the TRAIN rows only, via sum & sum-of-squares
      * per-chunk val mask (a fixed fraction of RUNS, seeded, so the
        split is stable across generations and comparable)
      * per-chunk TD targets from the champion, cached to disk
      * hall-of-fame style positive-run counts for head_weights
    """
    rng = np.random.default_rng(seed)
    seen = set()
    d_in = None
    s1 = s2 = None
    n_tr = 0
    pos_runs = np.zeros(len(HEADS), np.int64)
    n_states = n_runs = 0
    dropped = 0
    net = mu_t = sd_t = None
    if td_model:
        net, mu_t, sd_t = MLP.load(td_model)
    # The pre-pass is the longest silent stretch in the whole phase: it
    # reads every chunk and, with --td, runs the champion over every state
    # to build TD targets. Report per file so the log grows while it works.
    t_pre = time.time()
    print(f"  prepass: {len(chunks)} file(s)"
          + (f", TD targets from {os.path.basename(td_model)}"
             if td_model else ", no TD"), flush=True)
    for ci_, c in enumerate(chunks):
        t_c = time.time()
        X, Y, rid = c.load()
        if d_in is None:
            d_in = X.shape[1]
            s1 = np.zeros(d_in, np.float64); s2 = np.zeros(d_in, np.float64)
        c.d_in = d_in
        _, spans = run_slices(rid)
        keep = np.ones(len(X), bool)
        for a_, b_ in spans:
            h = hash(X[a_:b_].tobytes())
            if h in seen:
                keep[a_:b_] = False; dropped += 1
            else:
                seen.add(h)
        c.keep = keep
        # val split by RUN, seeded per file so it never changes
        runs = np.unique(rid[keep])
        if len(runs) == 0:                 # entirely duplicate file
            c.va = c.tr = np.zeros(len(X), bool)
            c.empty = True
            del X, Y, rid
            continue
        c.empty = False
        r_local = np.random.default_rng(seed + 7919 * c.file_idx)
        n_val = max(1, int(len(runs) * val_frac))
        val_runs = r_local.choice(runs, n_val, replace=False)
        va = np.isin(rid, val_runs) & keep
        tr = keep & ~va
        c.va, c.tr = va, tr
        # moments from train rows only
        xb = X[tr].astype(np.float64)
        s1 += xb.sum(0); s2 += (xb * xb).sum(0); n_tr += len(xb)
        # positives for head weighting
        for i in range(len(HEADS)):
            pos_runs[i] += len(np.unique(rid[keep & (Y[:, i] > 0)]))
        n_states += int(keep.sum()); n_runs += len(runs)
        # TD targets: champion evaluated ONCE per chunk, cached
        if net is not None:
            Yt = td_targets(X, Y, rid, td_model, n=td_n, beta=td_beta,
                            verbose=False)
            os.makedirs(cache_dir, exist_ok=True)
            c.td_path = os.path.join(
                cache_dir, f"td_{c.file_idx}_{os.path.basename(c.path)}.npy")
            np.save(c.td_path, Yt.astype(np.float32))
        del X, Y, rid, xb
        done = ci_ + 1
        el = time.time() - t_pre
        eta = el / done * (len(chunks) - done)
        print(f"    [{done}/{len(chunks)}] {os.path.basename(c.path):<16}"
              f" {c.n:>8} states  {time.time() - t_c:6.1f}s"
              f"  (elapsed {_hms(el)}, eta {_hms(eta)})", flush=True)
    mu = (s1 / n_tr).astype(np.float32)
    sd = np.maximum(np.sqrt(np.maximum(s2 / n_tr - (s1 / n_tr) ** 2, 0)),
                    1e-2).astype(np.float32)
    if dropped:
        print(f"  DEDUPE: dropped {dropped} duplicate runs across files")
    print(f"{len(chunks)} file(s): {n_states} states from {n_runs} runs, "
          f"train rows {n_tr}")
    return d_in, mu, sd, pos_runs


def head_weights_from_counts(pos_runs, verbose=True):
    w = HEAD_W_BASE.copy()
    for i, h in enumerate(HEADS):
        if h in ("win", "fit") and pos_runs[i] < MIN_POSITIVE_RUNS:
            w[i] = np.float32(0.05)
            if verbose:
                print(f"  head '{h}': only {pos_runs[i]} positive runs "
                      f"(<{MIN_POSITIVE_RUNS}) -> weight {w[i]}")
    return w


def evaluate(net, mu, sd, chunks):
    """Val metrics streamed across chunks, always against RAW MC labels."""
    se = np.zeros(len(HEADS)); base_se = np.zeros(len(HEADS))
    ysum = np.zeros(len(HEADS)); n = 0
    # two passes: mean of val labels first (for the baseline), then errors
    for c in chunks:
        if getattr(c, "empty", False):
            continue
        X, Y, _ = c.load()
        ysum += Y[c.va].sum(0); n += int(c.va.sum())
        del X, Y
    ymean = ysum / max(1, n)
    preds, ys = [], []
    for c in chunks:
        if getattr(c, "empty", False):
            continue
        X, Y, _ = c.load()
        P = predict(net, mu, sd, X[c.va])
        Yv = Y[c.va]
        se += ((P - Yv) ** 2).sum(0)
        base_se += ((ymean - Yv) ** 2).sum(0)
        preds.append(P[:, 0]); ys.append(Yv[:, 0])
        del X, Y
    r2 = 1.0 - se / np.maximum(base_se, 1e-12)
    return r2, se.sum() / max(1, n), np.concatenate(preds), np.concatenate(ys)


def train_epoch(net, mu, sd, chunks, W, batch, lr, rng, tag=""):
    """One epoch = one pass over every chunk in random order.

    `tag` non-empty prints a line per chunk. On a Pi a single epoch over
    the full corpus is minutes, so without this the log is silent long
    enough to be indistinguishable from the D-state hang in OPERATIONS 8.
    """
    order = rng.permutation(len(chunks))
    tot, cnt = 0.0, 0
    t_ep = time.time()
    n_done = 0
    for ci in order:
        c = chunks[ci]
        if getattr(c, "empty", False):
            continue
        X, Y, _ = c.load()
        if c.td_path:
            Y = np.load(c.td_path)
        idx = np.flatnonzero(c.tr)
        rng.shuffle(idx)
        for i in range(0, len(idx), batch):
            b = idx[i:i + batch]
            Xb = np.clip((X[b] - mu) / sd, -10, 10)
            out, cache = net.forward(Xb)
            diff = (out - Y[b]) * W
            loss = float((diff * diff).mean())
            g = net.backward(cache, 2.0 * diff * W / len(HEADS))
            net.adam(g, lr=lr)
            tot += loss; cnt += 1
        del X, Y
        n_done += 1
        if tag:
            print(f"    {tag} [{n_done}/{len(chunks)}]"
                  f" {os.path.basename(c.path):<16}"
                  f" loss {tot / max(1, cnt):.4f}"
                  f"  {time.time() - t_ep:6.1f}s", flush=True)
    return tot / max(1, cnt)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", nargs="+", default=["data/gen0.npz"],
                    help="one or more npz paths / globs")
    ap.add_argument("--out", default="models/v_gen0.npz")
    ap.add_argument("--epochs", type=int, default=60)
    ap.add_argument("--batch", type=int, default=256)
    ap.add_argument("--lr", type=float, default=1e-3)
    ap.add_argument("--h1", type=int, default=256)
    ap.add_argument("--h2", type=int, default=128)
    ap.add_argument("--wd", type=float, default=1e-5,
                    help="weight decay; raise to ~1e-3 for small datasets")
    ap.add_argument("--td", default=None,
                    help="previous-generation model to bootstrap TD "
                         "targets from. Use the LAST generation's net, "
                         "never the one being trained.")
    ap.add_argument("--td_n", type=int, default=5)
    ap.add_argument("--td_beta", type=float, default=0.5)
    ap.add_argument("--val_frac", type=float, default=0.2)
    ap.add_argument("--patience", type=int, default=10)
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--cache", default=".td_cache",
                    help="dir for per-chunk TD target cache (auto-cleaned)")
    ap.add_argument("--chunk_progress", action="store_true",
                    help="also print a line per chunk WITHIN each epoch. "
                         "On the Pi an epoch over the full corpus is long "
                         "enough that per-epoch output alone can't "
                         "distinguish 'working' from 'hung'.")
    args = ap.parse_args()

    files = discover(args.data)
    chunks = [Chunk(f, i) for i, f in enumerate(files)]
    for c in chunks:
        print(f"  {c.path}: {c.n} states, {c.n_runs} runs")

    d_in, mu, sd, pos_runs = prepass(
        chunks, args.val_frac, args.seed, args.td, args.td_n, args.td_beta,
        args.cache)
    if args.td:
        print(f"  TD(n={args.td_n}, beta={args.td_beta}) from "
              f"{os.path.basename(args.td)}, cached per chunk")
    W = head_weights_from_counts(pos_runs)

    net = MLP(d_in, args.h1, args.h2, len(HEADS), seed=args.seed)
    net.wd = args.wd
    rng = np.random.default_rng(args.seed)

    best_r2, best_ep, best_state, stale = -1e9, 0, None, 0
    t_train = time.time()
    print(f"  training: up to {args.epochs} epochs, patience {args.patience}",
          flush=True)
    for ep in range(1, args.epochs + 1):
        t_ep0 = time.time()
        tl = train_epoch(net, mu, sd, chunks, W, args.batch, args.lr, rng,
                         tag=(f"ep {ep:3d}" if args.chunk_progress else ""))
        r2, vloss, _, _ = evaluate(net, mu, sd, chunks)
        # evaluate() already ran on EVERY epoch before this patch; the
        # result was simply discarded 9 times out of 10. Printing it is
        # free -- the val cost was always being paid.
        dt = time.time() - t_ep0
        el = time.time() - t_train
        eta = el / ep * (args.epochs - ep)
        print(f"  ep {ep:3d}/{args.epochs} train {tl:.4f} | val R^2 "
              + "  ".join(f"{h} {r2[i]:+.3f}" for i, h in enumerate(HEADS))
              + f" | {dt:.1f}s  elapsed {_hms(el)}  eta {_hms(eta)}"
              + (f"  best ep {best_ep} ({best_r2:+.3f}), stale {stale}"
                 if best_ep else ""),
              flush=True)
        if r2[0] > best_r2 + 1e-4:
            best_r2, best_ep, stale = r2[0], ep, 0
            best_state = {k: getattr(net, k).copy() for k in net.params}
        else:
            stale += 1
            if stale >= args.patience:
                print(f"  early stop at ep {ep}: no val gain for "
                      f"{args.patience} epochs (best ep {best_ep}, "
                      f"prog R^2 {best_r2:+.3f})")
                break
    if best_state:
        for k, v in best_state.items():
            setattr(net, k, v)
        print(f"  restored best snapshot: epoch {best_ep}, "
              f"val prog R^2 {best_r2:+.3f}")

    # atomic save: never leave a half-written model where autoloop can see it
    tmp = args.out + ".tmp.npz"
    net.save(tmp, mu, sd)
    os.replace(tmp, args.out)
    print(f"wrote {args.out}")

    # calibration on the progress head, streamed
    _, _, pv, ya = evaluate(net, mu, sd, chunks)
    print("\nCALIBRATION (val, progress head) -- predicted vs actual:")
    order = np.argsort(pv)
    for q in np.array_split(order, 5):
        print(f"  pred {pv[q].mean():.3f} -> actual {ya[q].mean():.3f} "
              f"(n={len(q)})")

    # clean the TD cache
    for c in chunks:
        if c.td_path and os.path.exists(c.td_path):
            os.remove(c.td_path)


if __name__ == "__main__":
    main()
