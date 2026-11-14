// PATTERN: retrigger query for HELD cards.
#include "../balatro.hpp"

namespace {

class Mime : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != RETRIG_QUERY_HELD) return;
        ++g.scoring.retriggers;
    }
};
REGISTER_JOKER(Mime, "Mime", 2, 5)

}  // namespace — internal linkage: no cross-file ODR collisions
