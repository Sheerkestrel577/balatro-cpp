// PATTERN: pure rule passive.
#include "../balatro.hpp"

namespace {

class Troubadour : public Joker {
public:
    void passiveModifiers(PassiveMods& m) const override { m.handSize += 2; m.hands -= 1; }
};
REGISTER_JOKER(Troubadour, "Troubadour", 2, 6)

}  // namespace
