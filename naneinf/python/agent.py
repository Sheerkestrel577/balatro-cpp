"""
agent.py — a policy network and a rollout, in pure NumPy.

No torch, no version clashes. Evolution never computes a gradient, so the
entire "neural network" is three matrix multiplies:

    scores = tanh(tanh(obs @ W1 + b1) @ W2 + b2) @ W3 + b3

policy.py is the same thing written with torch, if you prefer that.

Run it:   python agent.py
"""
import numpy as np

from naneinf import Balatro, Action, NUM_ACTIONS, OBS_SIZE

HIDDEN = 128


# ------------------------------------------------------------------ genome
def genome_size(hidden=HIDDEN):
    return (OBS_SIZE * hidden + hidden          # layer 1
            + hidden * hidden + hidden          # layer 2
            + hidden * NUM_ACTIONS + NUM_ACTIONS)  # output


def random_genome(rng, hidden=HIDDEN, scale=0.5):
    return rng.normal(0, scale, genome_size(hidden)).astype(np.float32)


def unpack(genome, hidden=HIDDEN):
    """Slice the flat genome into weight matrices (no copying)."""
    i = 0
    def take(n, shape):
        nonlocal i
        w = genome[i:i + n].reshape(shape)
        i += n
        return w
    W1 = take(OBS_SIZE * hidden, (OBS_SIZE, hidden))
    b1 = take(hidden, (hidden,))
    W2 = take(hidden * hidden, (hidden, hidden))
    b2 = take(hidden, (hidden,))
    W3 = take(hidden * NUM_ACTIONS, (hidden, NUM_ACTIONS))
    b3 = take(NUM_ACTIONS, (NUM_ACTIONS,))
    return W1, b1, W2, b2, W3, b3


def forward(params, obs):
    W1, b1, W2, b2, W3, b3 = params
    h = np.tanh(obs @ W1 + b1)
    h = np.tanh(h @ W2 + b2)
    return h @ W3 + b3


# --------------------------------------------------------------- rollout
# Actions that change real game state. A run doing only the others (toggles,
# shifts, clear) is stuck moving the cursor and never acting.
PROGRESS = {
    Action.PLAY, Action.DISCARD, Action.USE_CONSUMABLE, Action.SELL_JOKER,
    Action.SELL_CONSUMABLE, Action.SELECT_BLIND, Action.SKIP_BLIND,
    Action.CONFIRM_CASHOUT, Action.LEAVE_SHOP, Action.REROLL_SHOP,
    Action.BUY_VOUCHER, Action.REROLL_BOSS, Action.SKIP_PACK,
}
PROGRESS |= {Action.BUY_SLOT + i for i in range(6)}
PROGRESS |= {Action.BUY_PACK + i for i in range(2)}
PROGRESS |= {Action.PICK_PACK + i for i in range(5)}


def rollout(genome, seed=None, hidden=HIDDEN, stall_limit=500, max_ante=99):
    """Play one run. Returns (fitness, game)."""
    params = unpack(genome, hidden)
    g = Balatro(seed)
    stall = 0
    while not g.done and stall < stall_limit and g.ante <= max_ante:
        mask = g.legal_mask()
        if not mask.any():
            break
        scores = forward(params, g.obs())
        scores[~mask] = -np.inf          # illegal actions can never win
        a = int(scores.argmax())
        g.step(a)
        stall = 0 if a in PROGRESS else stall + 1
    return fitness(g), g


def fitness(g):
    """Blinds cleared + partial credit for the blind we died on.

    The fraction matters: without it every early individual scores exactly
    1.0 and selection has nothing to climb.
    """
    blinds = g.ante * 3 + g.blind
    margin = float(np.clip((g.score_log2 - g.goal_log2) / 8.0, -1.0, 1.0))
    return blinds + (margin + 1) / 2


# ------------------------------------------------------------------- demo
if __name__ == "__main__":
    rng = np.random.default_rng(0)
    print(f"observation {OBS_SIZE} -> {NUM_ACTIONS} actions")
    print(f"genome: {genome_size():,} parameters ({HIDDEN} hidden)\n")

    f, g = rollout(random_genome(rng), "QJSKRTWR")
    print(g.render())
    print(f"\nfitness {f:.2f}\n")

    fits = [rollout(random_genome(rng))[0] for _ in range(20)]
    print(f"20 random genomes: mean {np.mean(fits):.2f}, best {max(fits):.2f}")
    print("(3.0 = cleared ante 1 · 24.0 = cleared ante 8)")
