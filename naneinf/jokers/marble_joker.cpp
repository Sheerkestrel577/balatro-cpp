// PATTERN: adds a card to the DECK on blind select.
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class MarbleJoker : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type != BLIND_SELECTED || isCopy) return;
        Card c;
        c.rank = g.rng->randint(keys::kMarble() + "r", 2, 14);
        c.suit = g.rng->randint(keys::kMarble() + "s", 0, 3);
        c.enhancement = ENH_STONE;
        g.addCard(c);
    }
};
REGISTER_JOKER(MarbleJoker, "Marble Joker", 2, 6)

}  // namespace
