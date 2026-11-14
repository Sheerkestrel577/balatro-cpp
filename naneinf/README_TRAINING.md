# README_TRAINING — the search + value-net loop

What this tree is: your v27 engine **plus clone support** (new `clone.cpp`,
five new C API functions, updated headers), a rebuilt Linux
`libnaneinf.so`, and the full iteration-zero training pipeline in
`python/`. Engine tests: 464/464 under ASan/UBSan after the changes.

ENGINE FIXES (v27.2): (a) The Eye / The Mouth no longer BLOCK restricted
plays -- they score ZERO instead (author's ruling): the hand is consumed,
Matador triggers, the Telescope/Supernova counter ticks, and the scoring
pipeline never runs, so Obelisk / Ride the Bus / Green Joker are
untouched. Kills the Mouth softlock; pinned by a twin-game invariant
test. (b) Mr. Bones' save now ENDS the round survived (score := goal, so
the normal win transitions fire); previously it left the game in
PHASE_PLAYING with 0 hands forever. (c) Agents seed imagination with
crc32, not hash() -- runs are now reproducible across processes without
pinning PYTHONHASHSEED. Test count: 474.

ENGINE FIX (v27.1): Balatro::broadcast iterated `jokers` by range-for
while a joker's interact could ADD jokers (Riff-Raff on blind select);
push_back reallocation = use-after-free, crashing ~1 in 40 shop-heavy
runs. Fixed with a size-snapshot index loop (jokers added mid-broadcast
don't see the event that created them); the INDEPENDENT scoring loop got
the same treatment as insurance. Latent since before the clone work --
the rollout agent's shopping volume exposed it. Both shipped binaries
(.so and .dll) are rebuilt with the fix; wine-verified 464/464.

## 0. If you're on Windows: rebuild the DLL first

The shipped `naneinf.dll` predates `clone.cpp`. `BUILD_WINDOWS.md` is
updated — the source list now includes `clone.cpp`. Until you rebuild,
`naneinf.py` still loads (new symbols are optional) but `clone()` /
`imagine()` / `deck()` raise with a rebuild hint, and the agents fall
back to slow replay-forking.

Sanity: `py -c "import naneinf; print(naneinf._MISSING or 'current')"`
then `py test_clone.py 25` → expect `ALL PASS`.

## 1. What got added to the engine

| API | What | Why |
|---|---|---|
| `nf_clone` | deep copy, ~12 µs | forking for search; 17–300x faster than replay |
| `nf_reshuffle_draws` | reshuffle draw pile, independent RNG | rollouts sample the shuffle instead of peeking at it |
| `nf_reseed_rng` | fresh BalatroRNG | rollouts crossing shops see sampled futures, not the run's real one |
| `nf_deck` | live deck composition | exact king count → the true fitness K·(100/N)², computable at any state |
| `nf_cleared8` | ante 8 beaten | the actual objective — `nf_won` is naneinf-overflow only |

Python side (`naneinf.py`): `g.clone()`, `g.imagine(seed)` (clone +
reshuffle + reseed = the honest-rollout primitive), `g.deck()`,
`g.deck_kings`, `g.cleared8`. Also `pack_contents()` now includes
`rank` (fixes a crash on Standard-pack picks).

**The one rule**: `clone()` copies the RNG state, so a bare clone's
future IS the run's future. Evaluating the deterministic outcome of a
visible choice (playing selected cards, buying a visible shop item) on a
clone is legal. Anything that draws cards or rolls dice must go through
`imagine()`. `test_clone.py` property C verifies this machinery works.

## 2. The loop, in order

All from `python/`. Times are single-core; scale by your cores
(`--workers`).

```
# (a) sanity: engine + clone behave
python3 test_clone.py 25                      # ALL PASS

# (b) baseline check of the search agent
python3 agent_v0.py                           # one random-seed run
python3 batch.py 60                           # ante distribution, ~3 min

# (c) GENERATION 0: collect self-play data
#     0.15 epsilon = 15% random shop buys, so rare jokers get outcome
#     statistics at all. More runs is straightforwardly better; 1000+
#     recommended, it's ~3.3 s/run/core.
python3 collect.py --runs 1000 --workers 8 --epsilon 0.15 \
        --out data/gen0.npz

# (d) train the value net
python3 train_value.py --data data/gen0.npz --out models/v_gen0.npz
#     small data (<5k states): add  --h1 64 --h2 32 --wd 1e-3
#     WHAT TO LOOK AT: val R^2 on 'prog' (positive = it predicts progress
#     on unseen runs) and the CALIBRATION table (monotone = it ranks
#     states correctly). Both improve primarily with MORE RUNS.

# (e) run the V-guided agent: shop decisions from V, joker dict unused
python3 agent_v1.py models/v_gen0.npz --runs 60

# (f) GENERATION 1+: collect WITH the trained agent, retrain on all data
#     (edit collect.py's one_run to build a ValueAgent — marked TODO —
#      or just alternate: v1 plays, collect.py-style logging attached)
python3 train_value.py --data "data/gen*.npz" --out models/v_gen1.npz
```

Repeat (e)-(f). Each generation: better agent → better data → better V.
Expect gen0's v1 ≈ v0 (that's honest: R²≈0 nets add nothing yet). The
crossover where V beats the hand-typed dict needs enough data that rare
jokers have statistics — that is exactly what epsilon and run count buy.

