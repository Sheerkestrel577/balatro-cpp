"""
best.py — replay the current champion (best.npy) and narrate what it does.

  py best.py                     # random seed, training temp (0.5)
  py best.py PCK2A3FJ            # specific seed
  py best.py PCK2A3FJ --temp 0   # greedy argmax (the honesty test:
                                 #   if this deadlocks, the policy still
                                 #   needs sampling noise to act)
  py best.py --max-ante 8        # watch a full run attempt

Prints every action; on PLAY/DISCARD also prints the selected cards, so
pairs/flushes are visible at a glance. Uses the SAME sampler seeding as
rollout() in evo_agent.py, so a replay of a training seed reproduces
what fitness actually saw.
"""
import argparse

import numpy as np

from evo_agent import SPEC, act
from naneinf import Balatro, Action, NUM_ACTIONS, action_name

ap = argparse.ArgumentParser()
ap.add_argument("seed", nargs="?", default=None)
ap.add_argument("--temp", type=float, default=0.5)
ap.add_argument("--max-ante", type=int, default=1)
ap.add_argument("--max-steps", type=int, default=2000)
ap.add_argument("--genome", default="best.npy")
a = ap.parse_args()

params = SPEC.unpack(np.load(a.genome))
g = Balatro(a.seed)
arng = np.random.default_rng(sum(ord(ch) for ch in g.seed) * 7919 + 1)

steps = 0
while not g.done and g.ante <= a.max_ante and steps < a.max_steps:
    steps += 1
    scores = act(params, g)
    scores[~g.legal_mask()] = -np.inf
    if not np.isfinite(scores.max()):
        print("(no legal actions)")
        break
    if a.temp > 0:
        z = np.exp((scores - scores.max()) / a.temp)
        act_i = int(arng.choice(NUM_ACTIONS, p=z / z.sum()))
    else:
        act_i = int(scores.argmax())

    if act_i in (Action.PLAY, Action.DISCARD):
        sel = " ".join(h["name"] for h in g.hand() if h["selected"])
        print(f"{action_name(act_i):<18} [{sel}]")
    else:
        print(action_name(act_i))
    g.step(act_i)

print()
print(g.render())
print(f"\nsteps {steps} | score 2^{g.score_log2:.2f} / goal 2^{g.goal_log2:.2f}"
      f" | done={g.done} won={g.won}")
