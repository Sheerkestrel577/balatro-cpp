"""
eval.py — fixed-panel benchmark for a saved genome.

The `smalls`/`bigs` columns in evolve.py move with PANEL DIFFICULTY as
much as with policy quality, so gen-to-gen comparison is noise. This
runs a FIXED seed set (same seeds every invocation, forever), so two
numbers taken a week apart are actually comparable.

  py eval.py                          # best.npy, 20 seeds, es 0.3
  py eval.py --n 50                   # tighter error bars
  py eval.py --es 1.0                 # A/B the anti-fidget setting
  py eval.py --genome old_best.npy    # compare two champions
"""
import argparse

import numpy as np

from evo_agent import rollout

# frozen benchmark seeds -- never change these, that's the whole point
CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789"
_r = np.random.default_rng(20260811)
BENCH = ["".join(CHARS[j] for j in _r.integers(0, len(CHARS), 8))
         for _ in range(200)]

ap = argparse.ArgumentParser()
ap.add_argument("--genome", default="best.npy")
ap.add_argument("--n", type=int, default=20)
ap.add_argument("--es", type=float, default=0.3)
ap.add_argument("--temp", type=float, default=0.5)
a = ap.parse_args()

genome = np.load(a.genome)
seeds = BENCH[:a.n]
fits = np.array([rollout(genome, s, es=a.es, temp=a.temp) for s in seeds])

# fitness 3.0 = nothing cleared; each cleared blind is worth ~1.0
blinds = np.clip(np.floor(fits - 3.0), 0, None).astype(int)
se = fits.std(ddof=1) / np.sqrt(len(fits)) if len(fits) > 1 else 0.0

print(f"{a.genome}  n={a.n}  es={a.es} temp={a.temp}")
print(f"  fitness      {fits.mean():.2f} +/- {se:.2f} (sd {fits.std():.2f})")
print(f"  best / worst {fits.max():.2f} / {fits.min():.2f}")
print(f"  cleared 1+   {(blinds >= 1).mean()*100:5.1f}%   (small blind)")
print(f"  cleared 2+   {(blinds >= 2).mean()*100:5.1f}%   (big blind)")
print(f"  cleared 3+   {(blinds >= 3).mean()*100:5.1f}%   (ante 1 boss)")
print(f"  cleared 4+   {(blinds >= 4).mean()*100:5.1f}%   (into ante 2)")
