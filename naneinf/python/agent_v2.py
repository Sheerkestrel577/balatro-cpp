"""agent_v2.py — shop decisions by HONEST ROLLOUT instead of opinion.

For every affordable shop option (each joker/consumable slot, each pack,
reroll, and "buy nothing"), the agent:
  1. imagine()s the game: exact clone + reshuffled draws + reseeded RNG,
     so the copy's future is a fair sample, not the run's real future;
  2. applies the option in the copy;
  3. fast-forwards ~2 blinds using the v0 policy;
  4. scores where the copy ended up (progress, survival, money, kings).
Averaged over k samples, best option wins. The joker dict is not
consulted for shop decisions; the value net is optional garnish on the
terminal score (it knows survival shapes, which sharpens comparisons).

    python3 agent_v2.py --runs 40                 # rollouts only
    python3 agent_v2.py --runs 40 --model models/v_gen0.npz
"""
from __future__ import annotations
import argparse, zlib, os, sys, time
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import naneinf
from agent_v0 import (Agent, Game, A, PH_BLIND, PH_PLAY, PH_CASH, PH_SHOP,
                      PH_PACK, money_floor)

V_LOADED = None


def stable_seed(*parts):
    """Deterministic across processes -- Python's hash() on strings is
    randomized per interpreter (PYTHONHASHSEED), which made agent runs
    unreproducible: same seed, different discard rollouts, different
    outcome. crc32 of the repr is stable everywhere."""
    return zlib.crc32(repr(parts).encode()) & 0xFFFFFFFF


def _load_model(path):
    global V_LOADED
    if path and V_LOADED is None:
        from train_value import MLP
        V_LOADED = MLP.load(path)
    return V_LOADED


def _pilot(game_obj, rng_seed):
    """A throwaway v0 brain steering an imagined game."""
    import random as _r
    p = Agent.__new__(Agent)
    p.G = game_obj
    p.say = lambda *a: None
    p.rng = _r.Random(rng_seed)
    p._init_fields()          # ALL runtime fields, single source of truth
    return p


def rollout(game_obj, rng_seed, max_blinds=2, max_steps=400):
    """Fast-forward an imagined game ~max_blinds blinds with v0 policy.
    Returns the terminal score of where it ended up."""
    p = _pilot(game_obj, rng_seed)
    g = game_obj.g
    blinds_done = 0
    margin_sum = 0.0            # doublings of overkill at each blind clear
    pos0 = (g.ante, g.blind)
    steps0 = len(game_obj.log)
    last = -1; stuck = 0
    while not g.done and not p.dead and blinds_done < max_blinds:
        if len(game_obj.log) - steps0 > max_steps: break
        if len(game_obj.log) == last:
            stuck += 1
            if stuck > 4: p.random_legal()
        else:
            stuck = 0
        last = len(game_obj.log)
        ph = g.phase
        if   ph == PH_BLIND: game_obj.do(A.SELECT_BLIND)
        elif ph == PH_PLAY:  p.playing()
        elif ph == PH_CASH:
            blinds_done += 1
            # scoring POWER, not just survival: how hard did we beat it?
            # This is what a good joker changes even when both variants
            # clear -- and it is visible inside a 2-blind horizon.
            margin_sum += max(0.0, min(g.score_log2 - g.goal_log2, 6.0))
            p.cash_out()
        elif ph == PH_SHOP:  p.shop()          # dict policy INSIDE rollouts
        elif ph == PH_PACK:  p.pack()
        else: break
    # ---- terminal score: interpretable, comparison-only ----
    pos = g.ante * 3 + g.blind                 # absolute board position
    dead = 1.0 if (g.phase == 5 or p.dead) else 0.0
    try:
        K, N = g.deck_kings, max(1, g.deck_size)
    except Exception:
        K, N = 4, max(1, g.deck_size)
    # THE OBJECTIVE, at a volume decisions can hear. Phi = K*(100/N)^2 is
    # the stated fitness (52-card 4-king deck: ~14.8; pruned king deck:
    # 100+). lam ramps with ante: survive first, then the objective takes
    # over. At lam=0.10, one junk card added at N=52 costs ~0.06 -- the
    # same order as a rollout margin difference, so Certificate's card
    # feed, pack-card picks, and Immolate pruning all become AUDIBLE.
    phi = K * (100.0 / N) ** 2
    lam = 0.02 + 0.015 * min(g.ante, 8)          # 0.035 .. 0.14
    score = ((pos - (pos0[0] * 3 + pos0[1]))     # blinds actually advanced
             + 0.8 * margin_sum                  # power: overkill per clear
             - dead * 6.0
             + 0.02 * min(g.money, 30)           # money is instrumental
             + lam * phi)
    if V_LOADED is not None:
        from train_value import predict
        net, mu, sd = V_LOADED
        try:
            from collect import extra_features
            x = np.concatenate([g.obs(), np.array(extra_features(g),
                                                  np.float32)])
            v = predict(net, mu, sd, x[None, :])[0]
            score += 3.0 * float(v[0])           # prog head as the tail est.
        except Exception:
            pass
    return score


