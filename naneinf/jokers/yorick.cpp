// PATTERN: scaling counter with a reset condition. THE canonical isCopy
// case: a copy grants the CURRENT mult but must not advance the counter.
// Copy for: Green Joker, Square, Runner, Castle, Obelisk, Wee, Red Card,
// Hit the Road, Vampire, Constellation, Lucky Cat, Glass Joker, Campfire...
#include "../balatro.hpp"

namespace {

class Yorick : public Joker {
    double mult_ = 1;
    int counter_ = 0;
public:
    void interact(Balatro& g, int type, const EventData& ev, bool isCopy) override {
        if (type == INDEPENDENT) {
            // effect: applies for copies too (reads target's current state)
            g.scoring.mult = g.scoring.mult * BigNum(mult_);
            return;
        }
        if (type == CARD_DISCARDED) {
            if (isCopy) return;              // <-- STATE CHANGES ARE GUARDED
            counter_+=1;
            if(counter_==23){
                counter_=0;
                mult_++;
            }
        }
    }
};
REGISTER_JOKER(Yorick, "Yorick", 4, 20)

}  // namespace — internal linkage: no cross-file ODR collisions
