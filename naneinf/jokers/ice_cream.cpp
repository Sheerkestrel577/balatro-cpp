// PATTERN: decays each hand at HAND_SCORED (the "cards fly away" moment,
// user-confirmed), self-destructs at zero.
// Copy for: Popcorn (decays per ROUND_END), Ramen (per CARD_DISCARDED).
#include "../balatro.hpp"

namespace {

class IceCream : public Joker {
    double chips_ = 100;                                  // ⚠ verify
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.chips = g.scoring.chips + BigNum(chips_);
            return;
        }
        if (type == HAND_SCORED) {
            if (isCopy) return;
            chips_ -= 5;
            if (chips_ <= 0) g.queueDestroyJoker(*this);
        }
    }
};
REGISTER_JOKER(IceCream, "Ice Cream", 1, 5)

}  // namespace — internal linkage: no cross-file ODR collisions
