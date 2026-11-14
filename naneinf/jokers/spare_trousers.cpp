// PATTERN: scales when the hand CONTAINS two pair.
#include "../balatro.hpp"

namespace {

class SpareTrousers : public Joker {
    double mult_ = 0;
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.mult = g.scoring.mult + BigNum(mult_);
            return;
        }
        if (type != HAND_DETERMINED || isCopy) return;
        if (g.scoring.containedHands & (1u << TWO_PAIR)) mult_ += 2;   // ⚠
    }
};
REGISTER_JOKER(SpareTrousers, "Spare Trousers", 2, 6)

}  // namespace
