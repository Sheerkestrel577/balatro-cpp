// PATTERN: xmult if the hand CONTAINS a type (bitmask, not handType).
#include "../balatro.hpp"

namespace {

class TheFamily : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        if (g.scoring.containedHands & (1u << FOUR_OAK))
            g.scoring.mult = g.scoring.mult * BigNum(4.0);   // ⚠ verify
    }
};
REGISTER_JOKER(TheFamily, "The Family", 3, 8)

}  // namespace
