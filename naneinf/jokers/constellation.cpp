// PATTERN: scales on CONSUMABLE_USED (planets only).
#include "../balatro.hpp"

namespace {

class Constellation : public Joker {
    double xmult_ = 1.0;
public:
    void interact(Balatro& g, int type, const EventData& ev, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.mult = g.scoring.mult * BigNum(xmult_);
            return;
        }
        if (type == CONSUMABLE_USED && !isCopy && ev.consumableId == 1)
            xmult_ += 0.1;                               // ⚠ verify
    }
};
REGISTER_JOKER(Constellation, "Constellation", 2, 6)

}  // namespace
