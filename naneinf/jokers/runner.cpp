// PATTERN: accumulates chips when a hand type appears.
#include "../balatro.hpp"

namespace {

class Runner : public Joker {
    double chips_ = 0;
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.chips = g.scoring.chips + BigNum(chips_);
            return;
        }
        if (type != HAND_DETERMINED || isCopy) return;
        if (g.scoring.containedHands & (1u << STRAIGHT)) chips_ += 15;  // ⚠
    }
};
REGISTER_JOKER(Runner, "Runner", 1, 5)

}  // namespace
