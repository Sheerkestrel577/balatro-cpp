// PATTERN: xmult if the hand CONTAINS a type (bitmask, not handType).
#include "../balatro.hpp"

namespace {

class TheTrio : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        if (g.scoring.containedHands & (1u << THREE_OAK))
            g.scoring.mult = g.scoring.mult * BigNum(3.0);   // ⚠ verify
    }
};
REGISTER_JOKER(TheTrio, "The Trio", 3, 8)

}  // namespace
