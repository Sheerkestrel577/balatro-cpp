// PATTERN: first single-card discard -> destroy it, $3.
#include "../balatro.hpp"

namespace {

class TradingCard : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool isCopy) override {
        if (type != DISCARD || isCopy || !ev.cards) return;
        if (g.discardsUsedThisRound != 1) return;     // FIRST discard
        if (ev.cards->size() != 1) return;
        g.destroyCardLater(*(*ev.cards)[0]);
        g.addMoney(3);                                // ⚠ verify
    }
};
REGISTER_JOKER(TradingCard, "Trading Card", 2, 6)

}  // namespace
