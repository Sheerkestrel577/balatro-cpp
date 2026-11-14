# Joker Pattern Cookbook

**How to implement joker #N:** find its archetype below, open that file,
copy it, rename, change the numbers. Every example compiles and is covered
by a test in `tests.cpp`.

| If the joker... | Pattern file | Key points |
|---|---|---|
| adds flat mult/chips | `joker_basic.cpp` | `INDEPENDENT`, one line |
| cares "if hand **contains** X" | `jolly_joker.cpp` | reads `containedHands` bitmask, **never** `handType` |
| grows a counter over time | `ride_the_bus.cpp` | **the `isCopy` pattern**: effect unguarded, state change guarded |
| rolls a chance | `bloodstone.cpp` | `g.chance(key,n,d)`; **not** isCopy-guarded (copies roll their own) |
| can destroy itself | `gros_michel.cpp` | `queueDestroyJoker`; never erase mid-broadcast |
| decays each hand and dies | `ice_cream.cpp` | `HAND_SCORED` = the "cards fly away" moment |
| triggers on held cards | `baron.cpp` | `CARD_HELD`, once per trigger (retriggers multiply free) |
| pays money on round end | `rocket.cpp` | `ROUND_END`, growth on `BOSS_DEFEATED` |
| changes its own sell value | `egg.cpp` | mutate `bonusSell`, not score |
| reacts to destroyed cards | `canio.cpp`, `glass_joker.cpp` | ask `isFace` **at that moment** — Pareidolia/Sixth Sense feed it free |
| retriggers scored cards | `sock_and_buskin.cpp` | `RETRIG_QUERY_SCORED`, `++g.scoring.retriggers` |
| retriggers held cards | `mime.cpp` | `RETRIG_QUERY_HELD` |
| inspects the held hand | `blackboard.cpp` | loop `g.scoring.held`, use `suitMask` |
| reads money / run counters | `bull.cpp`, `fortune_teller.cpp` | live read at trigger time |
| permanently changes a card | `midas_mask.cpp` | writes a **fact** (allowed); contrast Pareidolia |
| affects only the first X | `photograph.cpp` | per-hand flag reset at `HAND_DETERMINED` |
| listens for lucky procs | `lucky_cat.cpp` | engine raises `LUCKY_TRIGGERED` |
| changes a **rule** | `splash.cpp`, `shortcut.cpp`, `smeared_joker.cpp`, `pareidolia.cpp`, `four_fingers.cpp` | `passiveModifiers` only; never copied |
| stacks with duplicates | `oops_all_6s.cpp` | `m.x *= 2` rather than `= true` |
| does something when sold | `luchador.cpp` | `SELF_SOLD` fires before removal |
| pays at the cash out | `golden_joker.cpp` | `ROUND_END` + `g.addMoney()`; engine diverts it into the frozen payout |
| pays from deck contents | `cloud_9.cpp` | scan `g.deck` at `ROUND_END` |
| pays from a run counter | `satellite.cpp` | `g.uniquePlanetsUsed` |
| pays only if unused | `delayed_gratification.cpp` | `g.discardsUsedThisRound`, reset by `confirmCashOut()` |
| pays during scoring | `business_card.cpp`, `golden_ticket.cpp` | `CARD_SCORED` + `addMoney`; commits instantly (Bull sees it) |
| pays from held cards | `reserved_parking.cpp` | `CARD_HELD` + chance |
| pays on discard | `faceless_joker.cpp` (per action), `mail_in_rebate.cpp` (per card) | note which of the two events |
| changes an economy rule | `credit_card.cpp`, `to_the_moon.cpp` | passive: debt floor, interest cap |
| reads money as score | `bootstraps.cpp` | mirror of an econ joker |
| copies another joker | `blueprint.cpp`, `brainstorm.cpp` | `g.invokeAsCopy` (depth guard handles cycles) |

## File template (use this every time)

```cpp
#include "../balatro.hpp"

namespace {                      // <-- ALWAYS. internal linkage.

class MyJoker : public Joker { ... };
REGISTER_JOKER(MyJoker, "My Joker", rarity, cost)

}  // namespace
```

The anonymous namespace is not decoration. Two `.cpp` files declaring the
same class name is a One Definition Rule violation: the linker silently
picks one body and every copy shares it, with **no compiler error**. That
is exactly what happens when you copy a joker file and forget to rename.
Internal linkage makes it impossible. `tests.cpp` also asserts the registry
contains no duplicate names, which catches the forgotten `REGISTER_JOKER`
string.

## Two C++ traps that have already bitten this project

