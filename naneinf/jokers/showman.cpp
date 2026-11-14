// PATTERN: pure rule passive.
#include "../balatro.hpp"

namespace {

class Showman : public Joker {
public:
    void passiveModifiers(PassiveMods& m) const override { m.allowDuplicates = true; }
};
REGISTER_JOKER(Showman, "Showman", 2, 5)

}  // namespace
