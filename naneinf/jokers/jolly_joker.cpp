// PATTERN: "if played hand contains a <type>". Reads containedHands —
// NEVER scoring.handType. This is what makes 4oaK-flush proc flush jokers.
// Copy for: Jolly/Zany/Mad/Crazy/Droll (mult) and Sly/Wily/Clever/Devious/
// Crafty (chips), The Duo/Trio/Family/Order/Tribe (xmult).
#include "../balatro.hpp"

namespace {

class JollyJoker : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        if (g.scoring.containedHands & (1u << PAIR))
            g.scoring.mult = g.scoring.mult + BigNum(8);
    }
};
REGISTER_JOKER(JollyJoker, "Jolly Joker", 1, 3)

}  // namespace — internal linkage: no cross-file ODR collisions
