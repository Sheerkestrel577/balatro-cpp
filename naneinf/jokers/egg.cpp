// PATTERN: mutates its own sell value (bonusSell), not the score.
// Copy for: Gift Card (mutates OTHERS' bonusSell — iterate g.jokers),
// Diet Cola, Turtle Bean (which also decays hand size via passives).
#include "../balatro.hpp"

namespace {

class Egg : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type != ROUND_END || isCopy) return;
        bonusSell += 3;                                   // ⚠ verify
    }
};
REGISTER_JOKER(Egg, "Egg", 1, 4)

}  // namespace — internal linkage: no cross-file ODR collisions
