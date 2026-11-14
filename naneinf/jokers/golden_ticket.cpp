// PATTERN: money on scored cards with a given ENHANCEMENT.
#include "../balatro.hpp"

namespace {

class GoldenTicket : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_SCORED) return;
        if (ev.card->enhancement == ENH_GOLD) g.addMoney(4);   // ⚠ verify
    }
};
REGISTER_JOKER(GoldenTicket, "Golden Ticket", 1, 5)

}  // namespace — internal linkage: no cross-file ODR collisions
