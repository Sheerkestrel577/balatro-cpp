// PATTERN: "the first scored <X>" — identify the CARD, never a "used" flag.
// Retriggers of that card must fire again (photochad). A flag would eat the
// retrigger, which is why this joker is stateless.
#include "../balatro.hpp"

namespace {

class Photograph : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_SCORED) return;
        for (Card* c : g.scoring.scored) {
            if (!g.isFace(*c)) continue;      // find the first face card
            if (c == ev.card)                 // ...is it the one triggering?
                g.scoring.mult = g.scoring.mult * BigNum(2);   // ⚠ verify
            return;                           // only the first one matters
        }
    }
};
REGISTER_JOKER(Photograph, "Photograph", 1, 5)

}  // namespace — internal linkage: no cross-file ODR collisions
