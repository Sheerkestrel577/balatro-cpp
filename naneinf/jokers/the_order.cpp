// PATTERN: xmult if the hand CONTAINS a type (bitmask, not handType).
#include "../balatro.hpp"

namespace {

class TheOrder : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        if (g.scoring.containedHands & (1u << STRAIGHT))
            g.scoring.mult = g.scoring.mult * BigNum(3.0);   // ⚠ verify
    }
};
REGISTER_JOKER(TheOrder, "The Order", 3, 8)

}  // namespace
