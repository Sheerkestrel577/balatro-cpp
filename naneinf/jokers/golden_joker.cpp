// PATTERN: the plain cash-out econ joker. Subscribe to ROUND_END, add money.
// That is ALL "adds to the cash out screen" means — the engine's cashOut()
// broadcasts ROUND_END at the right point (after hands bonus, before
// interest), so this payout counts toward interest automatically.
#include "../balatro.hpp"

namespace {

class GoldenJoker : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != ROUND_END) return;
        g.addMoney(4);                                    // ⚠ verify
    }
};
REGISTER_JOKER(GoldenJoker, "Golden Joker", 1, 6)

}  // namespace — internal linkage: no cross-file ODR collisions
