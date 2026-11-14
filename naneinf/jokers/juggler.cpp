// PATTERN: pure rule passive.
#include "../balatro.hpp"

namespace {

class Juggler : public Joker {
public:
    void passiveModifiers(PassiveMods& m) const override { m.handSize += 1; }
};
REGISTER_JOKER(Juggler, "Juggler", 1, 4)

}  // namespace
