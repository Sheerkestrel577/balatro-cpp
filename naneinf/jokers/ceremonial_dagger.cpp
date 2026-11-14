// PATTERN: eats the joker to its right for mult.
#include "../balatro.hpp"

namespace {

class CeremonialDagger : public Joker {
    double mult_ = 0;
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.mult = g.scoring.mult + BigNum(mult_);
            return;
        }
        if (type != BLIND_SELECTED || isCopy) return;
        for (size_t i = 0; i + 1 < g.jokers.size(); ++i)
            if (g.jokers[i].get() == this) {
                mult_ += 2 * g.jokers[i + 1]->sellValue();   // ⚠ verify
                g.queueDestroyJoker(*g.jokers[i + 1]);
                return;
            }
    }
};
REGISTER_JOKER(CeremonialDagger, "Ceremonial Dagger", 2, 6)

}  // namespace
