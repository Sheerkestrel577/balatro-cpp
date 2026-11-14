"""collect.py — generate training data by self-play.

Logs one row per decision point: the engine's 222-float observation plus
a few derived features, labelled with what ACTUALLY happened to that run.
Labels are ground truth from the simulator, not opinions.

    python3 collect.py --runs 500 --workers 8 --out data/gen0.npz

Output npz:
    X        (N, F)  float32   state features
    y_prog   (N,)    float32   antes cleared / 8      -- dense, always useful
    y_win    (N,)    float32   1.0 if run reached ante 8
    y_fit    (N,)    float32   K*(100/N)^2, the real objective (0 if lost)
    y_money  (N,)    float32   final money / 50
    run_id   (N,)    int32     which run each row came from
"""
from __future__ import annotations
import argparse, json, os, sys, time
import multiprocessing as mp
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)



def extra_features(g, kings=None):
    """Derived features the raw obs doesn't expose."""
    deck = max(1, g.deck_size)
    if kings is None:
        try: kings = g.deck_kings     # exact, via nf_deck
        except Exception: kings = 4
    return [
        kings / 16.0,                 # king count (exact)
        kings / deck,                 # king proportion -- the objective
        deck / 52.0,                  # thinness matters quadratically
        (100.0 / deck) ** 2 * kings / 100.0,   # scaled fitness-so-far
        g.money / 50.0,
        min(g.money, 25) / 25.0,      # interest-relevant money
        g.ante / 8.0,
        len(g.jokers()) / 5.0,
    ]


def one_run(args):
    """Play one run, returning (rows, labels)."""
    (idx, epsilon, agent_kind, model,
     mix_p, mix_max, burst_p, mix_eps, skip_p, skip_rate) = args
    import random
    from agent_v0 import Agent, PH_SHOP, PH_BLIND
    import naneinf

    rr = random.Random(idx * 7919 + 13)
    rows, pols = [], []

    def snap(g, pol):
        rows.append(np.concatenate([g.obs(),
                                    np.array(extra_features(g),
                                             dtype=np.float32)]))
        pols.append(pol)

    def attach(a, pol):
        """Snapshot state at every shop entry and blind start."""
        orig_shop, orig_playing = a.shop, a.playing

        def shop_wrapped():
            snap(a.G.g, pol); orig_shop()

        def playing_wrapped():
            if a.G.g.hands_left == 4:   # once per blind, not once per hand
                snap(a.G.g, pol)
            orig_playing()

        a.shop, a.playing = shop_wrapped, playing_wrapped

    def v0_pilot(G, seed, eps, stop_ante):
        """A v0 brain adopting a live game. Hand-play stays competent
        (shared exact-clone logic); only the SHOP is randomized -- a
        saboteur that misplays hands just dies at the next blind and
        the strategy branch never gets tested."""
        p = Agent.__new__(Agent)
        p.G = G; p.say = lambda *x: None
        p.rng = random.Random(seed)
        p._init_fields()
        p.epsilon = eps
        p.skip_explore = skip_ex
        p.stop_ante = stop_ante
        return p

    def v2_pilot(G, seed, stop_ante=8):
        from agent_v2 import RolloutAgent, _load_model
        p = RolloutAgent.__new__(RolloutAgent)
        p.G = G; p.say = lambda *x: None
        p.rng = random.Random(seed)
        p._init_fields()
        p.k = 3; p.epsilon = epsilon
        p.skip_explore = skip_ex
        p.stop_ante = stop_ante
        _load_model(model)
        return p

    # SKIP EXPLORATION. Before gen 7 no run ever skipped a blind, so the
    # value net has never seen a post-skip state and every skip tag in
    # the corpus was declined. A fraction of runs now skip eligible
    # blinds at random, generating the states the net needs in order to
    # value the choice at all. Per-RUN (not per-blind) so most runs stay
    # clean and the dataset keeps an honest majority baseline.
    skipper = rr.random() < skip_p
    skip_ex = skip_rate if skipper else 0.0

    switch = burst = 0
    if agent_kind == "v2":
        roll = rr.random()
        if roll < mix_p:
            switch = rr.randint(1, mix_max)
        elif roll < mix_p + burst_p:
            burst = rr.randint(2, 6)

    if agent_kind == "v2" and switch:
        # WEAK OPEN: v0 improvises antes 1..switch, v2 inherits the mess
        # and plays it out for real. Coverage of strategy branches the
        # strong agent never opens, labelled with what the BEST policy
        # could still make of them.
        opener = Agent(None)
        opener.rng = random.Random(idx * 104729 + 7)
        opener.skip_explore = skip_ex
        opener.epsilon = max(epsilon, mix_eps)
        opener.stop_ante = switch
        attach(opener, 1)
        r = opener.run(max_secs=60)
        a = opener
        if not (opener.G.g.done or opener.dead):
            a = v2_pilot(opener.G, idx * 7919 + 13)
            attach(a, 0)
            r = a.run(max_secs=180)
    elif agent_kind == "v2" and burst:
        # MIDGAME BURST: v2 plays to ante `burst`, v0 improvises that ONE
        # ante ("ah fuck it, tarot merchant" -- with a real midgame bank
        # and board), then v2 resumes and finishes. Branch points that
        # only EXIST in the middlegame get explored; recovery is honest.
        from agent_v2 import RolloutAgent
        first = RolloutAgent(None, k=3, epsilon=epsilon, model=model)
        first.rng = random.Random(idx * 7919 + 13)
        first.skip_explore = skip_ex
        first.stop_ante = burst - 1
        attach(first, 0)
        r = first.run(max_secs=120)
        a = first
        if not (first.G.g.done or first.dead):
            saboteur = v0_pilot(first.G, idx * 104729 + 7,
                                max(epsilon, mix_eps), stop_ante=burst)
            attach(saboteur, 1)
            r = saboteur.run(max_secs=60)
            a = saboteur
            if not (saboteur.G.g.done or saboteur.dead):
                closer = v2_pilot(first.G, idx * 7919 + 14)
                attach(closer, 0)
                r = closer.run(max_secs=180)
                a = closer
    elif agent_kind == "v2":
        from agent_v2 import RolloutAgent
        a = RolloutAgent(None, k=3, epsilon=epsilon, model=model)
        a.rng = random.Random(idx * 7919 + 13)
        a.skip_explore = skip_ex
        attach(a, 0)
        r = a.run(max_secs=180)
    else:
        a = Agent(None)
        a.rng = random.Random(idx * 7919 + 13)
        a.skip_explore = skip_ex
        a.epsilon = epsilon           # exploration: see agent_v0.shop()
        attach(a, 0)
        r = a.run(max_secs=180)

    r["mix_switch"] = switch
    r["burst"] = burst
    r["skips"] = sum(1 for x in a.G.log if x == naneinf.Action.SKIP_BLIND)
    r["skip_run"] = bool(skipper)

    g = a.G.g
    n = len(rows)
    if n == 0:
        return None
    X = np.stack(rows).astype(np.float32)
    pol = np.array(pols, dtype=np.int8)
    prog = np.float32((g.ante - 1) / 8.0)
    win = np.float32(1.0 if r["won"] else 0.0)
    fit = np.float32(r["fitness"] / 100.0)
    money = np.float32(min(r["money"], 50) / 50.0)
    return X, prog, win, fit, money, r, pol


