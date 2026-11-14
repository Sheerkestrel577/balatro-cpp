// Pareidolia — passive: every card counts as a face (ask-time isFace).
#include "../balatro.hpp"

namespace {

class Pareidolia : public Joker {
public:
    void passiveModifiers(PassiveMods& m) const override {
        m.allFaces = true;
    }
};
REGISTER_JOKER(Pareidolia, "Pareidolia", 2, 5)

}  // namespace — internal linkage: no cross-file ODR collisions
