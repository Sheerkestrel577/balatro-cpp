// PATTERN: flat mult. The simplest possible joker — copy this for every
// "+N Mult" / "+N Chips" joker.
#include "../balatro.hpp"

namespace {

class JokerBasic : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        g.scoring.mult = g.scoring.mult + BigNum(4);   // ⚠ verify numbers
    }
};
REGISTER_JOKER(JokerBasic, "Joker", 1, 2)

}  // namespace — internal linkage: no cross-file ODR collisions
