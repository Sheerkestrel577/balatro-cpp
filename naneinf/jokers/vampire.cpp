// PATTERN: scaling counter + fact mutation in one joker.
// "+X0.1 Mult per scored ENHANCED card, removing the enhancement."
// Two spec rules both apply here: the xmult counter is isCopy-guarded
// (state), and stripping the enhancement writes a new FACT (allowed —
// it changes what the card IS). This is what makes Midas Mask a combo:
// Midas golds the faces at HAND_DETERMINED, Vampire eats them.
// ⚠ verify: relative order of Midas vs Vampire is JOKER ORDER (left to
// right), so a Vampire left of Midas eats nothing that hand.
#include "../balatro.hpp"

namespace {

class Vampire : public Joker {
    double xmult_ = 1.0;
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.mult = g.scoring.mult * BigNum(xmult_);
            return;
        }
        if (type != HAND_DETERMINED || isCopy) return;
        for (Card* c : g.scoring.scored) {
            if (c->enhancement == ENH_NONE) continue;
            xmult_ += 0.1;                  // ⚠ verify
            c->enhancement = ENH_NONE;      // a NEW FACT
        }
    }
};
REGISTER_JOKER(Vampire, "Vampire", 2, 7)

}  // namespace — internal linkage: no cross-file ODR collisions
