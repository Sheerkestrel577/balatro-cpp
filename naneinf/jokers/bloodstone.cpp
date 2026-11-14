// PATTERN: per-scored-card probability. NOT isCopy-guarded: a copy rolls
// its own chance (spec §1).
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class Bloodstone : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_SCORED) return;
        if (!g.suitMatches(*ev.card, HEARTS)) return;
        if (g.chance(keys::kBloodstone(), 1, 2))
            g.scoring.mult = g.scoring.mult * BigNum(1.5);   // ⚠ verify
    }
};
REGISTER_JOKER(Bloodstone, "Bloodstone", 2, 7)

}  // namespace — internal linkage: no cross-file ODR collisions
