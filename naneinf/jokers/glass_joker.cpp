// PATTERN: same as Canio but keyed on the enhancement of destroyed cards.
#include "../balatro.hpp"

namespace {

class GlassJoker : public Joker {
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
                if (c->enhancement == ENH_GLASS) xmult_ += 0.75;  // ⚠ verify
        }
    }
};
REGISTER_JOKER(GlassJoker, "Glass Joker", 2, 6)

}  // namespace — internal linkage: no cross-file ODR collisions
