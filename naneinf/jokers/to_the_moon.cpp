// To the Moon — "Earn an extra $1 of interest for every $5 you have at end
// of round." That is a RATE increase (from $1/$5 to $2/$5), NOT a cap
// increase: at $25 held you get $10, not $5.
#include "../balatro.hpp"

namespace {
class ToTheMoon : public Joker {
public:
    void passiveModifiers(PassiveMods& m) const override {
        m.interestRate += 1;        // ⚠ verify: does the $5 cap scale too?
    }
};
REGISTER_JOKER(ToTheMoon, "To the Moon", 2, 5)
}  // namespace
