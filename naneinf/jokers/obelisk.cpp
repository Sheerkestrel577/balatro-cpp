// PATTERN: scales while you avoid your most-played hand.
#include "../balatro.hpp"

namespace {

class Obelisk : public Joker {
    double xmult_ = 1.0;
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.mult = g.scoring.mult * BigNum(xmult_);
            return;
        }
        if (type != HAND_DETERMINED || isCopy) return;
        if (g.scoring.handType == g.mostPlayedHand()) xmult_ = 1.0;
        else xmult_ += 0.2;                              // ⚠ verify
    }
};
REGISTER_JOKER(Obelisk, "Obelisk", 3, 8)

}  // namespace
