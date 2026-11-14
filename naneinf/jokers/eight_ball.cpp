// PATTERN: per-scored-card chance that CREATES a consumable.
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class EightBall : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_SCORED) return;
        if (ev.card->rank != 8) return;
        if (g.chance(keys::k8Ball(), 1, 4))                         // ⚠ verify
            g.createRandomConsumables(0, 1, keys::SRC_8BALL);
    }
};
REGISTER_JOKER(EightBall, "8 Ball", 1, 5)

}  // namespace
