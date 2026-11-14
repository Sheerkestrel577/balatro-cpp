// PATTERN: chance to mutate run state (hand levels).
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class SpaceJoker : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != HAND_DETERMINED) return;
        if (g.chance(keys::kSpaceJoker(), 1, 4))               // ⚠ verify
            g.levelUpHand(g.scoring.handType);
    }
};
REGISTER_JOKER(SpaceJoker, "Space Joker", 2, 5)

}  // namespace
