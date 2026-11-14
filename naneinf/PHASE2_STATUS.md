# Phase 2: seed exactness — status

## What now works

`Balatro g("ALEEB");` constructs a run driven by **Balatro's real RNG stack**
— pseudohash → per-key LCG nodes → LuaJIT's Tausworthe generator — the port
that is bit-verified against LuaJIT (137/137 vectors). `play.cpp` asks for an
8-character seed and rolls a real one from the game's 35-char alphabet if you
leave it blank.

`keys.hpp` holds the **real key dictionary**, transcribed from Immolate's
`lib/cache.cl` (CC BY-NC-SA 4.0, SpectralPack). Composition rule confirmed
from `instance.cl`: a key is `type + source + ante`, with `_resample<N+1>`
appended on resamples.

Subsystems now using real key strings:

| Subsystem | Key |
|---|---|
| shop item type | `cdt` + `sho` + ante |
| shop joker rarity | `rarity` + `sho` + ante |
| shop joker pick | `Joker1/2/3/4` + `sho` + ante |
| shop joker edition | `edi` + `sho` + ante |
| shop tarot / planet | `Tarot`/`Planet` + `sho` + ante |
| booster pack | `shop_pack` + ante |
| skip tag | `Tag` + ante |
| voucher | `Voucher` + ante |
| boss blind | `boss` (no ante — the game uses one stream) |
| deck shuffle | `nr` + ante |
| lucky cards | `lucky_mult`, `lucky_money` |
| Misprint / Gros Michel / Cavendish | `misprint`, `gros_michel`, `cavendish` |
| standard playing cards | `front`/`Enhanced`/`stdset`/`stdseal`/`stdsealtype`/`standard_edition` + ante |

## Pool orderings — DONE

`pools.hpp` now carries the game's pool order for all ten pools, transcribed
from Immolate's `items.cl`: 61/64/20/5 jokers, 22 tarots, 12 planets, 18
spectrals, **32 vouchers**, **24 tags**, 28 bosses. A roll picks an *index*,
so this was as load-bearing as the key strings. The engine builds every pool
in game order now (shop, packs, Judgement, Wraith, The Soul, Riff-Raff,
bosses).

Side benefit: pool membership independently **verified all 150 rarities** —
zero mismatches after the audit fixes, which confirms them against a second
source.

## Three more bugs the source caught

1. **Rarity key order.** `functions.cl` passes `{N_Type, N_Ante, N_Source}`
   for the rarity roll — ante *before* source, unlike almost every other
   key. So it's `rarity1sho`, not `raritysho1`.
2. **Card-type key has no source.** `{N_Type, N_Ante}` → `cdt1`.
3. **Edition thresholds were wrong.** The real cutoffs are Negative >0.997,
   Poly >0.994, **Holo >0.98, Foil >0.96**. I had 0.988/0.980.

Also confirmed: shop rates are joker 20 / tarot 4 / planet 4 (spectral and
playing card 0 without vouchers), walked in that order after scaling the
`cdt` roll by the total; rarity thresholds >0.95 rare, >0.7 uncommon; and
the boss stream genuinely has no ante component.

## The shop queue — DONE (and it was simpler than feared)

The "biggest remaining gap" turned out to be over-engineering on my part.
Immolate's analyzer just calls `next_shop_item(inst, ante)` repeatedly under
the heading **"Shop Queue"** — there is no per-slot key and no per-slot
resample. Every call advances the *same* per-key streams (`cdt<ante>`,
`rarity<ante>sho`, `Joker1sho<ante>`, ...). My old code faked a resample
index per slot, which split one stream into many.

Two consequences now implemented:

* **The queue is per ANTE, not per shop.** It carries across all three
  blinds. The small blind's shop takes the first items, the big blind's the
  next, and so on.
* **Rerolls advance the queue.** Rerolling in ante 1's small-blind shop
  changes what the big-blind shop offers. This is real behaviour, and a test
  that previously asserted the opposite has been corrected.

Resampling now appears only where the game uses it: when a drawn item is
unavailable (already owned, or already in this shop), `randchoice_common`
retries the same key with `_resample2`, `_resample3`, ... Showman disables
the exclusion.

**Critically, the property the AI experiment needs still holds:** play
decisions (which cards you play or discard) never shift a later shop. Only
shop actions do. Both directions are now tested.

## What is NOT yet true

**Seed-exactness is not achieved.** Correct key *strings* are necessary but
not sufficient — the engine must also consume each key the same number of
times in the same order as the Lua. Three known gaps:

