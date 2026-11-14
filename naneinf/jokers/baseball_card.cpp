// PATTERN: inspects OTHER jokers — needs each instance to know its own
// rarity, which REGISTER_JOKER now fills in.
#include "../balatro.hpp"

namespace {

class BaseballCard : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        for (auto& j : g.jokers)
            if (!j->pendingDestroy && j->rarity == 2)       // uncommon
                g.scoring.mult = g.scoring.mult * BigNum(1.5);   // ⚠ verify
    }
};
REGISTER_JOKER(BaseballCard, "Baseball Card", 3, 8)

}  // namespace
