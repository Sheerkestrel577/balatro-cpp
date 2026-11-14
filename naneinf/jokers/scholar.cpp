// PATTERN: rank check. Odd ranks in Balatro are A, 9, 7, 5, 3 — and in
// card.hpp an Ace is rank 14 (11 is a Jack, which is NOT odd).
#include "../balatro.hpp"

namespace {

class Scholar : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_SCORED) return;
        int r = ev.card->rank;
        if (r == 14) {          // BRACES REQUIRED: without them the chips
            g.scoring.mult = g.scoring.mult + BigNum(4);    // line runs for
            g.scoring.chips = g.scoring.chips + BigNum(20); // EVERY card.
        }
    }
};
REGISTER_JOKER(Scholar, "Scholar", 1, 4)

}  // namespace — internal linkage: no cross-file ODR collisions
