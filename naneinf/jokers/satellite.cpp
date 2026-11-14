// PATTERN: cash-out payout from a RUN COUNTER (unique planets used).
// The engine maintains uniquePlanetsUsed when a planet is consumed.
#include "../balatro.hpp"

namespace {

class Satellite : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != ROUND_END) return;
        g.addMoney((int)g.uniquePlanetsUsed);             // ⚠ verify
    }
};
REGISTER_JOKER(Satellite, "Satellite", 2, 6)

}  // namespace — internal linkage: no cross-file ODR collisions
