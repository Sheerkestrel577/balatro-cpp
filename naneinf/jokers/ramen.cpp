// PATTERN: decays per DISCARDED CARD (user-confirmed timing).
#include "../balatro.hpp"

namespace {

class Ramen : public Joker {
    double xmult_ = 2.0;
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.mult = g.scoring.mult * BigNum(xmult_);
            return;
        }
        if (type != CARD_DISCARDED || isCopy) return;
        xmult_ -= 0.01;                                      // ⚠ verify
        if (xmult_ <= 1.0) g.queueDestroyJoker(*this);
    }
};
REGISTER_JOKER(Ramen, "Ramen", 2, 6)

}  // namespace
