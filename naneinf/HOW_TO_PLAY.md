# Playing the engine by hand

Two prebuilt binaries are in this folder:

* **`naneinf.exe`** — Windows x64, statically linked (no DLLs needed).
  Double-click, or run from a terminal so you can see output:
  `.\naneinf.exe`
* **`naneinf-linux`** — Linux x64. `chmod +x naneinf-linux && ./naneinf-linux`

Or build it yourself: `g++ -std=c++20 -O2 play.cpp balatro.cpp blinds.cpp
balatro_rng.cpp hand_eval.cpp jokers/*.cpp consumables/*.cpp -o play`

## What you're playing
White Stake Plasma, seeded (it asks for a seed at startup — the same seed
always gives the same run). Everything goes through the same action
interface the AI will use, so if you can do it here, the AI can do it.

## Commands
```
c 0 1 2      toggle cards 0,1,2 in hand      j 1     select joker 1
u 0          select consumable 0             clear   clear selection
p            play selected                   d       discard selected
use          use selected consumable         sell    sell selected joker
< >          shift selected card             jl jr   shift selected joker
s            select blind                    skip    skip blind (take tag)
rerollboss   reroll boss (needs voucher)     hold    hold joker (Amber Acorn)
ok           confirm cash out
buy 0        buy shop slot 0                 pack 0  buy booster pack 0
voucher      buy the voucher                 reroll  reroll the shop
pick 2       take pack card 2                skippack  leave the pack
leave        leave the shop
legal        list every legal action id      state   reprint the board
help  quit
```

## Reading the board
* `10D` = ten of diamonds. `(X)` = debuffed. `[red]` = red seal.
  `GLASS`, `STEEL`, `GOLD` etc. = enhancement. `POLY`, `foil` = edition.
* `[?? ]` = face down (The House / Fish / Mark / Wheel). You genuinely
  cannot see it — the engine hides it from the player the same way it will
  hide it from the AI's observation.
* Selected things are marked `*`.
* Beating the blind automatically opens the cash-out screen; type `ok` to
  bank it and enter the shop.

## Things worth trying
* Skip a blind and watch the tag fire when you next enter the shop.
* Buy a Buffoon pack, then `pick` a joker.
* Get to ante 1's boss — check whether it's a debuff boss and watch `(X)`
  appear on the affected cards.
* `legal` at any point dumps the exact legal-action mask the AI sees.

## Known gaps (v11)
Only 3 consumables exist (The Hermit, Strength, Death), so Arcana packs and
shop consumable slots repeat a lot. 83 of 150 jokers are implemented.
Vouchers past the few wired into `applyVouchers()` are inert.
