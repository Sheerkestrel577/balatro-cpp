// The Idol — X-Mult when you score a card matching the round's target
// rank+suit. The target (rank AND suit) is picked from ONE deck card at
// round start by the engine (Balatro's reset_idol_card, key 'idol'..ante),
// so this joker just reads g.idolCardRank / g.idolCardSuit.
#include "../balatro.hpp"

namespace {

class TheIdol : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_SCORED) return;
        if (ev.card->rank == g.idolCardRank &&
            g.suitMatches(*ev.card, (Suit)g.idolCardSuit))
            g.scoring.mult = g.scoring.mult * BigNum(2.0);   // ⚠ verify
    }
};
REGISTER_JOKER(TheIdol, "The Idol", 2, 6)

}  // namespace
