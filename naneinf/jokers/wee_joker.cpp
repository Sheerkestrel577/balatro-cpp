// PATTERN: scaling counter with a reset condition. THE canonical isCopy
// case: a copy grants the CURRENT mult but must not advance the counter.
// Copy for: Green Joker, Square, Runner, Castle, Obelisk, Wee, Red Card,
// Hit the Road, Vampire, Constellation, Lucky Cat, Glass Joker, Campfire...
#include "../balatro.hpp"

namespace {

class WeeJoker : public Joker {
    double chips_ = 0;
public:
    void interact(Balatro& g, int type, const EventData& ev, bool isCopy) override {
        if (type == INDEPENDENT) {
            // effect: applies for copies too (reads target's current state)
            g.scoring.chips = g.scoring.chips + BigNum(chips_);
            return;
        }
        if (type == CARD_SCORED) {
            if (isCopy) return;
            int r = ev.card->rank;
            if (r == 2)
                chips_+=8;
        }
    }
};
REGISTER_JOKER(WeeJoker, "Wee Joker", 3, 8)

}  // namespace — internal linkage: no cross-file ODR collisions
