// Ancient Joker — X-Mult per scored card of the round's target suit. The
// target suit is chosen by the engine at round start from the 4 suits MINUS
// the current one (Balatro's reset_ancient_card, key 'anc'..ante), so it
// never repeats round-to-round. This joker just reads g.ancientCardSuit.
#include "../balatro.hpp"

namespace {

class AncientJoker : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_SCORED) return;
        if (g.ancientCardSuit >= 0 && g.suitMatches(*ev.card, (Suit)g.ancientCardSuit))
            g.scoring.mult = g.scoring.mult * BigNum(1.5);   // ⚠ verify
    }
};
REGISTER_JOKER(AncientJoker, "Ancient Joker", 3, 8)

}  // namespace