## 2.5 agent_v2 — honest rollout shopping (roadmap item 2, now built)

`agent_v2.py` replaces shop opinions with measurement: every affordable
option (buy, sell-and-replace when slots are full, pack, reroll, stop)
is applied in an imagine()d copy, fast-forwarded ~2 blinds with the v0
policy, and scored on blinds advanced + PER-BLIND OVERKILL MARGIN +
kings-fitness + a little money. Options share sampled futures (common
random numbers) so their difference is the decision, not luck.

First measured result: **2 wins / 30 runs vs 1 / 1000 for the dict**
(mean ante similar -- the change is entirely in the tail, which is where
winning lives). ~3.6 s/run. Generate gen1 training data with it:

    python3 collect.py --runs 500 --workers 8 --agent v2 --out data/gen1.npz

then retrain on everything: --data "data/gen*.npz". With real wins in
the data, y_fit / y_win finally have positive examples and HEAD_W can
start shifting toward the king objective.

Tuning: k (samples/option) and max_blinds in rollout() trade time for
decision quality; --model adds V to the rollout terminal score.

## 3. What's deliberately NOT in yet (your roadmap, in order of value)

1. **Wins.** v0 dies by fractions of a doubling in antes 4–6 holding
   junk jokers. First lever: x-mult acquisition + planet concentration.
   The intended fix is not better dict values — it's (2) and (3).
2. **Honest shop rollouts.** With `imagine()`, evaluate "buy X" by
   playing the next 1–2 blinds under k sampled futures and reading the
   terminal state. Slots into `agent_v1.shop()` where `value_of` is
   called — evaluate `imagine`-rollout outcomes instead of (or blended
   with) the one-step V score. This is the biggest missing piece and is
   now ~free at 12 µs/clone.
3. **Search-backup training targets.** Once rollouts exist, train V
   toward rollout-backed values (TD targets), blended with final
   outcomes. See the conversation: target networks, slow LR, replay
   across generations.
4. **Blind-skipping, selling, voucher strategy** — all legal actions the
   agents currently barely use; V + rollouts will price them.
5. **Curriculum toward the king objective.** Phase 1 is pure survival
   (current shaping). Once win rate is real, reweight training heads
   toward `y_fit` (train_value.py `HEAD_W`) and let Death/Strength/
   Cryptid/thinning lines dominate. The consumable lines are already
   implemented in `agent_v0.try_consumables`.

## 4. File map

```
w4/clone.cpp            NEW  the deep copy (read the header comment)
w4/capi.cpp             +5 functions
w4/balatro.hpp          +cloneGame() declaration
w4/balatro_rng.hpp      +RNG::clone() (+#include <memory>)
w4/build.sh, BUILD_WINDOWS.md   updated source lists
w4/python/libnaneinf.so REBUILT (Linux; rebuild the DLL on Windows)
w4/python/naneinf.py    +clone/imagine/deck/kings/cleared8, pack rank fix
w4/python/test_clone.py NEW  run after ANY engine change — it is the
                             tripwire for members missed in clone.cpp
w4/python/agent_v0.py   search agent: exact play eval via clones,
                             imagine()-based discard lookahead,
                             king-line consumables, dict-based shop
w4/python/agent_v1.py   same, but shop decisions from the value net
w4/python/collect.py    self-play -> (state, outcome) npz
w4/python/train_value.py multi-head value net (numpy; torch-ready shape)
w4/python/batch.py      quick ante-distribution eval
```

## 5. Honesty ledger (what still peeks, and how much it matters)

- Bare-clone play evaluation previews probabilistic in-scoring rolls
  (Misprint, Bloodstone, Lucky procs) and face-down card values. Small,
  bounded, and fixable by using `imagine()` in `eval_play` at the cost
  of k× evaluations — worth doing once those jokers matter.
- Discard evaluation and everything in `imagine()` is leak-free.
- `test_clone.py` C-property is the regression test for the whole
  honesty story. Keep it green.
