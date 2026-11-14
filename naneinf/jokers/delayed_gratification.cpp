// PATTERN: conditional cash-out payout — needs per-round tracking the
// engine resets in cashOut() (discardsUsedThisRound).
#include "../balatro.hpp"

namespace {

class DelayedGratification : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != ROUND_END) return;
        if (g.discardsUsedThisRound > 0) return;
        g.addMoney(2 * g.discardsLeft);                   // ⚠ verify
    }
};
REGISTER_JOKER(DelayedGratification, "Delayed Gratification", 1, 4)

}  // namespace — internal linkage: no cross-file ODR collisions
