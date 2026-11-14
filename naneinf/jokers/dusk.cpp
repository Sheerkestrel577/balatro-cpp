// PATTERN: retrigger query gated on run state (the final hand).
#include "../balatro.hpp"

namespace {

class Dusk : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != RETRIG_QUERY_SCORED) return;
        if (g.handsLeft == 1) ++g.scoring.retriggers;   // last hand of round
    }
};
REGISTER_JOKER(Dusk, "Dusk", 2, 5)

}  // namespace
