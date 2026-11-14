// PATTERN: creates a Tarot on blind select.
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class Cartomancer : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type != BLIND_SELECTED || isCopy) return;
        g.createRandomConsumables(0, 1, keys::SRC_CARTOMANCER);
    }
};
REGISTER_JOKER(Cartomancer, "Cartomancer", 2, 6)

}  // namespace
