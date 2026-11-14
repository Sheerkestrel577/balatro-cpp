// Four Fingers — passiveModifiers demo. 4-card flushes & straights.
#include "../balatro.hpp"

namespace {

class FourFingers : public Joker {
public:
    void passiveModifiers(PassiveMods& m) const override {
        m.straightFlushLen = 4;
    }
};
REGISTER_JOKER(FourFingers, "Four Fingers", 2, 7)

}  // namespace — internal linkage: no cross-file ODR collisions
