// PATTERN: SELF_SOLD grants a TAG.
#include "../balatro.hpp"

namespace {

class DietCola : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != SELF_SOLD) return;
        g.pendingTags.push_back(TAG_DOUBLE);
    }
};
REGISTER_JOKER(DietCola, "Diet Cola", 2, 6)

}  // namespace
