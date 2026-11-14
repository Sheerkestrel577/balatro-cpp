// PATTERN: PACK_OPENED hook + chance. 1 in 2 for a Tarot.
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class Hallucination : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != PACK_OPENED) return;
        if (g.chance(keys::kHallucination(g.ante), 1, 2))                 // ⚠ verify
            g.createRandomConsumables(0, 1, keys::SRC_ARCANA);
    }
};
REGISTER_JOKER(Hallucination, "Hallucination", 1, 4)

}  // namespace
