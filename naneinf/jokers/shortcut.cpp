// PATTERN: passive that feeds the HAND EVALUATOR (not scoring).
#include "../balatro.hpp"

namespace {

class Shortcut : public Joker {
public:
    void passiveModifiers(PassiveMods& m) const override { m.straightGap = 2; }
};
REGISTER_JOKER(Shortcut, "Shortcut", 2, 7)

}  // namespace — internal linkage: no cross-file ODR collisions
