// PATTERN: live counter tied to being IN PLAY (card.lua add_to_deck /
// remove_from_deck), NOT a passive modifier.
//
// Chaos grants a free-reroll CHARGE, not a per-shop discount flag:
//   * openShop() recharges g.freeRerolls to the number of Chaos held
//     (state_events.lua new_round: free_rerolls = #chaos)
//   * acquiring one mid-shop adds a charge immediately, so a shop you have
//     already rerolled in becomes free again
//   * selling one removes a charge (floored at 0)
// Together those make sell-then-rebuy grant a second free reroll in the same
// shop — the real game's behaviour, because the buy hook is unconditional.
//
// This is why passiveModifiers() is the WRONG home for it: passives are
// recomputed from scratch and could not track a spent charge, and copies
// never contribute (a Blueprint copying Chaos grants nothing in the real
// game either, since add_to_deck only fires for the physical card).
#include "../balatro.hpp"

namespace {

class ChaosTheClown : public Joker {
    bool counted_ = false;          // this physical card's charge is applied
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (isCopy) return;
        if (type == JOKERS_CHANGED && !counted_) {
            counted_ = true;
            g.addFreeRerolls(+1);   // add_to_deck
        } else if (type == SELF_SOLD && counted_) {
            counted_ = false;
            g.addFreeRerolls(-1);   // remove_from_deck
        }
    }
};
REGISTER_JOKER(ChaosTheClown, "Chaos the Clown", 1, 4)

}  // namespace
