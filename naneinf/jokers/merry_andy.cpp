// PATTERN: pure rule passive.
#include "../balatro.hpp"

namespace {

class MerryAndy : public Joker {
public:
    void passiveModifiers(PassiveMods& m) const override { m.discards += 3; m.handSize -= 1; }
};
REGISTER_JOKER(MerryAndy, "Merry Andy", 2, 7)

}  // namespace
