// Misprint — INDEPENDENT + keyed RNG demo. +0..23 mult.
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class Misprint : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        g.scoring.mult = g.scoring.mult
            + BigNum((double)g.rng->randint(keys::misprint(), 0, 23));
    }
};
REGISTER_JOKER(Misprint, "Misprint", 1, 4)

}  // namespace — internal linkage: no cross-file ODR collisions
