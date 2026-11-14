// PATTERN: mutates its own sell value (bonusSell), not the score.
// Copy for: Gift Card (mutates OTHERS' bonusSell — iterate g.jokers),
// Diet Cola, Turtle Bean (which also decays hand size via passives).
#include "../balatro.hpp"

namespace {

class GiftCard : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type != ROUND_END || isCopy) return;
        for (auto& j : g.jokers) ++j->bonusSell;
        for (auto& c : g.consumables) ++c->bonusSell;   // consumables too
    }
};
REGISTER_JOKER(GiftCard, "Gift Card", 2, 6)

}  // namespace — internal linkage: no cross-file ODR collisions
