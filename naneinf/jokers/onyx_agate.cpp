// Greedy Joker — CARD_SCORED + suit-helper demo. +3 mult per diamond.
// Note: routes through suitMatches => Wild/Smeared/debuff correct for free.
#include "../balatro.hpp"

namespace {

class OnyxAgate : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_SCORED) return;
        if (g.suitMatches(*ev.card, CLUBS))
            g.scoring.mult = g.scoring.mult + BigNum(7);
    }
};
REGISTER_JOKER(OnyxAgate, "Onyx Agate", 2, 7)

}  // namespace — internal linkage: no cross-file ODR collisions
