// PATTERN: "if played hand contains a <type>". Reads containedHands —
// NEVER scoring.handType. That is what makes a 4oaK-flush proc flush jokers.
// The anonymous namespace gives this class internal linkage, so two joker
// files can never collide at link time (ODR violation) even if you forget
// to rename after a copy-paste.
#include "../balatro.hpp"

namespace {

class CleverJoker : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        if (g.scoring.containedHands & (1u << TWO_PAIR))
            g.scoring.chips = g.scoring.chips + BigNum(80);   // ⚠ verify
    }
};
REGISTER_JOKER(CleverJoker, "Clever Joker", 1, 4)

}  // namespace
