// PATTERN: creates a Tarot on an Ace + Straight.
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class Superposition : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type != HAND_DETERMINED || isCopy) return;
        if (!(g.scoring.containedHands & (1u << STRAIGHT))) return;
        for (Card* c : g.scoring.scored)
            if (c->rank == 14) { g.createRandomConsumables(0, 1, keys::SRC_SUPERPOS);
                                 return; }
    }
};
REGISTER_JOKER(Superposition, "Superposition", 1, 4)

}  // namespace
