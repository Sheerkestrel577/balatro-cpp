// PATTERN: first discard of the round levels that hand.
#include "../balatro.hpp"

namespace {

class BurntJoker : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool isCopy) override {
        if (type != DISCARD || isCopy || !ev.cards) return;
        if (g.discardsUsedThisRound != 1) return;
        std::vector<Card*> cs = *ev.cards;
        g.levelUpHand(evaluateHand(cs, g.mods).scoredType);
    }
};
REGISTER_JOKER(BurntJoker, "Burnt Joker", 3, 8)

}  // namespace
