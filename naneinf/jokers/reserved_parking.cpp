// PATTERN: money from HELD cards (chance per held face card).
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class ReservedParking : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_HELD) return;
        if (g.isFace(*ev.card) && g.chance(keys::kReservedParking(), 1, 2))
            g.addMoney(1);                                // ⚠ verify
    }
};
REGISTER_JOKER(ReservedParking, "Reserved Parking", 1, 6)

}  // namespace — internal linkage: no cross-file ODR collisions
