// Greedy Joker — CARD_SCORED + suit-helper demo. +3 mult per diamond.
// Note: routes through suitMatches => Wild/Smeared/debuff correct for free.
#include "../balatro.hpp"

namespace {

class LustyJoker : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_SCORED) return;
        if (g.suitMatches(*ev.card, HEARTS))
            g.scoring.mult = g.scoring.mult + BigNum(3);
    }
};
REGISTER_JOKER(LustyJoker, "Lusty Joker", 1, 5)

}  // namespace — internal linkage: no cross-file ODR collisions
