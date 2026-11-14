// PATTERN: money on a DISCARD ACTION (per action, not per card) — contrast
// mail_in_rebate.cpp which is per discarded card.
#include "../balatro.hpp"

namespace {

class FacelessJoker : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != DISCARD || !ev.cards) return;
        int faces = 0;
        for (Card* c : *ev.cards) if (g.isFace(*c)) ++faces;
        if (faces >= 3) g.addMoney(5);                    // ⚠ verify
    }
};
REGISTER_JOKER(FacelessJoker, "Faceless Joker", 1, 4)

}  // namespace — internal linkage: no cross-file ODR collisions
