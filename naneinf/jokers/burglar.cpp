// Burglar — BLIND_SELECTED demo. +3 hands, lose all discards.
#include "../balatro.hpp"

namespace {

class Burglar : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != BLIND_SELECTED) return;
        g.handsLeft += 3;
        g.discardsLeft = 0;
    }
};
REGISTER_JOKER(Burglar, "Burglar", 2, 6)

}  // namespace — internal linkage: no cross-file ODR collisions
