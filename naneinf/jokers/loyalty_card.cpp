// PATTERN: every Nth hand. Counter + modulo, isCopy-guarded.
#include "../balatro.hpp"

namespace {

class LoyaltyCard : public Joker {
    int hands_ = 0;
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == INDEPENDENT) {
            if (hands_ > 0 && hands_ % 6 == 0)                    // ⚠ verify
                g.scoring.mult = g.scoring.mult * BigNum(4.0);
            return;
        }
        if (type == HAND_DETERMINED && !isCopy) ++hands_;
    }
};
REGISTER_JOKER(LoyaltyCard, "Loyalty Card", 2, 5)

}  // namespace
