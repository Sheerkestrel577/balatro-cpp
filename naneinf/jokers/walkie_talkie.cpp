// PATTERN: rank check. Odd ranks in Balatro are A, 9, 7, 5, 3 — and in
// card.hpp an Ace is rank 14 (11 is a Jack, which is NOT odd).
#include "../balatro.hpp"

namespace {

class WalkieTalkie : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_SCORED) return;
        int r = ev.card->rank;
        if (r == 10 || r == 4) {        // BRACES REQUIRED (see scholar.cpp)
            g.scoring.chips = g.scoring.chips + BigNum(10);
            g.scoring.mult = g.scoring.mult + BigNum(4);
        }
    }
};
REGISTER_JOKER(WalkieTalkie, "Walkie Talkie", 1, 4)

}  // namespace — internal linkage: no cross-file ODR collisions
