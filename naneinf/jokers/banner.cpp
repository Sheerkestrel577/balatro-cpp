// PATTERN: reads live run state at trigger time. User-confirmed: Bull sees
// money earned EARLIER IN THIS SAME HAND (gold seals, Rough Gem), because
// the engine commits money instantly.
// Copy for: Fortune Teller (g.tarotsUsed), Constellation, Stuntman, Swashbuckler.
#include "../balatro.hpp"

namespace {

class Banner : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        g.scoring.chips = g.scoring.chips + BigNum(30.0 * g.discardsLeft);  // ⚠ verify
    }
};
REGISTER_JOKER(Banner, "Banner", 1, 5)

}  // namespace — internal linkage: no cross-file ODR collisions
