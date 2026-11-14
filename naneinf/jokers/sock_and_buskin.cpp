// PATTERN: retrigger query for scored cards, using an ask-time helper.
// (Pareidolia makes this retrigger everything — automatically.)
// Copy for: Hack (ranks), Dusk (last hand), Seltzer (counter), Hanging Chad
// (first scored card, +2). Mime = the held-card twin (RETRIG_QUERY_HELD).
#include "../balatro.hpp"

namespace {

class SockAndBuskin : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != RETRIG_QUERY_SCORED) return;
        if (g.isFace(*ev.card)) ++g.scoring.retriggers;
    }
};
REGISTER_JOKER(SockAndBuskin, "Sock and Buskin", 2, 6)

}  // namespace — internal linkage: no cross-file ODR collisions
