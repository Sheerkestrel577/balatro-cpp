// PATTERN: held-in-hand, per card. Fires once per held card per trigger
// (Mime/red seal retriggers multiply it automatically).
// Copy for: Raised Fist, Shoot the Moon, Reserved Parking.
#include "../balatro.hpp"

namespace {

class Baron : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_HELD) return;
        if (ev.card->rank == 13)                          // King
            g.scoring.mult = g.scoring.mult * BigNum(1.5); // ⚠ verify
    }
};
REGISTER_JOKER(Baron, "Baron", 3, 8)

}  // namespace — internal linkage: no cross-file ODR collisions
