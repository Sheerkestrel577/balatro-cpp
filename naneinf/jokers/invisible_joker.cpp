// PATTERN: SELF_SOLD + clone(). Needs 2 rounds first.
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class InvisibleJoker : public Joker {
    int rounds_ = 0;
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == ROUND_END && !isCopy) { ++rounds_; return; }
        if (type != SELF_SOLD || rounds_ < 2) return;
        std::vector<int> others;
        for (size_t i = 0; i < g.jokers.size(); ++i)
            if (g.jokers[i].get() != this) others.push_back((int)i);
        if (others.empty()) return;
        int pick = others[g.rng->randint(keys::kInvisible(), 0, (int)others.size() - 1)];
        auto copy = g.jokers[pick]->clone();     // deep copy WITH state
        if (copy) { copy->edition = ED_NONE; g.addJoker(std::move(copy)); }
    }
};
REGISTER_JOKER(InvisibleJoker, "Invisible Joker", 3, 8)

}  // namespace
