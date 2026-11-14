// PATTERN: reads a RUN COUNTER maintained by the engine.
#include "../balatro.hpp"

namespace {

class FortuneTeller : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        g.scoring.mult = g.scoring.mult + BigNum((double)g.tarotsUsed);
    }
};
REGISTER_JOKER(FortuneTeller, "Fortune Teller", 1, 6)

}  // namespace — internal linkage: no cross-file ODR collisions
