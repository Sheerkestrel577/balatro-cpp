// PATTERN: rewards a SHRINKING deck (below the starting 52).
#include "../balatro.hpp"

namespace {

class Erosion : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        int alive = 0;
        for (const Card& c : g.deck) if (!c.destroyed) ++alive;
        int missing = 52 - alive;
        if (missing > 0)
            g.scoring.mult = g.scoring.mult + BigNum(4.0 * missing);  // ⚠ verify
    }
};
REGISTER_JOKER(Erosion, "Erosion", 2, 6)

}  // namespace
