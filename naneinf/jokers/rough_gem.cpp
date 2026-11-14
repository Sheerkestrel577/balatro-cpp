// Greedy Joker — CARD_SCORED + suit-helper demo. +3 mult per diamond.
// Note: routes through suitMatches => Wild/Smeared/debuff correct for free.
#include "../balatro.hpp"

namespace {

class RoughGem : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_SCORED) return;
        if (g.suitMatches(*ev.card, DIAMONDS))
            g.money = g.money+1;
    }
};
REGISTER_JOKER(RoughGem, "Rough Gem", 2, 7)

}  // namespace — internal linkage: no cross-file ODR collisions
