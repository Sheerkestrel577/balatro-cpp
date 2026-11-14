// PATTERN: permanently changes what a card IS (allowed — see the facts-vs-
// interpretations rule). Contrast with Pareidolia, which changes what cards
// COUNT AS and therefore stores nothing. Fires with DNA/Vampire's timing.
// Copy for: Vampire (strips enhancements, scales), Sixth Sense.
#include "../balatro.hpp"

namespace {

class MidasMask : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type != HAND_DETERMINED || isCopy) return;
        for (Card* c : g.scoring.scored)
            if (g.isFace(*c)) c->enhancement = ENH_GOLD;  // a NEW FACT
    }
};
REGISTER_JOKER(MidasMask, "Midas Mask", 2, 7)

}  // namespace — internal linkage: no cross-file ODR collisions
