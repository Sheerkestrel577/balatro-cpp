// PATTERN: scaling counter with a reset condition. THE canonical isCopy
// case: a copy grants the CURRENT mult but must not advance the counter.
// Copy for: Green Joker, Square, Runner, Castle, Obelisk, Wee, Red Card,
// Hit the Road, Vampire, Constellation, Lucky Cat, Glass Joker, Campfire...
#include "../balatro.hpp"

namespace {

class FlashCard : public Joker {
    double mult_ = 0;
public:
    void interact(Balatro& g, int type, const EventData& ev, bool isCopy) override {
        if (type == INDEPENDENT) {
            // effect: applies for copies too (reads target's current state)
            g.scoring.mult = g.scoring.mult + BigNum(mult_);
            return;
        }
        if (type == SHOP_REROLL) {
            if (isCopy) return;              // <-- STATE CHANGES ARE GUARDED
            mult_+=2;
        }
    }
};
REGISTER_JOKER(FlashCard, "Flash Card", 2, 5)

}  // namespace — internal linkage: no cross-file ODR collisions
