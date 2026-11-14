// PATTERN: conditional xmult that inspects the HELD cards through the suit
// helper (so Wild/Smeared/debuff are handled for free).
// Copy for: Flower Pot (uses g.suitCoverPossible on scored cards).
#include "../balatro.hpp"

namespace {

class Blackboard : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        for (Card* c : g.scoring.held) {
            uint8_t m = g.suitMask(*c);
            bool dark = (m & ((1u << SPADES) | (1u << CLUBS))) != 0;
            if (!dark) return;              // a red card -> no bonus
        }
        g.scoring.mult = g.scoring.mult * BigNum(3);      // ⚠ verify
    }
};
REGISTER_JOKER(Blackboard, "Blackboard", 2, 6)

}  // namespace — internal linkage: no cross-file ODR collisions
