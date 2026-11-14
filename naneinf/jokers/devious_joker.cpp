// PATTERN: "if played hand contains a <type>". Reads containedHands —
// NEVER scoring.handType. That is what makes a 4oaK-flush proc flush jokers.
// The anonymous namespace gives this class internal linkage, so two joker
// files can never collide at link time (ODR violation) even if you forget
// to rename after a copy-paste.
#include "../balatro.hpp"

namespace {

class DeviousJoker : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        if (g.scoring.containedHands & (1u << STRAIGHT))
            g.scoring.chips = g.scoring.chips + BigNum(100);   // ⚠ verify
    }
};
REGISTER_JOKER(DeviousJoker, "Devious Joker", 1, 4)

}  // namespace
