// Blueprint — copy joker demo: forwards EVERY event to the right neighbor
// as a copy. Cycle safety = Balatro's copy-depth guard (pigeonhole).
#include "../balatro.hpp"

namespace {

class Blueprint : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        for (size_t i = 0; i < g.jokers.size(); ++i)
            if (g.jokers[i].get() == this) {
                if (i + 1 < g.jokers.size())
                    g.invokeAsCopy(*g.jokers[i + 1], type, ev);
                return;
            }
    }
};
REGISTER_JOKER(Blueprint, "Blueprint", 3, 10)

}  // namespace — internal linkage: no cross-file ODR collisions
