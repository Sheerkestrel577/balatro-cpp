// PATTERN: money DURING scoring (not at cash out). Commits instantly, so
// Bull sees it this same hand (user-confirmed).
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class BusinessCard : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_SCORED) return;
        if (g.isFace(*ev.card) && g.chance(keys::kBusinessCard(), 1, 2))
            g.addMoney(2);                                // ⚠ verify
    }
};
REGISTER_JOKER(BusinessCard, "Business Card", 1, 4)

}  // namespace — internal linkage: no cross-file ODR collisions
