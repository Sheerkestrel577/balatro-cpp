// PATTERN: legendary per-scored-card xmult.
#include "../balatro.hpp"

namespace {

class Triboulet : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_SCORED) return;
        if (ev.card->rank == 12 || ev.card->rank == 13)     // Q or K
            g.scoring.mult = g.scoring.mult * BigNum(2);    // ⚠ verify
    }
};
REGISTER_JOKER(Triboulet, "Triboulet", 4, 20)

}  // namespace
