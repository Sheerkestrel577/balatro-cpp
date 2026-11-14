# naneinf — Python bindings

No build step, no dependencies (numpy optional). `naneinf.py` loads the
prebuilt library next to it via ctypes:

* **Windows** → `naneinf.dll` (included, statically linked)
* **Linux** → `libnaneinf.so` (included)
* **macOS** → build it yourself (see below)

```python
from naneinf import Balatro, Action, action_name

g = Balatro("QJSKRTWR")      # or Balatro() for a random real 8-char seed
g.step(Action.SELECT_BLIND)
for i in (2, 3, 4):
    g.step(Action.TOGGLE_CARD + i)
g.step(Action.PLAY)
print(g.render())
```

## The RL surface

```python
g.legal_mask()      # bool array, length NUM_ACTIONS (58) — sample only these
g.obs()             # float32 vector, length OBS_SIZE (222)
g.step(action)      # -> False if illegal (state unchanged)
g.done, g.won       # terminal + naneinf reached
g.score_log2        # log2 of THIS BLIND's score; naneinf is >= 1024
g.goal_log2
```

`step()` returns False rather than throwing on an illegal action, so a
policy that ignores the mask degrades instead of crashing — but always mask.

## Observation layout (222 floats)

| span | contents |
|---|---|
| 0–5 | phase one-hot |
| 6–19 | ante, blind, boss, money, hands, discards, score_log2/1024, goal_log2/1024, draw pile, hand/joker/consumable counts, hand size, won |
| 20–115 | 12 hand slots × 8 (present, rank, suit, enhancement, edition, seal, selected, face-down) |
| 116–165 | 10 joker slots × 5 (present, id/150, edition, selected, disabled) |
| 166–177 | 4 consumable slots × 3 |
| 178–201 | 6 shop slots × 4 |
| 202–209 | 2 packs × 3, voucher, skip tag |
| 210–221 | 12 hand levels |

Identities are **normalised indices, not one-hots** — feed them through
embedding layers rather than expanding to 150-wide vectors.

**Hidden information is hidden**: a face-down card reports rank/suit/etc. as
0, and a face-down joker reports id −1. The AI sees exactly what a player
would.

## Structured views

`g.hand()`, `g.jokers()`, `g.consumables()`, `g.shop()`, `g.packs()`,
`g.pack_contents()` return lists of dicts with names resolved — handy for
debugging, logging and hand-written heuristics.

## Actions

Indexed families are `BASE + index`:
`Action.TOGGLE_CARD + i`, `TOGGLE_JOKER + i`, `TOGGLE_CONSUMABLE + i`,
`BUY_SLOT + i`, `BUY_PACK + i`, `PICK_PACK + i`.
Singletons: `PLAY`, `DISCARD`, `USE_CONSUMABLE`, `SELL_JOKER`,
`SELL_CONSUMABLE`, `SELECT_BLIND`, `SKIP_BLIND`, `CONFIRM_CASHOUT`,
`LEAVE_SHOP`, `REROLL_SHOP`, `BUY_VOUCHER`, `REROLL_BOSS`, `HOLD_JOKER`,
`SKIP_PACK`, `CLEAR_SELECTION`, and the four shifts.

`action_name(a)` prints a readable label.

## Parallelism

Each `Balatro` is fully self-contained, so N instances can run in N
processes with no shared state. The library holds no globals per run — only
the static joker/consumable registries, which are read-only after startup.

## Rebuilding

```
g++ -std=c++20 -O2 -shared -fPIC capi.cpp balatro.cpp blinds.cpp \
    balatro_rng.cpp hand_eval.cpp jokers/*.cpp consumables/*.cpp \
    -o libnaneinf.so
```
Windows (from Linux): `x86_64-w64-mingw32-g++ ... -shared -static -o naneinf.dll`

`demo.py` runs random agents and prints a small Monte Carlo.


## Two starter agents

* **`agent.py`** — pure NumPy. No torch, no version clashes. Run
  `python agent.py` to watch a random-weight network play, and read it for
  the genome/rollout/fitness pattern evolution needs.
* **`policy.py`** — the same thing in PyTorch, if you want the torch idioms.

Neither trains anything: evolution mutates weights and keeps what scores
well, so there is no loss, no optimizer and no `.backward()` anywhere.

### If torch complains "Numpy is not available"

Your torch was built against NumPy 1.x and you have NumPy 2.x. Either
`pip install --upgrade torch`, or `pip install "numpy<2"`, or just use
`agent.py` and skip the problem.

### Fitness

`fitness(game)` = blinds cleared + partial credit for the blind you died on
(`3.0` = cleared ante 1, `24.0` = cleared ante 8). The fractional part is
what gives selection something to climb in early generations — plain "ante
reached" makes every starting individual score identically.
