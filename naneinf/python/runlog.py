"""
runlog — run logging + replay for naneinf.

The engine is deterministic given (seed, action sequence), so a run log only
NEEDS the seed and the accepted actions.  Everything else in the log is
convenience for humans reading it.  `replay()` re-simulates a log and yields
(obs, legal_mask, action) triples — exactly the (state, action) pairs you
want for behaviour cloning — without the log files ever storing 222-float
observations.

Log format: one JSON object per line (JSONL).

    {"type": "meta", "seed": "QJSKRTWR", "started": "...", "engine": "naneinf-w4"}
    {"type": "step", "i": 0, "phase": "blind_select", "action": 40,
     "name": "select_blind", "ante": 1, "blind": 0, "money": 4,
     "score_log2": 0.0}                       # state is BEFORE the action
    {"type": "note", "text": "ante 1 small blind beaten"}   # human breadcrumbs
    {"type": "end", "won": false, "ante": 3, "steps": 412, "seconds": 1043.2}

Usage (training side):

    from runlog import replay, load_actions, export_npz
    for obs, mask, action in replay("runs/run_....jsonl"):
        ...
    export_npz("runs/run_....jsonl", "run.npz")   # obs/mask/action arrays

CLI:
    python3 runlog.py summarize runs/            # table of all runs
    python3 runlog.py export runs/x.jsonl x.npz  # training arrays
    python3 runlog.py verify runs/x.jsonl        # re-simulate, check legality
    python3 runlog.py clear runs/                # delete every run log
    python3 runlog.py clear runs/ --stale-only   # only ones from another build
    python3 runlog.py clear runs/ --keep-complete --dry-run
"""
from __future__ import annotations

import json
import os
import sys
import time
from typing import Iterator, List, Optional, Tuple

from naneinf import Balatro, PHASE_NAMES, action_name

try:
    import numpy as np
except ImportError:
    np = None


# --------------------------------------------------------------------------
# engine fingerprint
# --------------------------------------------------------------------------
def engine_build() -> str:
    """Short hash of the shared library. Logs record it so a dataset made on
    one engine build is recognisably distinct from another — change the
    engine and old logs may no longer replay identically."""
    import hashlib
    import naneinf as _nf
    try:
        with open(_nf._LIBPATH, "rb") as f:
            return hashlib.sha256(f.read()).hexdigest()[:12]
    except Exception:
        return "unknown"


# --------------------------------------------------------------------------
# writing
# --------------------------------------------------------------------------
class RunLogger:
    """Appends one JSONL file per run. Every write is flushed, so a crashed
    or force-quit run still leaves a usable (if end-less) log."""

    def __init__(self, seed: str, dirpath: str = "runs",
                 log_obs: bool = False, tag: str = ""):
        os.makedirs(dirpath, exist_ok=True)
        stamp = time.strftime("%Y%m%d_%H%M%S")
        name = f"run_{stamp}_{seed}{('_' + tag) if tag else ''}.jsonl"
        self.path = os.path.join(dirpath, name)
        self.log_obs = log_obs
        self.steps = 0
        self._t0 = time.time()
        self._f = open(self.path, "w", encoding="utf-8")
        self._write({"type": "meta", "seed": seed,
                     "started": time.strftime("%Y-%m-%d %H:%M:%S"),
                     "engine": "naneinf-w4", "engine_build": engine_build(),
                     "log_obs": log_obs})

    def _write(self, obj: dict):
        self._f.write(json.dumps(obj, separators=(",", ":")) + "\n")
        self._f.flush()

    def step(self, g: Balatro, action: int,
             obs_before: Optional[list] = None):
        """Record an ACCEPTED action. All state fields describe the moment
        BEFORE the action was applied (pass obs_before captured then)."""
        rec = {"type": "step", "i": self.steps, "action": int(action),
               "name": action_name(action),
               "phase": PHASE_NAMES[g.phase], "ante": g.ante,
               "blind": g.blind, "money": g.money,
               "hands": g.hands_left, "discards": g.discards_left,
               "score_log2": round(g.score_log2, 4)}
        # NOTE: caller invokes this BEFORE g.step(action); see gui.py.
        if self.log_obs and obs_before is not None:
            rec["obs"] = [round(float(x), 5) for x in obs_before]
        self._write(rec)
        self.steps += 1

    def note(self, text: str, **extra):
        self._write({"type": "note", "text": text, **extra})

    def end(self, g: Balatro):
        self._write({"type": "end", "won": g.won, "ante": g.ante,
                     "naneinf_ante": g.naneinf_ante, "steps": self.steps,
                     "seconds": round(time.time() - self._t0, 1)})
        self._f.close()

    def abandon(self):
        """Close without an end record (mid-run quit)."""
        try:
            self._f.close()
        except Exception:
            pass


# --------------------------------------------------------------------------
# reading / replay
# --------------------------------------------------------------------------
def load(path: str) -> Tuple[dict, List[dict]]:
    """-> (meta, records). Tolerates truncated final lines."""
    meta, recs = {}, []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                obj = json.loads(line)
            except json.JSONDecodeError:
                break                       # crashed mid-write; keep the rest
            if obj.get("type") == "meta":
                meta = obj
            recs.append(obj)
    return meta, recs


