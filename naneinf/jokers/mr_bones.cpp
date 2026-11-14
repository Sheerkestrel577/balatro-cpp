// Mr. Bones — death prevention. The engine's checkDeath() looks for a joker
// named "Mr. Bones", verifies score >= 25% of the goal, then destroys it.
#include "../balatro.hpp"

namespace {
class MrBones : public Joker {
public:
    void interact(Balatro&, int, const EventData&, bool) override {}
};
REGISTER_JOKER(MrBones, "Mr. Bones", 2, 5)
}  // namespace
