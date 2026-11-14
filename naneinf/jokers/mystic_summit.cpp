// PATTERN: reads live run state at trigger time. User-confirmed: Bull sees
// money earned EARLIER IN THIS SAME HAND (gold seals, Rough Gem), because
// the engine commits money instantly.
// Copy for: Fortune Teller (g.tarotsUsed), Constellation, Stuntman, Swashbuckler.
#include "../balatro.hpp"

namespace {

class MysticSummit : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        if (g.discardsLeft==0)
            g.scoring.mult = g.scoring.mult + BigNum(15);  // ⚠ verify
    }
};
REGISTER_JOKER(MysticSummit, "Mystic Summit", 1, 5)

}  // namespace — internal linkage: no cross-file ODR collisions