def load_actions(path: str) -> Tuple[str, List[int]]:
    meta, recs = load(path)
    return meta.get("seed", ""), \
        [r["action"] for r in recs if r.get("type") == "step"]


def replay(path: str) -> Iterator[tuple]:
    """Re-simulate a log. Yields (obs, legal_mask, action) per step, with
    obs/mask captured BEFORE the action — ready for behaviour cloning.
    Raises if a logged action is illegal on replay (engine/log mismatch)."""
    seed, actions = load_actions(path)
    if not seed:
        raise ValueError(f"{path}: no meta/seed record")
    g = Balatro(seed)
    for i, a in enumerate(actions):
        obs, mask = g.obs(), g.legal_mask()
        if not g.step(a):
            raise RuntimeError(
                f"{path}: step {i} action {a} ({action_name(a)}) illegal on "
                "replay — log was made with a different engine build?")
        yield obs, mask, a
    # final-state sanity note available via g if the caller wants it
    replay.final = g          # stash for verify()


def export_npz(log_path: str, out_path: str):
    if np is None:
        raise RuntimeError("numpy required for export_npz")
    O, M, A = [], [], []
    for obs, mask, a in replay(log_path):
        O.append(obs); M.append(mask); A.append(a)
    np.savez_compressed(out_path,
                        obs=np.asarray(O, dtype=np.float32),
                        legal=np.asarray(M, dtype=bool),
                        action=np.asarray(A, dtype=np.int64))
    return len(A)


def summarize(dirpath: str) -> List[dict]:
    rows = []
    cur = engine_build()
    for fn in sorted(os.listdir(dirpath)):
        if not fn.endswith(".jsonl"):
            continue
        meta, recs = load(os.path.join(dirpath, fn))
        end = next((r for r in reversed(recs) if r.get("type") == "end"), None)
        steps = sum(1 for r in recs if r.get("type") == "step")
        build = meta.get("engine_build", "pre-fingerprint")
        rows.append({"file": fn, "seed": meta.get("seed", "?"),
                     "steps": steps,
                     "won": end["won"] if end else None,
                     "ante": end["ante"] if end else "?",
                     "complete": end is not None,
                     "build": build, "stale": build != cur})
    return rows


# --------------------------------------------------------------------------
# housekeeping
# --------------------------------------------------------------------------
def clear(dirpath: str, keep_complete: bool = False,
          stale_only: bool = False, dry_run: bool = False) -> List[str]:
    """Delete run logs. Returns the filenames removed (or that would be).

    keep_complete — keep runs that reached an 'end' record
    stale_only    — only delete runs recorded on a different engine build
    """
    cur = engine_build()
    removed = []
    for fn in sorted(os.listdir(dirpath)):
        if not fn.endswith(".jsonl"):
            continue
        path = os.path.join(dirpath, fn)
        meta, recs = load(path)
        complete = any(r.get("type") == "end" for r in recs)
        stale = meta.get("engine_build", "pre-fingerprint") != cur
        if keep_complete and complete:
            continue
        if stale_only and not stale:
            continue
        removed.append(fn)
        if not dry_run:
            os.remove(path)
    return removed


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------
def _main(argv):
    if len(argv) >= 2 and argv[0] == "summarize":
        rows = summarize(argv[1])
        print(f"current engine build: {engine_build()}\n")
        print(f"{'file':46} {'seed':10} {'steps':>6} {'won':>4} "
              f"{'ante':>4}  {'build':14}")
        for r in rows:
            won = {True: "yes", False: "no", None: "?"}[r["won"]]
            flags = ("" if r["complete"] else " incomplete") \
                + (" STALE" if r["stale"] else "")
            print(f"{r['file']:46} {r['seed']:10} {r['steps']:>6} "
                  f"{won:>4} {str(r['ante']):>4}  {r['build']:14}{flags}")
        stale = sum(1 for r in rows if r["stale"])
        if stale:
            print(f"\n{stale} run(s) recorded on a different engine build — "
                  "'verify' them before training, or "
                  f"'python3 runlog.py clear {argv[1]} --stale-only'")
        return 0
    if argv and argv[0] == "clear":
        d = argv[1] if len(argv) > 1 else "runs"
        opts = set(argv[2:])
        dry = "--dry-run" in opts
        gone = clear(d, keep_complete="--keep-complete" in opts,
                     stale_only="--stale-only" in opts, dry_run=dry)
        verb = "would delete" if dry else "deleted"
        print(f"{verb} {len(gone)} run(s) from {d}/")
        for fn in gone:
            print("  " + fn)
        if not gone:
            print("  (nothing matched)")
        return 0
    if len(argv) == 3 and argv[0] == "export":
        n = export_npz(argv[1], argv[2])
        print(f"wrote {argv[2]}: {n} (obs, legal, action) rows")
        return 0
    if len(argv) == 2 and argv[0] == "verify":
        n = sum(1 for _ in replay(argv[1]))
        g = replay.final
        print(f"OK: {n} steps replayed, final: ante {g.ante}, "
              f"won={g.won}, done={g.done}")
        return 0
    print(__doc__)
    return 1


if __name__ == "__main__":
    sys.exit(_main(sys.argv[1:]))
