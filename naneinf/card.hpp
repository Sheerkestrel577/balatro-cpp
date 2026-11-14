// card.hpp — a Card stores FACTS only (spec §5 "Facts vs interpretations").
// What a card COUNTS AS is computed by Balatro's helpers, never stored here.
#pragma once
#include <cstdint>

enum Suit { SPADES = 0, HEARTS = 1, CLUBS = 2, DIAMONDS = 3 };

enum Enhancement {
    ENH_NONE, ENH_BONUS, ENH_MULT, ENH_WILD, ENH_GLASS,
    ENH_STEEL, ENH_STONE, ENH_GOLD, ENH_LUCKY
};

enum Edition { ED_NONE, ED_FOIL, ED_HOLO, ED_POLY, ED_NEGATIVE };

enum Seal { SEAL_NONE, SEAL_RED, SEAL_BLUE, SEAL_GOLD, SEAL_PURPLE };

// WARNING: `id` is the first member, so aggregate brace-init like
// Card{9, SPADES} sets id=9 and rank=SPADES. Always assign fields by name
// (or use a helper) when constructing cards.
struct Card {
    // Stable identity. Cards live in a std::deque (pointers survive growth)
    // and everything else refers to them by id or by Card*. Required before
    // consumables/packs/DNA can add cards mid-run.
    int id = -1;
    int rank = 2;                 // 2..14 (11=J, 12=Q, 13=K, 14=A)
    int suit = SPADES;
    int enhancement = ENH_NONE;   // changed only by things that change what
    int edition = ED_NONE;        //   a card IS (tarots, packs, Midas...)
    int seal = SEAL_NONE;
    bool debuffed = false;        // set by engine per boss rules
    bool faceDown = false;        // The House / Wheel / Fish / Mark
    bool playedThisAnte = false;  // The Pillar
    bool destroyed = false;       // set by the destruction sweep
    int bonusChips = 0;           // permanent additions (Hiker)

    // base chip value of the rank (stone handled in the pipeline)
    int rankChips() const {
        if (rank == 14) return 11;
        if (rank >= 11) return 10;
        return rank;
    }
};
