// hand_eval.hpp — 12-type hand evaluation with "contains" semantics
// (spec §4 step 1; user-verified via the 4oaK-flush test).
#pragma once
#include <cstdint>
#include <vector>

#include "card.hpp"

struct PassiveMods;

// Ascending strength; also indexes the level table.
enum HandType {
    HIGH_CARD = 0, PAIR, TWO_PAIR, THREE_OAK, STRAIGHT, FLUSH,
    FULL_HOUSE, FOUR_OAK, STRAIGHT_FLUSH, FIVE_OAK, FLUSH_HOUSE,
    FLUSH_FIVE, NUM_HAND_TYPES
};
// (Royal Flush is a display alias of STRAIGHT_FLUSH, not a 13th type.)

extern const char* HAND_NAMES[NUM_HAND_TYPES];

struct HandEvalResult {
    uint16_t containedHands = 0;      // bit i set <=> HandType i present.
                                      // ALL "contains a ..." jokers read
                                      // this, never scoredType.
    int scoredType = HIGH_CARD;       // highest contained
    std::vector<int> scoringIdx;      // indices into `played` that score
                                      // (constituents + Stone always
                                      //  + everything under Splash)
    bool contains(HandType t) const { return containedHands & (1u << t); }
};

// `played`: 1..5 cards. Duplicate identical cards are legal (Death etc).
// Uses mods for Four Fingers / Shortcut / Smeared. Debuffed cards still
// participate in determination (they only skip their scoring effects).
HandEvalResult evaluateHand(const std::vector<Card*>& played,
                            const PassiveMods& mods);

// Determination-time suit mask (ignores debuff on purpose):
// bit per Suit; Wild = all four; Stone = none; Smeared merges H<->D, S<->C.
uint8_t detSuitMask(const Card& c, const PassiveMods& mods);
