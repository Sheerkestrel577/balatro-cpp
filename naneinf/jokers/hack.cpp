// Hack — RETRIG_QUERY_SCORED demo. Retrigger each played 2,3,4,5.
#include "../balatro.hpp"

namespace {

class Hack : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != RETRIG_QUERY_SCORED) return;
        if (ev.card->rank >= 2 && ev.card->rank <= 5)
            ++g.scoring.retriggers;
    }
};
REGISTER_JOKER(Hack, "Hack", 2, 6)

}  // namespace — internal linkage: no cross-file ODR collisions
