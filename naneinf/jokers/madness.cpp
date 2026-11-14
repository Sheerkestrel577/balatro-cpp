// PATTERN: gains xmult and DESTROYS a random joker (queued).
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class Madness : public Joker {
    double xmult_ = 1.0;
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.mult = g.scoring.mult * BigNum(xmult_);
            return;
        }
        if (type != BLIND_SELECTED || isCopy) return;
        if (g.blindIdx == 2) return;                 // small/big blinds only
        xmult_ += 0.5;                               // ⚠ verify
        std::vector<int> others;
        for (size_t i = 0; i < g.jokers.size(); ++i)
            if (g.jokers[i].get() != this) others.push_back((int)i);
        if (others.empty()) return;
        int pick = others[g.rng->randint(keys::kMadness(), 0, (int)others.size() - 1)];
        g.queueDestroyJoker(*g.jokers[pick]);        // swept after broadcast
    }
};
REGISTER_JOKER(Madness, "Madness", 2, 7)

}  // namespace
