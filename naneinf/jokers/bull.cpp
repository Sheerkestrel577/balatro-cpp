// PATTERN: reads live run state at trigger time. User-confirmed: Bull sees
// money earned EARLIER IN THIS SAME HAND (gold seals, Rough Gem), because
// the engine commits money instantly.
// Copy for: Fortune Teller (g.tarotsUsed), Constellation, Stuntman, Swashbuckler.
#include "../balatro.hpp"

namespace {

class Bull : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        g.scoring.chips = g.scoring.chips + BigNum(2.0 * g.money);  // ⚠ verify
    }
};
REGISTER_JOKER(Bull, "Bull", 2, 6)

}  // namespace — internal linkage: no cross-file ODR collisions
