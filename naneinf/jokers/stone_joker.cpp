// PATTERN: counts an enhancement across the full deck.
#include "../balatro.hpp"

namespace {

class StoneJoker : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        int n = 0;
        for (const Card& c : g.deck)
            if (!c.destroyed && c.enhancement == ENH_STONE) ++n;
        g.scoring.chips = g.scoring.chips + BigNum(25.0 * n);   // ⚠ verify
    }
};
REGISTER_JOKER(StoneJoker, "Stone Joker", 2, 6)

}  // namespace
