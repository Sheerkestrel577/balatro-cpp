#!/usr/bin/env python3
"""autoloop.py -- the whole generation loop, unattended.

    collect (champion drives) -> harvest wins -> train candidate (TD off
    champion) -> paired gate eval -> promote-or-reject -> prune -> repeat

Built to be left alone on a small box for weeks:
  * every phase is idempotent (output exists => skip), so power cuts and
    restarts resume mid-generation instead of redoing or corrupting work
  * phases run as subprocesses with hard timeouts; failures back off and
    retry forever rather than exiting -- systemd only needs to cover
    crashes and reboots
  * CHAMPION model: collection is always driven by the best net so far,
    never automatically by the newest. A candidate must win a paired
    eval on identical seeds to take over. Losing candidates are kept on
    disk but never steer data collection (a bad net poisons the next
    dataset).
  * HALL OF FAME: winning runs are ~0.5% of data but 100% of the win
    head's signal. They are harvested from every generation into
    data/hof.npz, which is ALWAYS in the training set even after the
    generation that produced them is pruned. Old junk ages out; wins
    are forever.
  * training window: last --window generations + hall of fame, bounding
    RAM and disk on an 8GB box.

    python3 autoloop.py --runs_per_gen 8000 --workers 4      # pi5
    tail -f logs/loop.log ; cat status.json ; cat history.csv
"""
from __future__ import annotations
import argparse, csv, glob, json, os, subprocess, sys, time
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
os.chdir(HERE)
PY = sys.executable


def log(msg):
    line = f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] {msg}"
    print(line, flush=True)
    with open("logs/loop.log", "a") as f:
        f.write(line + "\n")


def save_json_atomic(path, obj):
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(obj, f, indent=1)
    os.replace(tmp, path)


def sh(cmd, logfile, timeout):
    """Run a phase subprocess, appending output to its log."""
    with open(logfile, "a") as lf:
        lf.write(f"\n$ {' '.join(cmd)}\n")
        lf.flush()
        try:
            p = subprocess.run(cmd, stdout=lf, stderr=subprocess.STDOUT,
                               timeout=timeout, cwd=HERE)
            return p.returncode
        except subprocess.TimeoutExpired:
            lf.write(f"\n!! phase timeout after {timeout}s\n")
            return -9


def phase(name, cmd, logfile, timeout, done_path=None, retries=2):
    """Idempotent, self-healing phase runner. Never raises, never exits:
    on repeated failure it backs off for an hour and tries again --
    'forget about it' means self-heal on transient trouble (thermal
    throttle-kill, disk hiccup), not die quietly at 3am."""
    if done_path and os.path.exists(done_path):
        log(f"  {name}: exists, skip ({done_path})")
        return
    attempt = 0
    while True:
        attempt += 1
        log(f"  {name}: attempt {attempt}")
        rc = sh(cmd, logfile, timeout)
        if rc == 0 and (done_path is None or os.path.exists(done_path)):
            return
        log(f"  {name}: FAILED rc={rc} (see {logfile})")
        time.sleep(60 if attempt <= retries else 3600)


def harvest_wins(gen_npz, hof_path):
    """Append this generation's winning runs to the hall of fame."""
    d = np.load(gen_npz)
    rid = d["run_id"]
    winners = np.unique(rid[d["y_win"] > 0])
    if len(winners) == 0:
        return 0
    m = np.isin(rid, winners)
    pol = d["pol"][m] if "pol" in d.files else np.zeros(m.sum(), np.int8)
    new = dict(X=d["X"][m], y_prog=d["y_prog"][m], y_win=d["y_win"][m],
               y_fit=d["y_fit"][m], y_money=d["y_money"][m],
               run_id=d["run_id"][m], pol=pol)
    if os.path.exists(hof_path):
        h = np.load(hof_path)
        off = h["run_id"].max() + 1
        hp = h["pol"] if "pol" in h.files else np.zeros(len(h["run_id"]),
                                                        np.int8)
        new = dict(
            X=np.concatenate([h["X"], new["X"]]),
            y_prog=np.concatenate([h["y_prog"], new["y_prog"]]),
            y_win=np.concatenate([h["y_win"], new["y_win"]]),
            y_fit=np.concatenate([h["y_fit"], new["y_fit"]]),
            y_money=np.concatenate([h["y_money"], new["y_money"]]),
            run_id=np.concatenate([h["run_id"], new["run_id"] + off]),
            pol=np.concatenate([hp, new["pol"]]))
    tmp = hof_path + ".tmp.npz"
    np.savez_compressed(tmp, **new)
    with open(tmp, "rb") as f:
        os.fsync(f.fileno())               # on disk before the rename
    os.replace(tmp, hof_path)
    return len(winners)


