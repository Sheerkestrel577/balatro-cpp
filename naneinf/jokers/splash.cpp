// PATTERN: boolean passive. Every played card scores.
#include "../balatro.hpp"

namespace {

class Splash : public Joker {
public:
    void passiveModifiers(PassiveMods& m) const override {
        m.allCardsScore = true;
    }
};
REGISTER_JOKER(Splash, "Splash", 1, 3)

}  // namespace — internal linkage: no cross-file ODR collisions
