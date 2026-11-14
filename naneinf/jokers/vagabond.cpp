// PATTERN: creates a Tarot when you play a hand while poor.
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class Vagabond : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type != HAND_DETERMINED || isCopy) return;
        if (g.money <= 4) g.createRandomConsumables(0, 1, keys::SRC_VAGABOND);  // ⚠
    }
};
REGISTER_JOKER(Vagabond, "Vagabond", 3, 8)

}  // namespace
