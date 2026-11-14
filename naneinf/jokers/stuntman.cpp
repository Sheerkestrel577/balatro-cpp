// PATTERN: big chips, costs hand size (passive + scoring).
#include "../balatro.hpp"

namespace {

class Stuntman : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        g.scoring.chips = g.scoring.chips + BigNum(250);      // ⚠ verify
    }
    void passiveModifiers(PassiveMods& m) const override { m.handSize -= 2; }
};
REGISTER_JOKER(Stuntman, "Stuntman", 3, 7)

}  // namespace
