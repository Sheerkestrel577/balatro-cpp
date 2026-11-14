// PATTERN: reads the PLAYED cards (not the scored ones). Half Joker cares
// how many cards you played, so scoring.played is the right vector.
#include "../balatro.hpp"

namespace {

class HalfJoker : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        if (g.scoring.played.size() <= 3)
            g.scoring.mult = g.scoring.mult + BigNum(20);   // ⚠ verify
    }
};
REGISTER_JOKER(HalfJoker, "Half Joker", 1, 5)

}  // namespace
