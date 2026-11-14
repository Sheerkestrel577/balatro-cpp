// PATTERN: scales on CARD_SOLD, resets on BOSS_DEFEATED.
#include "../balatro.hpp"

namespace {

class Campfire : public Joker {
    double xmult_ = 1.0;
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.mult = g.scoring.mult * BigNum(xmult_);
            return;
        }
        if (isCopy) return;
        if (type == CARD_SOLD)      xmult_ += 0.25;      // ⚠ verify
        if (type == BOSS_DEFEATED)  xmult_ = 1.0;
    }
};
REGISTER_JOKER(Campfire, "Campfire", 3, 9)

}  // namespace
