"""test_clone.py — behavioral verification of nf_clone.

Three properties, each load-bearing for the search architecture:

  A. FIDELITY: clone then feed both games identical actions -> identical
     observations, money, score, phase at every step, for hundreds of steps.
     Catches any member missed in clone.cpp (divergence shows up fast:
     RNG state, counters, pointer remaps all feed back into behavior).

  B. INDEPENDENCE: stepping the clone never changes the original.

  C. HONEST IMAGINATION: after nf_reshuffle_draws + nf_reseed_rng, the
     clone's future DIVERGES from the original's (different draws/shops) --
     while the deck's card MULTISET stays identical (we shuffled, not
     replaced). This is the anti-seed-leak property.

Run:  python3 test_clone.py [n_seeds]
"""
import ctypes, random, sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__))))
import naneinf
from naneinf import Balatro

LIB = naneinf._lib
LIB.nf_clone.restype = ctypes.c_void_p
LIB.nf_clone.argtypes = [ctypes.c_void_p]
LIB.nf_reshuffle_draws.argtypes = [ctypes.c_void_p, ctypes.c_uint64]
LIB.nf_reseed_rng.argtypes = [ctypes.c_void_p, ctypes.c_uint64]
LIB.nf_deck.argtypes = [ctypes.c_void_p,
                        ctypes.POINTER(ctypes.c_int32)]
LIB.nf_deck.restype = ctypes.c_int


def wrap(handle):
    g = Balatro.__new__(Balatro)
    g._h = handle
    g._legal_buf = (ctypes.c_int32 * naneinf.NUM_ACTIONS)()
    g._obs_buf = (ctypes.c_float * naneinf.OBS_SIZE)()
    g._int_buf = (ctypes.c_int32 * 256)()
    return g


def deck_multiset(g):
    buf = (ctypes.c_int32 * 4096)()
    n = LIB.nf_deck(g._h, buf)
    return sorted(tuple(buf[i * 5 + j] for j in range(5)) for i in range(n))


def sig(g):
    return (g.phase, g.money, round(g.score_log2, 9), g.ante, g.blind,
            g.hands_left, g.discards_left, tuple(g.obs()))


def drive(g, rng, steps):
    """Random-legal walk; returns the action trace taken."""
    trace = []
    for _ in range(steps):
        if g.done:
            break
        acts = g.legal_actions()
        a = rng.choice(acts)
        if g.step(a):
            trace.append(a)
    return trace


def replay_on(g, trace):
    for a in trace:
        ok = g.step(a)
        if not ok:
            return False
    return True


def main(n_seeds=25):
    rng = random.Random(0)
    fails = tested = c_diverged = c_tested = 0
    t = -1
    while tested < n_seeds:
        t += 1
        g = Balatro()
        # advance to a random depth so clones are taken mid-run, in varied
        # phases (playing, shop, pack, cash-out)
        drive(g, rng, rng.randint(5, 300))
        if g.done:
            continue                     # dead before clone point: reroll seed
        tested += 1

        # take ALL clones from the pristine state, BEFORE test A mutates g
        c  = wrap(LIB.nf_clone(g._h))    # A: fidelity twin
        c3 = wrap(LIB.nf_clone(g._h))    # C: imagined twin
        c4 = wrap(LIB.nf_clone(g._h))    # C: un-imagined twin

        # ---- A: fidelity under identical actions ----
        if sig(g) != sig(c):
            print(f"[{g.seed}] FAIL A0: clone differs immediately")
            fails += 1
            continue
        probe = random.Random(1000 + t)
        ok = True
        for step in range(400):
            if g.done or c.done:
                if g.done != c.done:
                    print(f"[{g.seed}] FAIL A: done-divergence at {step}")
                    ok = False; fails += 1
                break
            acts = g.legal_actions()
            if acts != c.legal_actions():
                print(f"[{g.seed}] FAIL A: legal sets differ at {step}")
                ok = False; fails += 1; break
            a = probe.choice(acts)
            rg, rc = g.step(a), c.step(a)
            if rg != rc or sig(g) != sig(c):
                print(f"[{g.seed}] FAIL A: divergence at step {step}, "
                      f"action {naneinf.action_name(a)}")
                ok = False; fails += 1; break
        if not ok:
            continue

        # ---- B: independence ----
        c2 = wrap(LIB.nf_clone(g._h))
        before = sig(g)
        drive(c2, random.Random(2000 + t), 100)
        if sig(g) != before:
            print(f"[{g.seed}] FAIL B: stepping clone mutated original")
            fails += 1
            continue

        # ---- C: honest imagination diverges, multiset preserved ----
        # c3/c4 were cloned from the PRISTINE state above; c3 gets imagined.
        ms_before = deck_multiset(c3)
        LIB.nf_reshuffle_draws(c3._h, 0xC0FFEE + t)
        LIB.nf_reseed_rng(c3._h, 0xBEEF + t)
        if deck_multiset(c3) != ms_before:
            print(f"[{g.seed}] FAIL C: reshuffle changed card multiset")
            fails += 1
            continue
        # C1 (direct): from a PLAYING state with a nonempty draw pile,
        # discarding the same cards must (almost surely) draw different
        # replacements in the imagined twin. This is the seed-leak check
        # at the exact mechanism level, no random walk required.
        seed_name = c4.seed
        diverged = None
        if c3.phase == 1 and c3.draw_pile >= 3 and c3.discards_left > 0:
            c_tested += 1
            from naneinf import Action as _A
            for i in range(3):
                c3.step(_A.TOGGLE_CARD + i); c4.step(_A.TOGGLE_CARD + i)
            ok3, ok4 = c3.step(_A.DISCARD), c4.step(_A.DISCARD)
            if ok3 and ok4:
                h3 = [c["name"] for c in c3.hand()]
                h4 = [c["name"] for c in c4.hand()]
                diverged = h3 != h4
                if diverged: c_diverged += 1
        # C2 (walk): fallback for non-playing phases -- identical random
        # actions until states differ (shop rerolls, pack rolls, draws).
        if diverged is None:
            c_tested += 1
            probe2 = random.Random(3000 + t)
            diverged = False
            for step in range(200):
                if c3.done or c4.done:
                    diverged = (c3.done != c4.done); break
                acts3, acts4 = c3.legal_actions(), c4.legal_actions()
                if acts3 != acts4 or sig(c3) != sig(c4):
                    diverged = True; break
                a = probe2.choice(acts3)
                c3.step(a); c4.step(a)
            if diverged: c_diverged += 1
        print(f"[{seed_name}] ok (A fidelity 400, B independence, "
              f"C {'DIVERGED' if diverged else 'no-div'})")
    print(f"\nA/B: {'ALL PASS' if fails == 0 else f'{fails} FAILURES'} "
          f"over {tested} live seeds")
    print(f"C (honest imagination): {c_diverged}/{c_tested} diverged -- "
          f"expect nearly all; 0 or low means reshuffle/reseed is broken")
    if c_tested and c_diverged < max(1, int(0.7 * c_tested)):
        print("C FAILURE: imagination is not diverging from the true "
              "future -- seed leak protection NOT verified")
        fails += 1
    return fails


if __name__ == "__main__":
    sys.exit(main(int(sys.argv[1]) if len(sys.argv) > 1 else 25))
