// PATTERN: chance roll + self-destruction. Probability rolls are NOT
// isCopy-guarded (a copy rolls its own), but destroying yourself IS.
// Destruction goes through the queue — never erase during a broadcast.
// Copy for: Cavendish, Popcorn/Ice Cream (decay-to-death), Invisible Joker.
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class Cavendish : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.mult = g.scoring.mult * BigNum(3.0);   // ⚠ verify
            return;
        }
        if (type == ROUND_END) {
            if (g.chance(keys::cavendish(), 1, 1000) && !isCopy)
                g.queueDestroyJoker(*this);   // swept after the broadcast
        }
    }
};
REGISTER_JOKER(Cavendish, "Cavendish", 1, 4)

}  // namespace — internal linkage: no cross-file ODR collisions
