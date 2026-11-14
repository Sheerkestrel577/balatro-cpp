// PATTERN: permanently buffs the cards it scores (a new FACT on the card).
#include "../balatro.hpp"

namespace {

class Hiker : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool isCopy) override {
        if (type != CARD_SCORED || isCopy) return;
        ev.card->bonusChips += 5;                          // ⚠ verify
    }
};
REGISTER_JOKER(Hiker, "Hiker", 2, 5)

}  // namespace
