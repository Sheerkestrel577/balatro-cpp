// PATTERN: reads a per-hand-type run counter the engine maintains.
#include "../balatro.hpp"

namespace {

class Supernova : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        g.scoring.mult = g.scoring.mult
                       + BigNum((double)g.handPlayCount[g.scoring.handType]);
    }
};
REGISTER_JOKER(Supernova, "Supernova", 1, 5)

}  // namespace
