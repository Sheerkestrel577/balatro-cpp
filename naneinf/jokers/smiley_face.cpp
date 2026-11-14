// Greedy Joker — CARD_SCORED + suit-helper demo. +3 mult per diamond.
// Note: routes through suitMatches => Wild/Smeared/debuff correct for free.
#include "../balatro.hpp"

namespace {

class SmileyFace : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_SCORED) return;
        if (g.isFace(*ev.card))
            g.scoring.mult = g.scoring.mult + BigNum(5);
    }
};
REGISTER_JOKER(SmileyFace, "Smiley Face", 1, 4)

}  // namespace — internal linkage: no cross-file ODR collisions
