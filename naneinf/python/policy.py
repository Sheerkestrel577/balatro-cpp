"""
policy.py — the smallest thing that plays Balatro with a neural net.

Run it:   python policy.py

This is the whole "PyTorch part" of neuroevolution. Note what is NOT here:
no loss function, no optimizer, no .backward(), no training loop. Evolution
never computes a gradient — it mutates weights and keeps what scores well.
Torch is being used as a fast matrix container, nothing more.
"""
import sys

# point this at wherever you extracted the engine
sys.path.insert(0, r"C:\Users\s-sda\Downloads\naneinf-v24\naneinf\python")

import torch
import torch.nn as nn

from naneinf import Balatro, Action, NUM_ACTIONS, OBS_SIZE, action_name

torch.set_grad_enabled(False)          # we never train. this makes it faster.


# ---------------------------------------------------------------- the net
class Policy(nn.Module):
    """222 observations -> 58 action scores."""

    def __init__(self, hidden=128):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(OBS_SIZE, hidden), nn.Tanh(),
            nn.Linear(hidden, hidden),   nn.Tanh(),
            nn.Linear(hidden, NUM_ACTIONS),
        )

    def forward(self, obs):
        return self.net(obs)

    # --- the two methods evolution actually needs -----------------------
    def get_weights(self):
        """Flatten every parameter into one 1-D tensor (the genome)."""
        return torch.cat([p.reshape(-1) for p in self.parameters()])

    def set_weights(self, flat):
        """Write a flat genome back into the network."""
        i = 0
        for p in self.parameters():
            n = p.numel()
            p.copy_(flat[i:i + n].view_as(p))
            i += n


# ------------------------------------------------------- action selection
def choose(policy, game):
    """Score all 58 actions, mask the illegal ones, take the best."""
    obs = torch.from_numpy(game.obs())
    mask = torch.from_numpy(game.legal_mask())
    scores = policy(obs)
    scores[~mask] = float("-inf")      # illegal actions can never win
    return int(scores.argmax())


# ------------------------------------------------------------- the rollout
# Actions that change real game state. Anything else is just moving the
# cursor around, so a run doing only those is stuck.
PROGRESS = {
    Action.PLAY, Action.DISCARD, Action.USE_CONSUMABLE, Action.SELL_JOKER,
    Action.SELL_CONSUMABLE, Action.SELECT_BLIND, Action.SKIP_BLIND,
    Action.CONFIRM_CASHOUT, Action.LEAVE_SHOP, Action.REROLL_SHOP,
    Action.BUY_VOUCHER, Action.REROLL_BOSS, Action.SKIP_PACK,
}
PROGRESS |= {Action.BUY_SLOT + i for i in range(6)}
PROGRESS |= {Action.BUY_PACK + i for i in range(2)}
PROGRESS |= {Action.PICK_PACK + i for i in range(5)}


def rollout(policy, seed=None, stall_limit=500):
    """Play one run. Returns (fitness, game)."""
    g = Balatro(seed)
    stall = 0
    while not g.done and stall < stall_limit:
        mask = g.legal_mask()
        if not mask.any():
            break
        a = choose(policy, g)
        g.step(a)
        stall = 0 if a in PROGRESS else stall + 1

    # Fitness: blinds cleared, plus partial credit for the blind we died on.
    # The fractional part gives evolution something to climb from generation
    # one, instead of everyone scoring exactly 1.
    blinds = g.ante * 3 + g.blind
    margin = max(-1.0, min(1.0, (g.score_log2 - g.goal_log2) / 8.0))
    return blinds + (margin + 1) / 2, g


# ------------------------------------------------------------------- demo
if __name__ == "__main__":
    p = Policy()
    print(f"observation {OBS_SIZE} -> {NUM_ACTIONS} actions")
    print(f"genome size: {p.get_weights().numel():,} parameters\n")

    fit, g = rollout(p, "QJSKRTWR")
    print(g.render())
    print(f"\nfitness {fit:.2f}  (ante {g.ante}, won={g.won})")

    # A random net is the baseline every trained one must beat.
    fits = []
    for _ in range(20):
        p.set_weights(torch.randn(p.get_weights().numel()) * 0.5)
        fits.append(rollout(p)[0])
    print(f"\n20 random nets: mean {sum(fits)/len(fits):.2f}, best {max(fits):.2f}")
    print("(fitness 3.0 = cleared ante 1; 24.0 = cleared ante 8)")
