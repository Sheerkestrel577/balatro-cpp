// PATTERN: scaling counter with a reset condition. THE canonical isCopy
// case: a copy grants the CURRENT mult but must not advance the counter.
// Copy for: Green Joker, Square, Runner, Castle, Obelisk, Wee, Red Card,
// Hit the Road, Vampire, Constellation, Lucky Cat, Glass Joker, Campfire...
#include "../balatro.hpp"

namespace {

class RideTheBus : public Joker {
    double mult_ = 0;
public:
    void interact(Balatro& g, int type, const EventData& ev, bool isCopy) override {
        if (type == INDEPENDENT) {
            // effect: applies for copies too (reads target's current state)
            g.scoring.mult = g.scoring.mult + BigNum(mult_);
            return;
        }
        if (type == HAND_DETERMINED) {
            if (isCopy) return;              // <-- STATE CHANGES ARE GUARDED
            bool anyFace = false;
            for (Card* c : g.scoring.scored) if (g.isFace(*c)) anyFace = true;
            if (anyFace) mult_ = 0; else mult_ += 1;   // ⚠ verify numbers
        }
    }
};
REGISTER_JOKER(RideTheBus, "Ride the Bus", 1, 6)

}  // namespace — internal linkage: no cross-file ODR collisions
