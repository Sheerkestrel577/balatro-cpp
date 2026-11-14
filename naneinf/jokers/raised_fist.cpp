// Raised Fist — adds DOUBLE the lowest held card's rank as Mult.
// Held-in-hand, but it fires once for the hand, so it scans g.scoring.held
// at INDEPENDENT rather than subscribing to CARD_HELD.
#include "../balatro.hpp"

namespace {
class RaisedFist : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        int lowest = 99;
        for (Card* c : g.scoring.held) {
            if (g.isDebuffed(*c) || c->enhancement == ENH_STONE) continue;
            if (c->rank < lowest) lowest = c->rank;
        }
        if (lowest == 99) return;
        Card tmp; tmp.rank = lowest;
        g.scoring.mult = g.scoring.mult + BigNum(2.0 * tmp.rankChips()); // ⚠
    }
};
REGISTER_JOKER(RaisedFist, "Raised Fist", 1, 5)
}  // namespace
