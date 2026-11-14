// PATTERN: xmult if the hand CONTAINS a type (bitmask, not handType).
#include "../balatro.hpp"

namespace {

class TheTribe : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        if (g.scoring.containedHands & (1u << FLUSH))
            g.scoring.mult = g.scoring.mult * BigNum(2.0);   // ⚠ verify
    }
};
REGISTER_JOKER(TheTribe, "The Tribe", 3, 8)

}  // namespace
