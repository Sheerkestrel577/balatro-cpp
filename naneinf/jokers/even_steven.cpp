// Even Steven — the twin of Odd Todd. Even ranks: 10, 8, 6, 4, 2.
#include "../balatro.hpp"

namespace {

class EvenSteven : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_SCORED) return;
        int r = ev.card->rank;
        if (r == 2 || r == 4 || r == 6 || r == 8 || r == 10)
            g.scoring.mult = g.scoring.mult + BigNum(4);      // ⚠ verify
    }
};
REGISTER_JOKER(EvenSteven, "Even Steven", 1, 4)

}  // namespace — internal linkage: no cross-file ODR collisions
