#!/usr/bin/env python3
"""gate_replay.py -- re-decide every past gate under a different rule.

`logs/eval_gen*.json` stores per-seed `ante` and `won` for BOTH sides of
every comparison ever run, and autoloop's log cleanup only removes
`logs/gen{k}.log` -- so the full gate history survives and every promotion
decision the loop ever made can be replayed offline, for free, without
spending a single run.

Use this before adopting a new promotion rule. It answers the only
question that matters: which past verdicts would have flipped, and were
those flips right?

    python3 gate_replay.py                    # old vs new, all gens
    python3 gate_replay.py --z 1.0            # require ante > 1 s.e.
    python3 gate_replay.py --glob 'logs/eval_gen*.json'

Reads nothing but the eval jsons. Changes nothing.
"""
from __future__ import annotations
import argparse, glob, json, os, re, sys

try:
    sys.stdout.reconfigure(line_buffering=True)
except Exception:
    pass


def load(path):
    """-> (label_a, label_b, common_seeds, per-seed dicts) or None."""
    try:
        d = json.load(open(path))
    except Exception as e:
        print(f"  !! {path}: unreadable ({e})")
        return None
    labels = list(d.keys())
    if len(labels) < 2:
        return None                        # single-config run, no gate
    a, b = d[labels[0]], d[labels[1]]      # a = candidate, b = incumbent
    da = {r["seed"]: r for r in a}
    db = {r["seed"]: r for r in b}
    common = [s for s in da if s in db]
    if not common:
        return None
    return labels[0], labels[1], common, da, db


def stats(common, da, db):
    n = len(common)
    wa = sum(int(da[s]["won"]) for s in common)
    wb = sum(int(db[s]["won"]) for s in common)
    diffs = [da[s]["ante"] - db[s]["ante"] for s in common]
    ad = sum(diffs) / n
    if n > 1:
        var = sum((x - ad) ** 2 for x in diffs) / n
        se = (var ** 0.5) / (n ** 0.5)
    else:
        se = 0.0
    return n, wa, wb, wa - wb, ad, se


def rule_old(wd, ad, **_):
    return wd > 0 or (wd == 0 and ad > 0)


def rule_new(wd, ad, wa=0, wb=0, se=0.0, z=0.0):
    band = max(1.0, (wa + wb) ** 0.5)
    if wd > band:
        return True
    if wd < -band:
        return False
    return ad > z * se


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--glob", default="logs/eval_gen*.json")
    ap.add_argument("--z", type=float, default=0.0,
                    help="s.e. multiple the ante diff must clear inside the "
                         "win-noise band (0 = old tiebreak semantics)")
    args = ap.parse_args()

    files = sorted(glob.glob(args.glob),
                   key=lambda p: int(re.search(r"(\d+)", os.path.basename(p))
                                     .group(1))
                   if re.search(r"(\d+)", os.path.basename(p)) else 0)
    if not files:
        raise SystemExit(f"no eval jsons matched {args.glob!r} "
                         f"(run this from ~/naneinf/python)")

    print(f"{'gen':>4}  {'n':>4}  {'wins a/b':>9}  {'wd':>4}  {'band':>5}  "
          f"{'ante diff':>12}  {'old':>8}  {'new':>8}   flip")
    print("-" * 82)
    flips = []
    for f in files:
        g = re.search(r"(\d+)", os.path.basename(f))
        gen = g.group(1) if g else "?"
        got = load(f)
        if not got:
            print(f"{gen:>4}  (no paired comparison in {os.path.basename(f)})")
            continue
        la, lb, common, da, db = got
        n, wa, wb, wd, ad, se = stats(common, da, db)
        band = max(1.0, (wa + wb) ** 0.5)
        o = rule_old(wd, ad)
        nw = rule_new(wd, ad, wa=wa, wb=wb, se=se, z=args.z)
        flip = "" if o == nw else ("--> PROMOTE" if nw else "--> reject")
        if o != nw:
            flips.append((gen, o, nw, wd, ad, se))
        print(f"{gen:>4}  {n:>4}  {wa:>4}/{wb:<4}  {wd:>+4d}  {band:>5.1f}  "
              f"{ad:>+7.2f}+/-{se:<4.2f}  "
              f"{'PROMOTE' if o else 'reject':>8}  "
              f"{'PROMOTE' if nw else 'reject':>8}   {flip}")

    print()
    if not flips:
        print("No verdicts change under the new rule at "
              f"z={args.z}. Either the rule is a no-op on this history, or "
              "the gates were never close. Check the wd column: if it is "
              "mostly 0, the old tiebreak was already doing the deciding "
              "and the win-veto simply never fired.")
    else:
        print(f"{len(flips)} verdict(s) change at z={args.z}:")
        for gen, o, nw, wd, ad, se in flips:
            print(f"  gen {gen}: {'PROMOTE' if o else 'reject'} -> "
                  f"{'PROMOTE' if nw else 'reject'}  "
                  f"(win diff {wd:+d}, ante {ad:+.2f}+/-{se:.2f})")
        print()
        print("Judge each one by hand before adopting. A flip to PROMOTE is "
              "only an improvement if the ante gain is real -- compare it "
              "against its own s.e., not against zero.")

    print("\nNOTE: promotions compound. Replay assumes each gate ran with "
          "the champion it actually had; if an early verdict flips, every "
          "later comparison would have faced a different incumbent and "
          "these rows stop being what would truly have happened. Treat "
          "this as 'which decisions were close', not as a simulation.")


if __name__ == "__main__":
    main()
