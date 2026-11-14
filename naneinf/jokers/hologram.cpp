// PATTERN: scales when a card is ADDED to the deck.
#include "../balatro.hpp"

namespace {

class Hologram : public Joker {
    double xmult_ = 1.0;
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.mult = g.scoring.mult * BigNum(xmult_);
            return;
        }
        if (type == CARD_ADDED && !isCopy) xmult_ += 0.25;   // ⚠ verify
    }
};
REGISTER_JOKER(Hologram, "Hologram", 2, 7)

}  // namespace
