// PATTERN: pays when the BOSS ability triggers this hand.
#include "../balatro.hpp"

namespace {

class Matador : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != BOSS_TRIGGERED) return;
        g.addMoney(8);                                   // ⚠ verify
    }
};
REGISTER_JOKER(Matador, "Matador", 2, 7)

}  // namespace
