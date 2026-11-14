"""agent_v1.py — v0's exact play search, but shop decisions come from V.

The hand-typed joker dict is NOT consulted here. For each shop option the
agent forks the game, applies the purchase, and asks the value net how good
the resulting state is. Whatever V learned from outcome statistics decides.

    python3 agent_v1.py models/v_gen0.npz            # one run
    python3 agent_v1.py models/v_gen0.npz --runs 40  # batch, vs v0 baseline

NOTE ON HONESTY: forking currently replays the action log, which copies the
RNG stream -- so a fork can peek at the true next shop roll. This agent only
forks over its OWN purchase decisions (deterministic given the shop, which is
already visible), so nothing hidden is previewed. Do NOT add draw/reroll
rollouts until nf_clone + nf_reshuffle_draws land; see ENGINE_REQUESTS.md.
"""
from __future__ import annotations
import argparse, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


from agent_v0 import Agent, Game, A, PH_SHOP, PH_PACK, money_floor
from collect import extra_features
from train_value import MLP, predict

# weights over V's heads when scoring a candidate state. Progress dominates
# while wins are rare; shift toward 'fit' once the win rate is nonzero.
SCORE_W = np.array([1.0, 1.0, 1.2, 0.15], dtype=np.float32)


class ValueAgent(Agent):
    def __init__(self, model_path, seed=None, epsilon=0.05):
        super().__init__(seed)
        self.net, self.mu, self.sd = MLP.load(model_path)
        self.epsilon = epsilon

    def value_of(self, game_obj, kings):
        g = game_obj.g
        x = np.concatenate([g.obs(),
                            np.array(extra_features(g), np.float32)])
        return float(predict(self.net, self.mu, self.sd,
                             x[None, :])[0] @ SCORE_W)

    # ---- V-guided shop: fork, buy, evaluate, keep the best ----
    def shop(self):
        g, G = self.G.g, self.G
        floor = money_floor(g.ante)
        acted = True
        rerolls = 0
        while acted and g.phase == PH_SHOP:
            acted = False
            base_v = self.value_of(G, self.kings)
            best_a, best_v = None, base_v
            for si, it in enumerate(g.shop()):
                if it["sold"] or g.money - it["cost"] < 0:
                    continue
                if g.money - it["cost"] < floor and it["kind"] == 1:
                    continue                       # keep interest for tarots
                f = G.fork()
                if not f.do(A.BUY_SLOT + si):
                    continue
                v = self.value_of(f, self.kings)
                if v > best_v:
                    best_a, best_v = A.BUY_SLOT + si, v
            for pi, p in enumerate(g.packs()):
                if g.money - p["cost"] < floor:
                    continue
                f = G.fork()
                if not f.do(A.BUY_PACK + pi):
                    continue
                v = self.value_of(f, self.kings)
                if v > best_v:
                    best_a, best_v = A.BUY_PACK + pi, v
            if self.epsilon and self.rng.random() < self.epsilon:
                opts = [A.BUY_SLOT + si for si, it in enumerate(g.shop())
                        if not it["sold"] and g.money >= it["cost"]]
                if opts:
                    best_a = self.rng.choice(opts)
            if best_a is not None and G.do(best_a):
                acted = True
                if g.phase == PH_PACK:
                    self.pack()
                continue
            if rerolls < 2 and g.money - g.reroll_cost >= floor + 8:
                if G.do(A.REROLL_SHOP):
                    rerolls += 1; acted = True
        G.do(A.LEAVE_SHOP)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("model")
    ap.add_argument("--runs", type=int, default=1)
    ap.add_argument("--epsilon", type=float, default=0.05)
    args = ap.parse_args()
    res = []
    for i in range(args.runs):
        a = ValueAgent(args.model, epsilon=args.epsilon)
        r = a.run(max_secs=180)
        res.append(r)
        print(r)
    if args.runs > 1:
        antes = [r["ante"] for r in res]
        print(f"\nmean ante {np.mean(antes):.2f}  max {max(antes)}  "
              f"wins {sum(r['won'] for r in res)}/{len(res)}")


if __name__ == "__main__":
    main()