class RolloutAgent(Agent):
    def __init__(self, seed=None, k=3, epsilon=0.05, model=None):
        super().__init__(seed)
        self.k = k
        self.epsilon = epsilon
        _load_model(model)

    def _eval_option(self, actions):
        """Mean rollout score after applying `actions` in imagination."""
        tot = 0.0
        for i in range(self.k):
            # common random numbers: sample index i maps to the SAME
            # imagined future for every option, so options are compared
            # under identical luck and their difference is the decision.
            im = self.G.imagine(stable_seed(self.G.seed,
                                      len(self.G.log), i))
            ok = True
            for a in actions:
                if not im.do(a): ok = False; break
            if not ok:
                return None
            if im.g.phase == PH_PACK:            # a pack opened: pick greedily
                _pilot(im, i).pack()
            tot += rollout(im, 1000 + i)
        return tot / self.k

    def blind_select(self):
        """MEASURE play-vs-skip instead of always playing.

        Structurally identical to the shop decision: two legal options,
        each applied in an imagined clone under common random numbers,
        each scored by rollout. Skipping trades a blind's winnings (and
        the shop that follows it) for the skip tag -- which is how the
        real game hands out the consumables that make red seals, so the
        trade is genuinely open rather than obviously bad.

        The boss blind is never skippable; there the choice is empty and
        we just play it.
        """
        g, G = self.G.g, self.G
        if not g.legal_mask()[A.SKIP_BLIND]:
            G.do(A.SELECT_BLIND)
            return

        # forced exploration (collection only): take the skip regardless
        if self.skip_explore and self.rng.random() < self.skip_explore:
            if G.do(A.SKIP_BLIND):
                return

        options = [("play", [A.SELECT_BLIND]), ("skip", [A.SKIP_BLIND])]
        best_acts, best_v = [A.SELECT_BLIND], None
        for _name, acts in options:
            v = self._eval_option(acts)
            if v is None:
                continue
            if best_v is None or v > best_v + 1e-9:
                best_acts, best_v = acts, v
        if self.epsilon and self.rng.random() < self.epsilon:
            best_acts = self.rng.choice(options)[1]
        if not all(G.do(a) for a in best_acts):
            G.do(A.SELECT_BLIND)          # fallback: never stall the phase

    def shop(self):
        g, G = self.G.g, self.G
        floor = money_floor(g.ante)
        buys = 0
        while g.phase == PH_SHOP and buys < 3:
            options = [("stop", [])]
            held = g.jokers()
            slots_full = len(held) >= 5
            for si, it in enumerate(g.shop()):
                if it["sold"]: continue
                if it["kind"] == 0 and slots_full:
                    # can't buy outright: consider selling to make room.
                    # cheapest sells first -- least sunk value at risk.
                    order = sorted(range(len(held)),
                                   key=lambda j: held[j]["sell_value"])[:3]
                    for j in order:
                        if g.money + held[j]["sell_value"] < it["cost"]:
                            continue
                        options.append(
                            (f"sell {held[j]['name']} -> {it['name']}",
                             [A.TOGGLE_JOKER + j, A.SELL_JOKER,
                              A.BUY_SLOT + si]))
                    continue
                if g.money < it["cost"]: continue
                options.append((f"buy {it['name']}", [A.BUY_SLOT + si]))
            for pi, p in enumerate(g.packs()):
                if g.money - p["cost"] >= 0:
                    options.append((f"pack {p['name']}", [A.BUY_PACK + pi]))
            if g.money - g.reroll_cost >= floor + 5:
                options.append(("reroll", [A.REROLL_SHOP]))
            if len(options) == 1:
                break
            best_name, best_acts, best_v = "stop", [], None
            for name, acts in options:
                v = self._eval_option(acts) if acts else \
                    self._eval_option([])            # baseline: do nothing
                if v is None: continue
                # money kept below floor is only ok if the option earns it
                if acts and acts[0] != A.REROLL_SHOP:
                    pass
                if best_v is None or v > best_v + 1e-9:
                    best_name, best_acts, best_v = name, acts, v
            if self.epsilon and self.rng.random() < self.epsilon:
                cand = [o for o in options if o[1]]
                if cand:
                    best_name, best_acts = self.rng.choice(cand)
            if not best_acts:
                break
            ok = all(G.do(a) for a in best_acts)
            if not ok:
                break
            buys += 1
            if g.phase == PH_PACK:
                self.pack()                     # real pack: v0 pick logic
        G.do(A.LEAVE_SHOP)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", type=int, default=20)
    ap.add_argument("--k", type=int, default=3)
    ap.add_argument("--epsilon", type=float, default=0.05)
    ap.add_argument("--model", default=None,
                    help="optional value net used in rollout terminal score")
    ap.add_argument("--seed", default=None)
    args = ap.parse_args()
    res = []
    t0 = time.time()
    for i in range(args.runs):
        a = RolloutAgent(args.seed, k=args.k, epsilon=args.epsilon,
                         model=args.model)
        r = a.run(max_secs=240)
        res.append(r)
        antes = [x["ante"] for x in res]
        w = sum(x["won"] for x in res)
        print(f"\r[{i+1}/{args.runs}] ante avg {sum(antes)/len(antes):.2f} "
              f"max {max(antes)} wins {w} | "
              f"{(time.time()-t0)/len(res):.1f}s/run   ",
              end="", flush=True)
    print()
    if res:
        antes = [x["ante"] for x in res]
        print(f"mean ante {np.mean(antes):.2f}  max {max(antes)}  "
              f"wins {sum(r['won'] for r in res)}/{len(res)}")
        for r in sorted(res, key=lambda r: -r["ante"])[:3]:
            print("  ", {k: r[k] for k in
                         ("seed", "ante", "won", "money", "kings", "deck",
                          "jokers")})


if __name__ == "__main__":
    main()