def paired_verdict(eval_json, z=0.0):
    """Read batch.py's output json; return (promote, detail).

    Both sides faced the same seed list, so this is a paired test.

    The rule used to be `wd > 0 or (wd == 0 and ad > 0)` -- wins decide,
    ante breaks exact ties. That was written before the win rate was
    known to be ~0.4%. At 200 eval seeds each side expects UNDER ONE win,
    so `wd` is a difference of two near-zero counts and carries almost no
    information, while the paired ante diff has a standard error near
    0.09. The damage was asymmetric: wd == -1, which a single lucky seed
    on the incumbent's side produces routinely, hard-vetoed the candidate
    no matter how much better its ante was.

    So: trust `wd` only when it exceeds its own noise. Win counts are
    roughly Poisson, so the s.d. of the difference is about
    sqrt(wins_a + wins_b); inside that band the wins are indistinguishable
    and the tighter signal (ante) decides. This is self-scaling -- if the
    win rate ever climbs, sqrt(total) stops swamping wd and wins quietly
    take over again with no further change here.

    `z` sets how much ante must clear its own standard error to promote
    inside the band. z=0 reproduces the old tiebreak exactly (any positive
    ante diff promotes), so this patch changes ONE thing: a small negative
    win diff no longer holds a veto. Raise z later if the champion churns.
    """
    d = json.load(open(eval_json))
    labels = list(d.keys())
    a, b = d[labels[0]], d[labels[1]]         # a = candidate, b = incumbent
    da = {r["seed"]: r for r in a}
    db = {r["seed"]: r for r in b}
    common = [s for s in da if s in db]
    n = max(1, len(common))
    wa = sum(int(da[s]["won"]) for s in common)
    wb = sum(int(db[s]["won"]) for s in common)
    wd = wa - wb
    diffs = [da[s]["ante"] - db[s]["ante"] for s in common]
    ad = sum(diffs) / n
    if len(diffs) > 1:
        var = sum((x - ad) ** 2 for x in diffs) / len(diffs)
        se = (var ** 0.5) / (len(diffs) ** 0.5)
    else:
        se = 0.0
    band = max(1.0, (wa + wb) ** 0.5)         # wins below this are noise
    if wd > band:
        promote, basis = True, "wins"
    elif wd < -band:
        promote, basis = False, "wins"
    else:
        promote, basis = (ad > z * se), "ante"
    return promote, (f"{labels[0]} vs {labels[1]}: paired win diff {wd:+d} "
                     f"({wa} vs {wb}, noise band +/-{band:.1f}), "
                     f"ante diff {ad:+.2f} +/-{se:.2f} over {len(common)} "
                     f"seeds -> decided on {basis} "
                     f"-> {'PROMOTE' if promote else 'reject'}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs_per_gen", type=int, default=8000)
    ap.add_argument("--eval_runs", type=int, default=120)
    ap.add_argument("--workers", type=int, default=4)
    ap.add_argument("--window", type=int, default=0,
                    help="0 = train on EVERY generation ever collected "
                         "(streaming trainer, RAM bounded by the largest "
                         "single file). N>0 restricts to the last N gens "
                         "-- only for experiments, it invites forgetting.")
    ap.add_argument("--epsilon", type=float, default=0.08)
    ap.add_argument("--mix", type=float, default=0.2)
    ap.add_argument("--burst", type=float, default=0.15)
    ap.add_argument("--gate_z", type=float, default=0.0,
                    help="inside the win-noise band, ante must exceed this "
                         "many standard errors to promote. 0 = old tiebreak "
                         "semantics (any positive ante diff promotes).")
    ap.add_argument("--skip_explore", type=float, default=0.25,
                    help="fraction of runs that force-explore blind skips. "
                         "The corpus before gen 7 contains ZERO skips, so "
                         "the net cannot value the choice until these runs "
                         "exist. Lower to ~0.1 once a few generations of "
                         "skip data are in the window.")
    ap.add_argument("--max_gens", type=int, default=10**6)
    args = ap.parse_args()

    os.makedirs("logs", exist_ok=True)
    os.makedirs("data", exist_ok=True)
    os.makedirs("models", exist_ok=True)

    state = {"gen": 0, "champion": None, "rejects": 0}
    if os.path.exists("loop_state.json"):
        state.update(json.load(open("loop_state.json")))
    log(f"loop start: state={state}")

    while state["gen"] < args.max_gens:
        g = state["gen"] + 1
        champ = state["champion"]
        dat = f"data/gen{g}.npz"
        cand = f"models/v_gen{g}.npz"
        evj = f"logs/eval_gen{g}.json"
        lgf = f"logs/gen{g}.log"
        log(f"=== generation {g} (champion: {champ or 'none'}) ===")

        # ANTI-STAGNATION. A frozen champion means frozen TD targets and
        # a data stream from one policy: the loop can sit at a fixed
        # point for weeks with every candidate tying the incumbent. Turn
        # up exploration as rejects accumulate so the next generation's
        # data actually differs, and widen the eval so a real-but-small
        # gain stops being invisible.
        rej = state.get("rejects", 0)
        eps_g = args.epsilon * (1.0 + 0.5 * min(rej, 4))     # 0.08 -> 0.24
        skip_g = min(0.5, args.skip_explore * (1.0 + 0.5 * min(rej, 4)))
        eval_g = args.eval_runs * (2 if rej >= 4 else 1)
        if rej:
            log(f"  stagnation: {rej} consecutive reject(s) -> epsilon "
                f"{eps_g:.2f}, skip_explore {skip_g:.2f}, eval_runs {eval_g}")

        # 1. COLLECT -- champion drives; sabotage modes for coverage
        c = [PY, "collect.py", "--runs", str(args.runs_per_gen),
             "--agent", "v2", "--epsilon", f"{eps_g:.3f}",
             "--mix", str(args.mix), "--burst", str(args.burst),
             "--skip_explore", f"{skip_g:.3f}",
             "--workers", str(args.workers), "--out", dat,
             "--checkpoint_every", "500"]
        if champ:
            c += ["--model", champ]
        per_run_budget = 30                       # generous for a pi core
        phase("collect", c, lgf, done_path=dat,
              timeout=args.runs_per_gen * per_run_budget
              // max(1, args.workers) + 3600)

        # 2. HARVEST winning runs into the hall of fame
        hof_marker = f"data/.hof_gen{g}.done"
        if not os.path.exists(hof_marker):
            n = harvest_wins(dat, "data/hof.npz")
            open(hof_marker, "w").write(str(n))
            log(f"  harvest: {n} winning runs -> data/hof.npz")

        # 3. TRAIN candidate on window + hall of fame; TD off the champion
        lo = 1 if args.window <= 0 else max(1, g - args.window + 1)
        pats = [f"data/gen{k}.npz" for k in range(lo, g + 1)]
        pats = [p for p in pats if os.path.exists(p)]   # gens seeded
        # mid-history may not exist; never hand the trainer ghosts.
        if os.path.exists("data/hof.npz"):
            pats.append("data/hof.npz")
        t = [PY, "train_value.py", "--data", *pats, "--out", cand]
        if champ:
            t += ["--td", champ]
        phase("train", t, lgf, done_path=cand, timeout=6 * 3600)

        # 4. GATE: candidate vs champion (or vs raw search if no champion)
        e = [PY, "batch.py", "--runs", str(eval_g),
             "--workers", str(args.workers), "--agent", "v2",
             "--model", cand, "--out", evj]
        e += (["--compare", "v2", "--compare_model", champ] if champ
              else ["--compare", "v2raw"])
        phase("eval", e, lgf, done_path=evj,
              timeout=eval_g * 2 * 60 // max(1, args.workers) + 3600)

        # 5. DECIDE
        promote, detail = paired_verdict(evj, z=args.gate_z)
        log(f"  gate: {detail}")
        if promote:
            state["champion"] = cand
            state["rejects"] = 0
        else:
            state["rejects"] = state.get("rejects", 0) + 1

        # 6. BOOKKEEP + PRUNE (never touch hof or models; npz only)
        try:
            runs = json.load(open(dat.replace(".npz", "_runs.json")))
            wins = sum(r["won"] for r in runs)
            mean_ante = sum(r["ante"] for r in runs) / len(runs)
        except Exception:
            wins, mean_ante = -1, -1.0
        new_hist = not os.path.exists("history.csv")
        with open("history.csv", "a", newline="") as f:
            w = csv.writer(f)
            if new_hist:
                w.writerow(["gen", "runs_wins", "mean_ante",
                            "promoted", "champion", "gate"])
            w.writerow([g, wins, f"{mean_ante:.3f}", promote,
                        state["champion"], detail])
        # DATA IS NEVER PRUNED: every generation stays on disk and in
        # training. The streaming trainer's RAM is bounded by the largest
        # single file, so there is no memory reason to forget, and the
        # coverage argument (the net must keep seeing what bad play looks
        # like long after the policy stops producing it) says don't.
        # Only phase LOGS are disposable: keep the last 6.
        for k in range(1, g - 6):
            lg = f"logs/gen{k}.log"
            if os.path.exists(lg):
                os.remove(lg)
        state["gen"] = g
        save_json_atomic("loop_state.json", state)
        save_json_atomic("status.json",
                         {"gen": g, "champion": state["champion"],
                          "last_gate": detail, "wins_this_gen": wins,
                          "consecutive_rejects": state["rejects"],
                          "mean_ante": mean_ante,
                          "updated": time.strftime("%Y-%m-%d %H:%M:%S")})
        log(f"=== generation {g} done ===")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        log("interrupted; state saved after last completed generation")
