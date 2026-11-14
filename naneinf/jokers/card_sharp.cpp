// PATTERN: xmult if this hand type already appeared this round.
#include "../balatro.hpp"

namespace {

class CardSharp : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        if (g.handTypePlayedBeforeThisHand)
            g.scoring.mult = g.scoring.mult * BigNum(3.0);   // ⚠ verify
    }
};
REGISTER_JOKER(CardSharp, "Card Sharp", 2, 6)

}  // namespace
