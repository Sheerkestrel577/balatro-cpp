// PATTERN: money READ as score (the mirror of an econ joker).
#include "../balatro.hpp"

namespace {

class Bootstraps : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        g.scoring.mult = g.scoring.mult + BigNum(2.0 * (g.money / 5));  // ⚠
    }
};
REGISTER_JOKER(Bootstraps, "Bootstraps", 2, 7)

}  // namespace — internal linkage: no cross-file ODR collisions
