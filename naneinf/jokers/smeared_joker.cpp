// PATTERN: passive that changes what SUITS mean — reaches every suit joker
// without touching any of them (they all ask through suitMask).
#include "../balatro.hpp"

namespace {

class SmearedJoker : public Joker {
public:
    void passiveModifiers(PassiveMods& m) const override { m.smearedSuits = true; }
};
REGISTER_JOKER(SmearedJoker, "Smeared Joker", 2, 7)

}  // namespace — internal linkage: no cross-file ODR collisions
