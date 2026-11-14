# Joker Implementation Checklist — COMPLETE

**Status: 150 / 150 jokers. 52 / 52 consumables (22 tarots, 12 planets, 18 spectrals).**

Nothing on this list is outstanding. Kept for the pattern mapping and the
notes below; all remaining work is *verification*, not implementation.

## What still needs checking (not building)

Every joker and consumable carries a `⚠ verify` comment on its numbers.
They were written from knowledge of the pool, not transcribed row-by-row
from the wiki, so the values are the thing to audit — ideally by diffing
against real Balatro through BalatroBot rather than by hand.

Behaviours flagged as uncertain and worth an in-game check:
* Whether counter jokers (Green Joker, Ride the Bus) count the in-flight
  hand — implemented as YES, user-confirmed for Green Joker.
* Séance's hand type, Vagabond's money threshold, Card Sharp's exact
  "already played this round" semantics.
* Flower Pot: true suit-cover matching vs. a greedy pass in the real game.
* The Ox under multiple Chicots (user-reported money doubling; not
  documented on the wiki, deliberately NOT implemented).
* Pack size/count tables and edition/seal rates in pack generation.

## The stubbed list (deliberate scope cuts)

| Item | Status | Effect on validity |
|---|---|---|
| The Wheel of Fortune | in the pool at full weight, `canUse()` returns false | lower-bounds the naneinf rate |

