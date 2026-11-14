// PATTERN: two-condition scan over the SCORED cards.
#include "../balatro.hpp"

namespace {

class SeeingDouble : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        bool club = false, other = false;
        for (Card* c : g.scoring.scored) {
            if (g.suitMatches(*c, CLUBS)) club = true;
            if (g.suitMatches(*c, SPADES) || g.suitMatches(*c, HEARTS)
                || g.suitMatches(*c, DIAMONDS)) other = true;
        }
        if (club && other)
            g.scoring.mult = g.scoring.mult * BigNum(2.0);   // ⚠ verify
    }
};
REGISTER_JOKER(SeeingDouble, "Seeing Double", 2, 6)

}  // namespace
