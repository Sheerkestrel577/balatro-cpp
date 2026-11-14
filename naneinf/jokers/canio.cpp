// PATTERN: reacts to CARDS_DESTROYED, asking isFace AT THAT MOMENT — so
// Pareidolia (and Sixth Sense destroying a 6) feed it automatically with
// zero special-casing. Copy for: Glass Joker (counts glass instead).
#include "../balatro.hpp"

namespace {

class Canio : public Joker {
    double xmult_ = 1.0;
public:
    void interact(Balatro& g, int type, const EventData& ev, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.mult = g.scoring.mult * BigNum(xmult_);
            return;
        }
        if (type == CARDS_DESTROYED) {
            if (isCopy || !ev.cards) return;
            for (Card* c : *ev.cards)
                if (g.isFace(*c)) xmult_ += 1.0;          // ⚠ verify
        }
    }
};
REGISTER_JOKER(Canio, "Canio", 4, 20)

}  // namespace — internal linkage: no cross-file ODR collisions
