// PATTERN: chance roll + self-destruction. Probability rolls are NOT
// isCopy-guarded (a copy rolls its own), but destroying yourself IS.
// Destruction goes through the queue — never erase during a broadcast.
// Copy for: Cavendish, Popcorn/Ice Cream (decay-to-death), Invisible Joker.
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class GrosMichel : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.mult = g.scoring.mult + BigNum(15);   // ⚠ verify
            return;
        }
        if (type == ROUND_END) {
            if (g.chance(keys::grosMichel(), 1, 6) && !isCopy) {
                g.queueDestroyJoker(*this);   // swept after the broadcast
                // card.lua ~L3037: extinction sets the pool flag, which
                // drops Gros Michel from every pool and lets Cavendish in.
                // Set here (not in the sweep) so it only fires on the
                // EXTINCTION path — selling or eating a Gros Michel does
                // not make the species extinct.
                g.grosMichelExtinct = true;
            }
        }
    }
};
REGISTER_JOKER(GrosMichel, "Gros Michel", 1, 5)

}  // namespace — internal linkage: no cross-file ODR collisions
