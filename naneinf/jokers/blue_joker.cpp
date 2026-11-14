// PATTERN: counts cards remaining in the DRAW PILE.
#include "../balatro.hpp"

namespace {

class BlueJoker : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        g.scoring.chips = g.scoring.chips + BigNum(2.0 * g.drawPile.size());
    }
};
REGISTER_JOKER(BlueJoker, "Blue Joker", 1, 5)

}  // namespace
