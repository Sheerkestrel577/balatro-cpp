// PATTERN: creates JOKERS on blind select (registry + slots).
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class RiffRaff : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type != BLIND_SELECTED || isCopy) return;
        for (int i = 0; i < 2; ++i)
            g.createJokerOfRarity(1, keys::SRC_RIFF_RAFF);
    }
};
REGISTER_JOKER(RiffRaff, "Riff-Raff", 1, 6)

}  // namespace