1. **Gameplay-effect key names are conventions, not source.** Immolate only
   models shop/pack generation, so it has no keys for in-round effects.
   Every remaining one is now centralised in `keys.hpp` under
   GAMEPLAY-EFFECT KEYS and individually marked `[SRC]` (verified) or
   `[GUESS]` (follows Balatro's snake_case convention but unconfirmed):
   glass breaks, Business Card, Bloodstone, Reserved Parking, Space Joker,
   8 Ball, Hallucination, the per-round rerollers (Ancient, Idol, Castle,
   To Do List, Mail-In Rebate, Certificate), Madness/Ankh/Hex/Invisible/
   Perkeo picks, and the boss effects (Hook, Crimson Heart, Cerulean Bell,
   The Wheel). **Per-key isolation means a wrong name here corrupts only
   that one effect** — it cannot desync shops, tags, vouchers or bosses.
   One file to edit when they're confirmed.
2. **Locked items aren't modelled.** The game resamples when it draws a
   locked (undiscovered) item, and Showman disables that. We assume
   everything unlocked, which is the right population for this experiment
   but changes resample counts versus a fresh profile.
3. **Voucher and tag effects lag their pools.** Both pools are now the full
   32/24 in game order, so the *rolls* are right, but many of the new
   entries have no implemented effect yet.

## What this buys you today

* **Runs are reproducible from an 8-char seed** using the real generator, so
  a winning run is identified by a real seed string.
* **Per-key isolation is preserved** under the real RNG — tested: two runs
  on seed `TUTORIAL` that play completely differently still get identical
  shops. Common random numbers works, which is the property the generational
  AI experiment actually needs.
* The remaining work is **bounded and incremental**: fix the pool ordering,
  then the shop loop, then the ad-hoc keys — each verifiable independently
  against a seed analyzer, because per-key isolation means one wrong
  subsystem doesn't corrupt the others.

## VERIFIED AGAINST REAL GAMEPLAY

Seed **QJSKRTWR** (Roffle's naneinf run, youtube.com/watch?v=hjxFsBP0fH4)
now matches the video on:

* opening hand contains **three Queens**
* ante 1 small-blind shop: **Dusk, Crafty Joker**
* ante 1 big-blind shop: **Strength, Drunkard**
* ante 1 boss: **The Hook**
* the shop after that boss: **Venus, Wrathful Joker**, packs **Arcana +
  Celestial Jumbo**
* ante 1 voucher: **Hone** (persists across all of the ante's shops)
* ante 1 skip tags: **Rare Tag**, **Holographic Tag**
* that Arcana pack's contents: **The Devil, The Star, The High Priestess**
* using that High Priestess: **Mercury + Jupiter**

Packs verified across all three shops of ante 1:

| Shop | Packs |
|---|---|
| after small blind | **Buffoon** (forced, free) + **Celestial** |
| after big blind | **Celestial** + **Celestial Mega** |
| after boss (= ante 2's first) | **Arcana** + **Celestial Jumbo** |

And that Arcana pack's contents: **The Devil, The Star, The High Priestess**.

Two consecutive shops matching means the key strings, composition order,
per-ante queue, pool orderings, 20/4/4 rates and rarity thresholds are all
correct together. These are golden tests in `tests.cpp` (`testRealSeed`).

### The ante-boundary bug this exposed

The post-boss shop showed the wrong items — and the reference called it
"ante 2's shop". That was the tell: **the ante advances when the boss is
DEFEATED, before the shop opens.** The shop you see after a boss is the
*first shop of the next ante*: it draws from that ante's queue and offers
that ante's voucher. So ante 1 supplies only two shops; every later ante
supplies three.

Confirmed by probing ante 2's queue directly — it opens Venus, Wrathful
Joker, exactly the reference's post-boss shop.

The per-ante roll order also comes straight from the analyzer and is now
implemented in `beginAnte()`:

    next_boss(ante) -> next_voucher(ante) -> next_tag(ante) x2
    -> next_orbital_tag() [a burned call] -> shop queue

Two consequences: the boss/voucher/tags are rolled at ante START (not when
each blind is reached), and the **two skip tags are consecutive draws from
one `Tag<ante>` key** — not resamples, which is what the engine did before.

### The pack-table bug this exposed

Packs are **one weighted roll each** (`randweightedchoice`) that picks type
AND size together from a 15-entry table totalling 22.42 weight — Arcana/
Celestial/Standard at 4/2/0.5 per size, Buffoon at 1.2/0.6/0.15, Spectral at
0.6/0.3/0.07 — with card counts and pick counts from `PACK_INFO`. The engine
had been making two separate rolls with a resample. Fixed; the post-boss
packs now match the reference exactly.

**The forced first Buffoon pack IS real** (user-confirmed, and it matches
Immolate's `generatedFirstPack`): the first pack offered in a run — ante <= 2,
once — is always a Buffoon Pack. Crucially it returns *before* the weighted
roll, so it **consumes no draw** from `shop_pack<ante>`. That detail is
independently confirmed by the reference: shop 2's packs are rolls #2 and
#3, i.e. Celestial + Celestial Mega, which only lines up if the Buffoon was
free.

### The lock bugs this exposed

Two pools are gated, and locked entries are **resampled past** (never
removed — removing them would shift every index):

* **Ante-gated tags** (`init_locks`): at ante < 2, Negative, Standard,
  Meteor, Buffoon, Handy, Garbage, Ethereal, Top-up and Orbital tags are
  locked. Ante 1's second tag rolled Meteor, which the game resamples away
  to Holographic — matching the reference. The same table independently
  confirms every boss `minAnte` value.
* **Secret consumables**: Planet X / Ceres / Eris stay locked until Five of
  a Kind / Flush House / Flush Five has been played that run, and The Soul /
  Black Hole never appear in normal pools. High Priestess was producing
  Mercury + *Ceres*; with the lock it gives Mercury + **Jupiter**, matching
  the reference.

### The creator-source bug this exposed

Every card-creating effect has its own SOURCE tag in the key — `pri` for
The High Priestess, `emp` for The Emperor, `jud` for Judgement, `wra` for
Wraith, `sou` for The Soul, plus `vag`, `sup`, `8ba`, `sea`, `sixth`, `car`,
`rif`, `rta`, `uta`, `blusl`. The engine had been using invented names, so
every created card came from the wrong stream. All 11 creators now use the
real source tags and draw randlist-style.

### The pack-contents bug this exposed

A pack's cards are **consecutive draws from ONE key** (`randlist`), with each
drawn item temporarily *locked* so later draws resample past it — that is
the game's duplicate exclusion, and it is why a pack never shows the same
tarot twice. The engine had been giving each card slot its own resample
stream, which both mismatched after card 1 and allowed duplicates (a run
showed "Devil, Tower, Devil").

`drawFromPool()` now mirrors `randchoice_common` + `randlist`: draw on the
base key, and while the result is already taken, retry on `_resample2`,
`_resample3`, ... Showman disables the exclusion. Applies to Arcana,
Celestial, Spectral and Buffoon packs alike.

### The deck-order bug this exposed

Shops matched before the draw did, which localised the fault immediately.
The game's `DECK_ORDER` is **Clubs, Diamonds, Hearts, Spades**, and within a
suit the ranks are in **alphabetical order of their symbol**:
`2 3 4 5 6 7 8 9 A J K Q T`. Ace sits after 9; Ten sits last. The engine had
been building spades-first with numeric ranks, so the shuffle was permuting
a differently-ordered array — a correctly-shuffled wrong deck.

Also added: **the hand is kept sorted** (rank descending). Not cosmetic —
The Hook, Cerulean Bell, Immolate and the spectral replacers pick a random
card *by hand index*, so hand order changes which card they hit.
⚠ Sort direction assumed descending; suit-sort mode not modelled.

## Confidence by subsystem

| Subsystem | Keys | Order/count | Pool | Confidence |
|---|---|---|---|---|
| Shop items | source | queue implemented | game order | **VERIFIED** |
| Joker rarity/edition | source | source thresholds | game order | **VERIFIED** |
| Vouchers | source | one per ante | 32, game order | **high** |
| Skip tags | source | one per blind | 24, game order | **high** |
| Boss blinds | source | fewest-appearances | 28, game order | **high** |
| Deck shuffle | source | per round | DECK_ORDER fixed | **VERIFIED** |
| Packs | source | weighted table + forced first | game order | **VERIFIED** |
| Standard cards | source | per card | n/a | medium — rates unverified |
| Created consumables | source | randlist | game order + locks | **VERIFIED** |
| Gameplay effects | **guessed** | per trigger | n/a | **low, isolated** |

## How to verify when you want to close it out

Pick a seed, open MathIsFun0's analyzer (The-Soul) or `balatrohq.com/tools/
seed-analyzer/`, and compare ante 1's shop against `play.cpp`'s. Then work
subsystem by subsystem: vouchers and tags are easiest (one roll each, no
pool ordering), the shop is hardest. Every fix should land as a golden test
with the seed and the expected contents hardcoded.

## Licensing note

`keys.hpp` and `balatro_rng.*` derive from Immolate (CC BY-NC-SA 4.0).
Keep the attribution headers, keep the project non-commercial, and if you
publish the repo, license the derived files the same way.
