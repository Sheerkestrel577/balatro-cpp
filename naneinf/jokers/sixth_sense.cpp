// Sixth Sense — "If the FIRST HAND of round is a single 6, destroy it and
// create a Spectral card."
//
// TWO conditions that are easy to miss:
//   1. first hand of the round only  (handsPlayedThisRound == 1)
//   2. the spectral is TIED TO THE DESTRUCTION — with two Sixth Senses
//      (via Ankh etc.) only ONE spectral is created, because the 6 can only
//      be destroyed once. destroyCardLater() returns false for the second
//      copy, which is exactly the gate. Contrast Trading Card, whose $3 is
//      NOT gated, and Madness, which scales even with nothing to eat.
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {
class SixthSense : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type != HAND_SCORED || isCopy) return;
        if (g.handsPlayedThisRound != 1) return;        // first hand only
        if (g.scoring.played.size() != 1) return;
        if (g.scoring.played[0]->rank != 6) return;
        if (!g.destroyCardLater(*g.scoring.played[0])) return;  // already claimed
        g.createRandomConsumables(2, 1, keys::SRC_SIXTH_SENSE);         // a Spectral
    }
};
REGISTER_JOKER(SixthSense, "Sixth Sense", 2, 6)
}  // namespace
