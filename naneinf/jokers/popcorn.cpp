// Popcorn — +20 Mult, losing 4 per ROUND (contrast Ice Cream, per hand).
#include "../balatro.hpp"

namespace {
class Popcorn : public Joker {
    double mult_ = 20;
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.mult = g.scoring.mult + BigNum(mult_);
            return;
        }
        if (type != ROUND_END || isCopy) return;
        mult_ -= 4;                                        // ⚠ verify
        if (mult_ <= 0) g.queueDestroyJoker(*this);
    }
};
REGISTER_JOKER(Popcorn, "Popcorn", 1, 5)
}  // namespace
