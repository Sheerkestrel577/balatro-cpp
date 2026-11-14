"""
probe.py — does the policy actually SEE pairs?

The question: when one king is already selected, does the OTHER king's
toggle score go up relative to the junk cards? If yes, the "match" circuit
exists and the problem is elsewhere. If no, the network is blind to pairs
and no amount of evolution time will help until that's fixed.

Method: build a synthetic hand (two kings + junk), hold everything else
constant, and score the toggles twice -- once with nothing selected, once
with one king selected. Only the selection changes, so any difference in
the scores is caused by the selection and nothing else.

  py probe.py                     # best.npy
  py probe.py --genome ckpt_old.npy
"""
import argparse

import numpy as np

from evo_agent import SPEC, act
from naneinf import Balatro, Action

ap = argparse.ArgumentParser()
ap.add_argument("--genome", default="best.npy")
ap.add_argument("--trials", type=int, default=8)
a = ap.parse_args()

params = SPEC.unpack(np.load(a.genome))

RANKS = {13: "K", 12: "Q", 11: "J", 10: "10", 9: "9", 7: "7", 5: "5",
         4: "4", 3: "3", 2: "2"}
SUITS = "SHDC"


def card(rank, suit, selected=False):
    return {"rank": rank, "suit": suit, "enhancement": 0, "edition": 0,
            "seal": 0, "selected": selected, "face_down": False,
            "debuffed": False, "index": 0,
            "name": f"{RANKS.get(rank, rank)}{SUITS[suit]}"}


# two kings (a pair) + one suit-mate of the first king + junk
def make_hand(sel_idx=None):
    h = [card(13, 0),      # 0: KS   <- the "already selected" one
         card(13, 1),      # 1: KH   <- rank match (pair)
         card(7, 0),       # 2: 7S   <- suit match (flush-ish)
         card(4, 2),       # 3: 4D   junk
         card(9, 3),       # 4: 9C   junk
         card(2, 2),       # 5: 2D   junk
         card(5, 3),       # 6: 5C   junk
         card(12, 2)]      # 7: QD   junk (high card)
    if sel_idx is not None:
        h[sel_idx]["selected"] = True
    return h


def toggle_scores(g, hand):
    """act() reads the hand through g.hand(); shadow it with ours."""
    g.hand = lambda: hand
    s = act(params, g)
    return s[Action.TOGGLE_CARD:Action.TOGGLE_CARD + len(hand)].copy()


deltas_pair, deltas_suit, deltas_junk = [], [], []
print(f"{a.genome}: probing pair-awareness over {a.trials} game contexts\n")

for t in range(a.trials):
    g = Balatro()
    g.step(Action.SELECT_BLIND)
    before = toggle_scores(g, make_hand(None))
    after = toggle_scores(g, make_hand(0))     # KS now selected
    d = after - before
    deltas_pair.append(d[1])                   # KH: rank match
    deltas_suit.append(d[2])                   # 7S: suit match
    deltas_junk.append(np.mean([d[3], d[4], d[5], d[6], d[7]]))
    if t == 0:
        names = [c["name"] for c in make_hand(None)]
        print("  one context, scores with nothing selected -> KS selected:")
        for i, nm in enumerate(names):
            tag = ("  <- SELECTED" if i == 0 else
                   "  <- pair partner" if i == 1 else
                   "  <- suit partner" if i == 2 else "")
            print(f"    {nm:>4}  {before[i]:+.3f} -> {after[i]:+.3f}"
                  f"  ({d[i]:+.3f}){tag}")
        print()
    g.close()

p, s, j = (np.mean(deltas_pair), np.mean(deltas_suit), np.mean(deltas_junk))
print(f"  mean change when a king gets selected:")
print(f"    other king (pair)   {p:+.4f}")
print(f"    same suit (flush)   {s:+.4f}")
print(f"    junk cards          {j:+.4f}")
print()
edge = p - j
print(f"  PAIR EDGE (king minus junk): {edge:+.4f}")
if edge > 0.05:
    print("  -> circuit EXISTS. Selecting a king makes the other king more"
          "\n     attractive. The wall is downstream (hand size, PLAY timing,"
          "\n     scoring magnitude), not pair-blindness.")
elif edge < -0.05:
    print("  -> circuit is INVERTED. Selecting a king makes the other king"
          "\n     LESS attractive. Evolution learned an anti-correlation --"
          "\n     a fitness problem, not a capacity problem.")
else:
    print("  -> NO circuit. The match features aren't reaching the head."
          "\n     Architecture fix needed (wider head, or feed match feats"
          "\n     into the trunk too) -- more generations won't help.")
