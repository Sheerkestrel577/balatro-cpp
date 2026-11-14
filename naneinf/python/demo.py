"""Random-agent demo: plays whole runs through the action interface."""
import random, sys
from naneinf import Balatro, Action, NUM_ACTIONS, OBS_SIZE, action_name

def play_run(seed=None, verbose=False, max_steps=20000):
    g = Balatro(seed)
    steps = 0
    while not g.done and steps < max_steps:
        legal = g.legal_actions()
        if not legal:
            break
        # a random agent that at least prefers to act rather than fiddle
        weighted = [a for a in legal
                    if a not in (Action.CLEAR_SELECTION,
                                 Action.SHIFT_CARD_LEFT, Action.SHIFT_CARD_RIGHT,
                                 Action.SHIFT_JOKER_LEFT, Action.SHIFT_JOKER_RIGHT)]
        a = random.choice(weighted or legal)
        if verbose:
            print(f"[{g.phase_name}] {action_name(a)}")
        g.step(a)
        # the engine parks in CASH_OUT until confirmed
        steps += 1
    return g, steps

if __name__ == "__main__":
    seed = sys.argv[1] if len(sys.argv) > 1 else None
    print(f"action space {NUM_ACTIONS}, observation {OBS_SIZE} floats\n")

    g, steps = play_run(seed)
    print(g.render())
    print(f"\n-> {'NANEINF' if g.won else 'died'} at ante {g.ante} after {steps} steps")

    # a tiny Monte Carlo, the shape the real experiment will take
    print("\n10 random runs:")
    reached = []
    for _ in range(10):
        r, _s = play_run()
        reached.append(r.ante)
    print("  antes reached:", sorted(reached))
    print("  mean:", sum(reached) / len(reached))
