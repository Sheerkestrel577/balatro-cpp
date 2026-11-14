// PATTERN: SELF_SOLD — sent only to the joker being sold, BEFORE removal.
// Copy for: Diet Cola (creates a tag), Invisible Joker (duplicates one).
#include "../balatro.hpp"

namespace {

class Luchador : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != SELF_SOLD) return;
        g.bossDisabled = true;      // engine flag; blind hooks respect it
    }
};
REGISTER_JOKER(Luchador, "Luchador", 2, 5)

}  // namespace — internal linkage: no cross-file ODR collisions
