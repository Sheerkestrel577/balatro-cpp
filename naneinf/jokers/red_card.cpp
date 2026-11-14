// PATTERN: scales on PACK_SKIPPED.
#include "../balatro.hpp"

namespace {

class RedCard : public Joker {
    double mult_ = 0;
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.mult = g.scoring.mult + BigNum(mult_);
            return;
        }
        if (type == PACK_SKIPPED && !isCopy) mult_ += 3;     // ⚠ verify
    }
};
REGISTER_JOKER(RedCard, "Red Card", 1, 5)

}  // namespace