def _ckpt_path(out_path):
    """Checkpoint filename that will NOT be picked up by a data/gen*.npz
    training glob. np.savez_compressed appends '.npz' when the name lacks
    it, so '<out>.partial' silently became '<out>.partial.npz' -- which
    DID match gen*.npz and got trained on twice. Underscore prefix keeps
    it out of the glob no matter what numpy appends."""
    d, base = os.path.split(out_path)
    return os.path.join(d, "_ckpt_" + base)


def _write(out, path, t0=None):
    """Write collected runs to npz (+ _runs.json). Used for the final
    output AND periodic .partial checkpoints, so a Ctrl-C loses at most
    checkpoint_every runs."""
    Xs, yp, yw, yf, ym, rid, pls, summaries = [], [], [], [], [], [], [], []
    for i, (X, prog, win, fit, money, r, pol) in enumerate(out):
        Xs.append(X)
        n = len(X)
        yp.append(np.full(n, prog, np.float32))
        yw.append(np.full(n, win, np.float32))
        yf.append(np.full(n, fit, np.float32))
        ym.append(np.full(n, money, np.float32))
        rid.append(np.full(n, i, np.int32))
        pls.append(pol)
        summaries.append(r)
    # ATOMIC WRITE. autoloop treats "file exists" as "phase complete", so
    # a write interrupted by a power cut or a storage dropout must never
    # leave a truncated file under the real name. Write to a temp path,
    # fsync, then rename -- rename is atomic on ext4, so the real path is
    # either the old file, absent, or the complete new one. Never partial.
    tmp = str(path) + ".tmp.npz"
    np.savez_compressed(tmp,
                        X=np.concatenate(Xs), y_prog=np.concatenate(yp),
                        y_win=np.concatenate(yw), y_fit=np.concatenate(yf),
                        y_money=np.concatenate(ym),
                        run_id=np.concatenate(rid),
                        pol=np.concatenate(pls))
    with open(tmp, "rb") as f:
        os.fsync(f.fileno())
    jpath = str(path).replace(".npz", "") + "_runs.json"
    with open(jpath + ".tmp", "w") as f:
        json.dump(summaries, f)
        f.flush(); os.fsync(f.fileno())
    os.replace(tmp, path)
    os.replace(jpath + ".tmp", jpath)
    return summaries


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", type=int, default=200)
    ap.add_argument("--workers", type=int, default=max(1, mp.cpu_count() - 1))
    ap.add_argument("--epsilon", type=float, default=0.15,
                    help="chance of a random shop purchase (exploration)")
    ap.add_argument("--out", default="data/gen0.npz")
    ap.add_argument("--checkpoint_every", type=int, default=100,
                    help="write a partial npz every N runs (0 = off)")
    ap.add_argument("--agent", choices=["v0", "v2"], default="v0",
                    help="v0: fast dict shop (~0.05s/run). v2: honest "
                         "rollout shop (~3-4s/run, far better win rate)")
    ap.add_argument("--model", default=None,
                    help="value net for v2's rollout terminal score")
    ap.add_argument("--mix", type=float, default=0.0,
                    help="fraction of v2 runs that get a WEAK OPENING: "
                         "v0 at epsilon>=0.25 improvises the first 1..N "
                         "antes, then v2 takes over and plays it out. "
                         "Strategy-branch exploration with honest labels. "
                         "0.2-0.3 is plenty.")
    ap.add_argument("--mix_max_ante", type=int, default=3,
                    help="latest ante at which the strong agent takes over")
    ap.add_argument("--burst", type=float, default=0.0,
                    help="fraction of v2 runs with a MIDGAME improvisation "
                         "burst: v2 plays to a random ante in 2..6, v0 at "
                         "high epsilon improvises that one ante, v2 resumes "
                         "and finishes. Explores branch points that only "
                         "exist with a midgame bank/board. 0.1-0.2.")
    ap.add_argument("--skip_explore", type=float, default=0.0,
                    help="fraction of runs that are SKIP-EXPLORATION runs: "
                         "eligible blinds are skipped at --skip_rate, "
                         "generating post-skip states the value net has "
                         "never seen. 0.2-0.3 while bootstrapping.")
    ap.add_argument("--skip_rate", type=float, default=0.35,
                    help="per-blind skip probability inside such a run")
    ap.add_argument("--mix_eps", type=float, default=0.30,
                    help="shop-randomness of the saboteur/opener")
    args = ap.parse_args()

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    t0 = time.time()
    tasks = [(i, args.epsilon, args.agent, args.model,
              args.mix, args.mix_max_ante, args.burst, args.mix_eps,
              args.skip_explore, args.skip_rate)
             for i in range(args.runs)]
    out, antes_so_far, wins_so_far = [], [], 0
    skipped = 0

    def note(o):
        nonlocal skipped
        if o is None:
            skipped += 1
            return
        out.append(o)
        r = o[5]
        antes_so_far.append(r["ante"])
        n = len(out)
        w = sum(1 for x in out if x[5]["won"])
        el = time.time() - t0
        rate = el / n
        eta = rate * (args.runs - n)
        print(f"\r[{n}/{args.runs}] ante avg {sum(antes_so_far)/n:.2f} "
              f"max {max(antes_so_far)} wins {w} | "
              f"{rate:.1f}s/run, ETA {eta/60:.1f} min   ",
              end="", flush=True)
        if n % 25 == 0:
            print()                     # keep a scrollback trail
        if args.checkpoint_every and n % args.checkpoint_every == 0:
            _write(out, _ckpt_path(args.out))

    # DRAIN WITH A DEADLINE. mp.Pool never re-queues a task whose worker
    # died (Windows WinError-87 spawn failures make this a real event,
    # not a hypothetical): a bare `for o in imap_unordered(...)` then
    # waits forever for results that cannot arrive -- the classic stall
    # at exactly runs-minus-workers. A stall longer than any legal run
    # chain (worst case burst: 120+60+180=360s) means the remainder is
    # lost; write what we have. Ctrl-C likewise writes before dying.
    STALL = 600
    try:
        if args.workers > 1:
            with mp.Pool(args.workers) as p:   # __exit__ terminates zombies
                it = p.imap_unordered(one_run, tasks)
                while True:
                    try:
                        o = it.next(timeout=STALL)
                    except StopIteration:
                        break
                    except mp.TimeoutError:
                        print(f"\n!! no result for {STALL}s -- "
                              f"{args.runs - len(out) - skipped} task(s) "
                              f"lost to dead/stuck workers; writing the "
                              f"{len(out)} completed runs and moving on")
                        break
                    note(o)
        else:
            for t_ in tasks:
                note(one_run(t_))
    except KeyboardInterrupt:
        print(f"\n!! interrupted -- writing {len(out)} completed runs")
    print()

    if not out:
        print("no completed runs -- nothing written"); return
    summaries = _write(out, args.out)
    antes = [r["ante"] for r in summaries]
    wins = sum(r["won"] for r in summaries)
    print(f"{len(summaries)} runs, {sum(len(o[0]) for o in out)} states, "
          f"{time.time()-t0:.0f}s")
    print(f"  ante mean {np.mean(antes):.2f}  max {max(antes)}  "
          f"wins {wins} ({100*wins/len(summaries):.1f}%)")
    print(f"  wrote {args.out}")
    # remove the checkpoint now that the full file is written
    ck = _ckpt_path(args.out)
    for p in (ck, ck if ck.endswith(".npz") else ck + ".npz",
              ck.replace(".npz", "") + "_runs.json"):
        try: os.remove(p)
        except OSError: pass


if __name__ == "__main__":
    main()
