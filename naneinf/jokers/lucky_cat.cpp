// PATTERN: listens for a proc event the ENGINE raises (LUCKY_TRIGGERED).
#include "../balatro.hpp"

namespace {

class LuckyCat : public Joker {
    double xmult_ = 1.0;
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.mult = g.scoring.mult * BigNum(xmult_);
            return;
        }
        if (type == LUCKY_TRIGGERED && !isCopy) xmult_ += 0.25;   // ⚠ verify
    }
};
REGISTER_JOKER(LuckyCat, "Lucky Cat", 2, 6)

}  // namespace — internal linkage: no cross-file ODR collisions
