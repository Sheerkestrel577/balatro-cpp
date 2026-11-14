// PATTERN: rank check. Odd ranks in Balatro are A, 9, 7, 5, 3 — and in
// card.hpp an Ace is rank 14 (11 is a Jack, which is NOT odd).
#include "../balatro.hpp"

namespace {

class OddTodd : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_SCORED) return;
        int r = ev.card->rank;
        if (r == 14 || r == 3 || r == 5 || r == 7 || r == 9)
            g.scoring.chips = g.scoring.chips + BigNum(31);   // ⚠ verify
    }
};
REGISTER_JOKER(OddTodd, "Odd Todd", 1, 4)

}  // namespace — internal linkage: no cross-file ODR collisions
