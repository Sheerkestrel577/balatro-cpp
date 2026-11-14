// Chicot — disables every Boss Blind. Legendary; only from The Soul (so
// copies require Invisible Joker / Ankh / a Showman second Soul).
// STACKING: disabling a boss REVERSES its effect, and every copy disables
// it, so N copies apply the inverse N times (wiki-documented):
//   The Wall      score requirement / 2 per copy
//   Violet Vessel score requirement / 3 per copy
//   The Water     +original discards per copy
//   The Needle    +(original hands - 1) per copy
//   The Manacle   +1 hand size per copy, PERMANENTLY
// Debuff/effect blinds don't invert — they're simply off.
#include "../balatro.hpp"

namespace {
class Chicot : public Joker {
public:
    void passiveModifiers(PassiveMods& m) const override {
        m.disableBoss = true;
        ++m.chicotCount;
    }
};
REGISTER_JOKER(Chicot, "Chicot", 4, 20)
}  // namespace
