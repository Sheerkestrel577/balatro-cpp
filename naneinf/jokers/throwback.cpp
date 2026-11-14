// PATTERN: reads a run counter (blinds skipped).
#include "../balatro.hpp"

namespace {

class Throwback : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        g.scoring.mult = g.scoring.mult
                       * BigNum(1.0 + 0.25 * g.blindsSkipped);   // ⚠ verify
    }
};
REGISTER_JOKER(Throwback, "Throwback", 2, 6)

}  // namespace
