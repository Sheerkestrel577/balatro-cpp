// PATTERN: copy joker variant — targets the LEFTMOST joker instead of the
// right neighbor. Same depth guard protects both.
#include "../balatro.hpp"

namespace {

class Brainstorm : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (g.jokers.empty() || g.jokers[0].get() == this) return;
        g.invokeAsCopy(*g.jokers[0], type, ev);
    }
};
REGISTER_JOKER(Brainstorm, "Brainstorm", 3, 10)

}  // namespace — internal linkage: no cross-file ODR collisions
