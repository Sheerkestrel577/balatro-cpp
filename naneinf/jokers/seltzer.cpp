// PATTERN: retriggers for a fixed number of hands, then dies.
#include "../balatro.hpp"

namespace {

class Seltzer : public Joker {
    int left_ = 10;
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == RETRIG_QUERY_SCORED) { ++g.scoring.retriggers; return; }
        if (type != HAND_SCORED || isCopy) return;
        if (--left_ <= 0) g.queueDestroyJoker(*this);
    }
};
REGISTER_JOKER(Seltzer, "Seltzer", 2, 6)

}  // namespace
