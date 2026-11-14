// PATTERN: pure rule passive.
#include "../balatro.hpp"

namespace {

class Astronomer : public Joker {
public:
    void passiveModifiers(PassiveMods& m) const override { m.planetsFree = true; }
};
REGISTER_JOKER(Astronomer, "Astronomer", 2, 8)

}  // namespace
