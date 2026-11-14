// PATTERN: economy joker with growth on a round-structure event.
// Copy for: Golden Joker, Delayed Gratification, Cloud 9, To Do List,
// Mail-In Rebate, Business Card (money on score), Satellite.
#include "../balatro.hpp"

namespace {

class Rocket : public Joker {
    int payout_ = 1;                                      // ⚠ verify
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == ROUND_END) { g.money += payout_; return; }
        if (type == BOSS_DEFEATED) { if (!isCopy) payout_ += 2; }
    }
};
REGISTER_JOKER(Rocket, "Rocket", 2, 6)

}  // namespace — internal linkage: no cross-file ODR collisions
