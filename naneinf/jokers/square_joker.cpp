// PATTERN: accumulates on a played-card-count condition.
#include "../balatro.hpp"

namespace {

class SquareJoker : public Joker {
    double chips_ = 0;
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.chips = g.scoring.chips + BigNum(chips_);
            return;
        }
        if (type != HAND_DETERMINED || isCopy) return;
        if (g.scoring.played.size() == 4) chips_ += 4;    // ⚠ verify
    }
};
REGISTER_JOKER(SquareJoker, "Square Joker", 1, 4)

}  // namespace
