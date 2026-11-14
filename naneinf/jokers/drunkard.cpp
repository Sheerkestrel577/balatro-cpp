// PATTERN: pure rule passive.
#include "../balatro.hpp"

namespace {

class Drunkard : public Joker {
public:
    void passiveModifiers(PassiveMods& m) const override { m.discards += 1; }
};
REGISTER_JOKER(Drunkard, "Drunkard", 1, 4)

}  // namespace