1. **ALWAYS brace multi-line `if` bodies.** C++ ignores indentation, so
   ```cpp
   if (r == 14)
       mult += 4;
       chips += 20;   // <-- runs for EVERY card. No warning.
   ```
   is a silent scoring bug. Both Scholar and Walkie Talkie shipped with it.
   Build with `-Wall -Wextra` (the build script does) and brace everything.
2. **Anonymous namespace in every joker file** (see the template above) —
   copy-pasting a file and forgetting to rename the class is an ODR
   violation the linker resolves silently and wrongly.

## The three rules that prevent 90% of bugs

1. **Guard state, not effects.** `if (isCopy) return;` goes around counter
   increments and self-destruction — never around score/money effects or
   probability rolls.
2. **Ask, don't store.** Use `g.isFace()`, `g.suitMask()`,
   `g.suitMatches()`, `g.scoring.containedHands`. Never write a joker's
   influence onto a card. (Tarots/enhancements writing card facts is fine —
   they change what a card *is*, not what it *counts as*.)
3. **Queue destruction.** `queueDestroyJoker` / `destroyCardLater`; the
   engine sweeps after the broadcast. Erasing mid-loop is use-after-free.

## The cash-out phase

`computeCashOut(blindIdx)` builds and **freezes** the itemized payout in the
wiki's order — blind reward, unused hands, gold cards, joker effects,
Investment Tag, then interest. Nothing is credited until `confirmCashOut()`.
Between the two, the game sits in `PHASE_CASH_OUT`, where consumables can
still be used (blue-seal planets, tarots) *before* the shop loads.

Two behaviors this protects, both user-confirmed in game:
* **The Hermit can't launder econ jokers.** Doubling money on the cash-out
  screen doubles the wallet only; Golden Joker / To the Moon money is still
  pending and untouchable.
* **The interest line is frozen.** Crossing a threshold after the list is
  computed does not update it.
* **Interest is computed on the wallet only.** In-round money counts (gold
  seals, Rough Gem, Business Card commit instantly), but no cash-out line
  does — Golden Joker's $4 can never push you over a breakpoint.

Econ jokers themselves stay trivial: subscribe to `ROUND_END`, call
`g.addMoney()`. The engine's freeze flag diverts it into the pending payout
automatically — a joker never knows the difference.

## Numbers

Every value in these files carries `⚠ verify`. Patterns are the deliverable;
exact chips/mult/odds are data entry from the wiki at implementation time.

---

# Interaction layer

The AI drives the game through a fixed-width action space with a legality
mask — never by touching fields directly.

```cpp
bool mask[NUM_ACTIONS];
game.legalActions(mask);        // sample only over legal actions
game.applyAction(a);            // returns false (no-op) if illegal
```

## Actions
`A_TOGGLE_CARD_0..11`, `A_TOGGLE_JOKER_0..9`, `A_TOGGLE_CONSUMABLE_0..3`,
`A_SHIFT_CARD_LEFT/RIGHT`, `A_SHIFT_JOKER_LEFT/RIGHT`, `A_PLAY`,
`A_DISCARD`, `A_USE_CONSUMABLE`, `A_SELL_CONSUMABLE`, `A_SELL_JOKER`,
`A_CONFIRM_CASHOUT`, `A_CLEAR_SELECTION`.

Rules baked into the mask:
* Card selection caps at 5 and requires `PHASE_PLAYING`.
* **Shifts require exactly one selected item.** Ambiguity-free, and any
  permutation is still reachable by repeated single shifts.
* Joker/consumable actions are legal in **every** phase (real Balatro lets
  you rearrange and sell at blind select, mid-round, at cash out, in shop).
* `A_USE_CONSUMABLE` asks the consumable's own `canUse(game)`.

## Selection is the targeting mechanism
`sel.cards` is ordered by selection, so consumables read it directly:
Death needs exactly 2 (left = first selected), Strength up to 2, The Hermit
none. `canUse()` returning false masks the action out. No separate
targeting system exists or is needed.

## Writing a consumable
```cpp
namespace {
class Death : public Consumable {
public:
    bool canUse(const Balatro& g) const override { return g.sel.cards.size() == 2; }
    void use(Balatro& g) override { ... }   // mutate FACTS here
};
REGISTER_CONSUMABLE(Death, "Death", /*kind 0=tarot*/ 0, /*cost*/ 3)
}
```

## The joker-rearrange seam
Real Balatro snapshots joker order per pass; you may drag mid-scoring but
it only affects the *next* pass. The engine gets this free: `playHand()` is
one atomic call, and the player may rearrange before `computeCashOut()`
runs the **retrigger-aware end-of-round held pass** — where copiers pointed
at Mime double held gold cards and blue seals. That is the late-game money
engine, and `tests.cpp` asserts it ($42 with Mime vs $21 without).

