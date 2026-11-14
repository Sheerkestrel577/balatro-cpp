// Swashbuckler — +Mult equal to the total sell value of your OTHER jokers.
#include "../balatro.hpp"

namespace {
class Swashbuckler : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        int total = 0;
        for (auto& j : g.jokers)
            if (j.get() != this && !j->pendingDestroy) total += j->sellValue();
        g.scoring.mult = g.scoring.mult + BigNum((double)total);   // ⚠ verify
    }
};
REGISTER_JOKER(Swashbuckler, "Swashbuckler", 1, 4)
}  // namespace
