// PATTERN: SHOP_EXITED hook. Negative copy of a held consumable.
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class Perkeo : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type != SHOP_EXITED || isCopy) return;
        if (g.consumables.empty()) return;
        int i = g.rng->randint(keys::kPerkeo(),
                               0, (int)g.consumables.size() - 1);
        g.copyConsumable(i, ED_NEGATIVE);   // negative: ignores slot limits
    }
};
REGISTER_JOKER(Perkeo, "Perkeo", 4, 20)

}  // namespace
