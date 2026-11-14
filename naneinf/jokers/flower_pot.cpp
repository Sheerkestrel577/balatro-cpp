// PATTERN: uses the engine's suit-cover solver.
#include "../balatro.hpp"

namespace {

class FlowerPot : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        if (g.suitCoverPossible(g.scoring.scored))
            g.scoring.mult = g.scoring.mult * BigNum(3.0);   // ⚠ verify
    }
};
REGISTER_JOKER(FlowerPot, "Flower Pot", 2, 6)

}  // namespace