## Card identity
Cards live in a `std::deque<Card>` (pointers stay valid as the deck grows)
and carry a stable `id`. **Never change the deck to a `std::vector`** —
one tarot-added card would dangle every outstanding `Card*`.

---

# RNG stream discipline (critical for the experiment)

Every random purpose gets its **own key**, and every key gets its **own
independent stream** — in both backends. `MersenneRNG` no longer shares one
generator; it lazily creates a `mt19937_64` per key, seeded from
`hash(key, runSeed)`.

Why this is not optional: with a shared stream, any stylistic choice (which
card to play, one extra discard) shifts every later roll, so two agents
evaluated on "the same seed" would face different shops from ante 2 onward.
Common random numbers — the entire basis for comparing agents within a
generation — would be worthless.

Dedicated keys, all embedding the ante:
| Purpose | Key shape |
|---|---|
| shop slot type/rarity/pick/edition | `shop<ante>_<blind>_<slot>_<rerolls>_type` etc. |
| skip tag | `Tag<ante>_<blind>` |
| voucher | `Voucher<ante>` |
| deck shuffle | `shuffle` |
| lucky cards, glass, joker procs | `lucky_mult`, `glass`, `<joker name>` |

`tests.cpp` proves it: two runs on seed 4242 that play completely
differently (one discards twice and plays three cards, the other plays one)
still get **byte-identical shops, vouchers and skip tags**, and three
rerolls in ante 1 do not shift ante 1's big-blind shop. Different seeds do
differ. If you add a new random effect, give it a new key — never reuse
another subsystem's.

# The ante loop

`enterBlindSelect()` → `A_SELECT_BLIND` (or `A_SKIP_BLIND` for a tag) →
`startRound()` → play/discard actions → `computeCashOut(blindIdx)` →
`A_CONFIRM_CASHOUT` → shop (`A_BUY_SLOT_n`, `A_BUY_VOUCHER`,
`A_REROLL_SHOP`) → `A_LEAVE_SHOP` → next blind; ante increments after the
boss. Vouchers write into `PassiveMods` via `applyVouchers()`.

---

# The full game loop (v11)

```
enterBlindSelect()          bossId rolled here (blind 2 only)
  A_SELECT_BLIND  ──►  startRound()   score = 0, boss onSelect fires
  A_SKIP_BLIND    ──►  tag granted, next blind
      │
   PHASE_PLAYING   A_TOGGLE_CARD_n / A_PLAY / A_DISCARD / consumables
      │            checkDeath() after every hand
      ▼
  computeCashOut(blindIdx)   frozen payout; PHASE_CASH_OUT
  A_CONFIRM_CASHOUT ──► openShop()
      │
   PHASE_SHOP      A_BUY_SLOT_n, A_BUY_VOUCHER, A_REROLL_SHOP,
      │            A_BUY_PACK_n ──► PHASE_PACK (A_PICK_PACK_n / A_SKIP_PACK)
      ▼
  A_LEAVE_SHOP ──► next blind; ante++ after the boss
```

## Score & death
* **Score resets every blind.** naneinf is a per-blind overflow; `won()` is
  a latched run-level flag set the moment it happens (`naneinfAnte` records
  where). `clearedAnte8()` is the evaluation cut-off.
* Death: out of hands with the goal unmet → `PHASE_GAME_OVER`, unless
  **Mr. Bones** (score ≥ 25% of goal → survive, Bones self-destructs).

## Blind requirements
Antes 0–8 from the fixed table; ante 9+ from
`ante8 * (1.6 + (0.75k)^(1+0.2k))^k`, **truncated to two significant
digits** (not rounded — verified against the wiki's published values, which
only match with flooring). Plasma ×2 on top. Boss multipliers: 2× normally,
**4× The Wall, 1× The Needle, 6× Violet Vessel**. Tests assert exact matches
out to ante 32 (2.7e179), and deep endless is rebuilt in BigNum so it never
saturates.

## Boss blinds
All 28 (23 regular + 5 showdown) in `blinds.cpp`. Selection follows the real
rule: **fewest appearances first**, min-ante gated, showdown only on
multiples of 8, from a dedicated RNG key. Rerollable with Director's Cut
(once/ante) or Retcon (unlimited), $10.

**Debuff is an interpretation, never a stored fact** — `g.isDebuffed(card)`
consults the active boss, so The Club/Goad/Window/Head (suits), The Plant
(faces, Pareidolia-aware), The Pillar (`playedThisAnte`), and Verdant Leaf
(until a joker is sold) all work without mutating cards.

## Hidden information
`Card::faceDown` is set on draw by The House (first hand), The Fish (after
each hand), The Mark (face cards), and The Wheel (1 in 7). `Joker::faceDown`
by Amber Acorn, which also shuffles — the player may exempt one joker via
`A_HOLD_JOKER`. The observation encoder **must mask rank/suit of face-down
cards**.

