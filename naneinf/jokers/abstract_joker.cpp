// PATTERN: reads live run state at trigger time. User-confirmed: Bull sees
// money earned EARLIER IN THIS SAME HAND (gold seals, Rough Gem), because
// the engine commits money instantly.
// Copy for: Fortune Teller (g.tarotsUsed), Constellation, Stuntman, Swashbuckler.
#include "../balatro.hpp"

namespace {

class AbstractJoker : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        g.scoring.mult = g.scoring.mult + BigNum(3.0 * g.jokers.size());  // ⚠ verify
    }
};
REGISTER_JOKER(AbstractJoker, "Abstract Joker", 1, 4)

}  // namespace — internal linkage: no cross-file ODR collisions
