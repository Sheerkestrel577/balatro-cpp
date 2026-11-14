// Steel Joker — X0.2 Mult per Steel card in the FULL DECK.
// NOTE: this is a SCORING joker (INDEPENDENT), not a round-end one, and the
// xmult is 1 + 0.2*n. Multiplying by n/5 would zero the mult at n = 0.
#include "../balatro.hpp"

namespace {
class SteelJoker : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        int steel = 0;
        for (const Card& c : g.deck)
            if (!c.destroyed && c.enhancement == ENH_STEEL) ++steel;
        g.scoring.mult = g.scoring.mult * BigNum(1.0 + 0.2 * steel); // ⚠ verify
    }
};
REGISTER_JOKER(SteelJoker, "Steel Joker", 2, 7)
}  // namespace
