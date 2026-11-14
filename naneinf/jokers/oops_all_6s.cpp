// PATTERN: passive that STACKS (multiple copies multiply). Contrast with
// boolean passives (Splash/Pareidolia) which just set a flag.
#include "../balatro.hpp"

namespace {

class OopsAll6s : public Joker {
public:
    void passiveModifiers(PassiveMods& m) const override {
        m.probNumeratorMult *= 2;
    }
};
REGISTER_JOKER(OopsAll6s, "Oops! All 6s", 2, 4)

}  // namespace — internal linkage: no cross-file ODR collisions
