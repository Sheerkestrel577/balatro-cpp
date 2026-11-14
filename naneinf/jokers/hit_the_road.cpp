// PATTERN: scales on discarded cards, resets each round.
#include "../balatro.hpp"

namespace {

class HitTheRoad : public Joker {
    double xmult_ = 1.0;
public:
    void interact(Balatro& g, int type, const EventData& ev, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.mult = g.scoring.mult * BigNum(xmult_);
            return;
        }
        if (isCopy) return;
        if (type == ROUND_START) xmult_ = 1.0;
        if (type == CARD_DISCARDED && ev.card && ev.card->rank == 11)
            xmult_ += 0.5;                               // ⚠ verify
    }
};
REGISTER_JOKER(HitTheRoad, "Hit the Road", 3, 8)

}  // namespace
