// PATTERN: economy PASSIVE — changes a rule (the debt floor), not a payout.
#include "../balatro.hpp"

namespace {

class CreditCard : public Joker {
public:
    void passiveModifiers(PassiveMods& m) const override { m.creditLimit = 20; }
};
REGISTER_JOKER(CreditCard, "Credit Card", 1, 1)

}  // namespace — internal linkage: no cross-file ODR collisions
