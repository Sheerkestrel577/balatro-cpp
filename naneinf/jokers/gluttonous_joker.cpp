// Greedy Joker — CARD_SCORED + suit-helper demo. +3 mult per diamond.
// Note: routes through suitMatches => Wild/Smeared/debuff correct for free.
#include "../balatro.hpp"

namespace {

class GluttonousJoker : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_SCORED) return;
        if (g.suitMatches(*ev.card, CLUBS))
            g.scoring.mult = g.scoring.mult + BigNum(3);
    }
};
REGISTER_JOKER(GluttonousJoker, "Gluttonous Joker", 1, 5)

}  // namespace — internal linkage: no cross-file ODR collisions
