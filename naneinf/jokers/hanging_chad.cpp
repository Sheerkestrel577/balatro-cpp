// Hack — RETRIG_QUERY_SCORED demo. Retrigger each played 2,3,4,5.
#include "../balatro.hpp"

namespace {

class HangingChad : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != RETRIG_QUERY_SCORED) return;
        if (ev.card == g.scoring.scored.front())
            g.scoring.retriggers+=2;
    }
};
REGISTER_JOKER(HangingChad, "Hanging Chad", 1, 4)

}  // namespace — internal linkage: no cross-file ODR collisions
