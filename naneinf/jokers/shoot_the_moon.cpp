// PATTERN: held-in-hand, per card. Fires once per held card per trigger
// (Mime/red seal retriggers multiply it automatically).
// Copy for: Raised Fist, Shoot the Moon, Reserved Parking.
#include "../balatro.hpp"

namespace {

class ShootTheMoon : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_HELD) return;
        if (ev.card->rank == 12)                          // QUEEN (12), not King
            g.scoring.mult = g.scoring.mult + BigNum(13); // ⚠ verify
    }
};
REGISTER_JOKER(ShootTheMoon, "Shoot the Moon", 1, 5)

}  // namespace — internal linkage: no cross-file ODR collisions