---

# Consumables (v13)

All 22 tarots and 12 planets are implemented in `consumables/`. Engine
helpers available to them:

| Helper | Use |
|---|---|
| `g.clearSelectionAfterUse()` | after mutating selected cards |
| `g.destroyCardLater(card)` + `g.runDestructionWindow()` | The Hanged Man |
| `g.createRandomConsumables(kind, n, key)` | Emperor, High Priestess |
| `g.createRandomJoker(key)` | Judgement |
| `g.grantConsumable(registryIdx)` | The Fool |
| `g.levelUpHand(type)` | every planet; maintains `uniquePlanetsUsed` |

## Two hazards this subsystem exposed

**1. Use-after-free on `consumables`.** `A_USE_CONSUMABLE` now *moves the
card out of the vector and erases it before calling `use()`*, because
creators (Emperor, High Priestess, Fool) `push_back` during `use()`, which
reallocates the vector and dangles any reference held across the call.
ASan caught this as a heap-use-after-free. Consuming before resolving also
matches the game — the card frees its own slot.

**2. `Card` brace-init is a trap.** `id` is the first member, so
`Card{9, SPADES}` sets `id = 9, rank = 0`. Always assign fields by name.

## Steel Joker: the xmult shape
Cumulative-count jokers are `x(1 + rate*n)`, never `x(n/k)` — the latter
**zeroes the mult** when the count is 0. Steel Joker shipped with that bug.

---

# v14: content complete

**150/150 jokers, 52/52 consumables.** Every event hook now has a consumer:

| Hook | Consumer |
|---|---|
| `SHOP_EXITED` | Perkeo (negative consumable copy, ignores slot cap) |
| `PACK_OPENED` | Hallucination |
| `PACK_SKIPPED` | Red Card |
| `SHOP_REROLL` | Flash Card |
| `CARD_ADDED` | Hologram |
| `BOSS_TRIGGERED` | Matador |
| `SELF_SOLD` | Luchador, Diet Cola, Invisible Joker |
| `CARDS_DESTROYED` | Canio, Glass Joker |
| `LUCKY_TRIGGERED` | Lucky Cat |
| `CONSUMABLE_USED` | Constellation, Fortune Teller |

## Chicot stacks by INVERTING bosses (wiki-documented)

Disabling a boss *reverses* its effect, and every copy disables it, so N
copies apply the inverse N times:

| Boss | Per extra Chicot |
|---|---|
| The Wall | score requirement ÷ 2 |
| Violet Vessel | score requirement ÷ 3 |
| The Water | + the original discard count |
| The Needle | + (original hands − 1) |
| The Manacle | **+1 hand size, permanently** (`permanentHandSizeBonus`) |

Debuff/effect bosses don't invert — they're simply off. Chicot is Legendary
and only comes from The Soul, so copies require Invisible Joker, Ankh, or a
Showman second Soul. `mods.chicotCount` counts copies; `bossIsDisabled()`
stays boolean.

## Joker Stencil counts itself — and can hit X0
`x = 1 + (jokerSlots − jokerCount)`, **not clamped to 1**. Exceed your slots
(negative jokers) and Stencil becomes X0, wiping your mult. User-confirmed.


---

# Consumable handling rules (v23)

* **Pack consumables are USED IMMEDIATELY.** Arcana / Celestial / Spectral
  picks never enter your consumable slots — `pickPackCard` routes them
  through `useConsumableNow()`. Buffoon packs give jokers and Standard packs
  give playing cards; those are kept.
* **Buy-and-use.** With full consumable slots a shop consumable is still
  buyable *if it can resolve right now*, and it resolves immediately.
  `consumableUsableNow()` is the guard, so card-targeting tarots need a
  selection and creators need room — no silent no-ops.
* **Targeting in the shop.** Opening an Arcana/Spectral/Standard pack with
  no hand deals one (the game shows your hand there), and card selection is
  legal during `PHASE_PACK`. The dealt cards go back on `skipPack()`.
  ⚠ this deal is not verified against the game's RNG consumption.
* **A used consumable frees its own slot first.** `A_USE_CONSUMABLE` moves
  the card out of the vector and erases it *before* calling `use()`, so The
  High Priestess can create two planets from one slot (and creators can't
  dangle a reference into a vector they grow).

# Locks: resample, never remove

Locked pool entries are **resampled past** on `_resample2`, `_resample3`,
... They are never removed from the pool, because a roll picks an INDEX —
removing an entry would shift every later item. Two lock sets exist:
ante-gated tags/bosses, and secret consumables (Planet X / Ceres / Eris /
The Soul / Black Hole). See `PHASE2_STATUS.md`.
