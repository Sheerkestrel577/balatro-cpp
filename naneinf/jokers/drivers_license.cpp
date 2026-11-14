// PATTERN: xmult if enough ENHANCED cards in the deck.
#include "../balatro.hpp"

namespace {

class DriversLicense : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        int n = 0;
        for (const Card& c : g.deck)
            if (!c.destroyed && c.enhancement != ENH_NONE) ++n;
        if (n >= 16) g.scoring.mult = g.scoring.mult * BigNum(3.0);  // ⚠
    }
};
REGISTER_JOKER(DriversLicense, "Drivers License", 3, 7)

}  // namespace
