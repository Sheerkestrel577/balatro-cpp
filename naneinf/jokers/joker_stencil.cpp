// Joker Stencil — X1 Mult per EMPTY joker slot, counting itself.
// If you exceed your slots (negative jokers), the count goes to zero or
// below and the joker becomes X0 — user-confirmed in game. Do NOT clamp
// the empty count to 1.
#include "../balatro.hpp"

namespace {
class JokerStencil : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        double x = 1.0 + (g.mods.jokerSlots - (int)g.jokers.size());
        if (x < 0) x = 0;                    // over-full -> X0
        g.scoring.mult = g.scoring.mult * BigNum(x);
    }
};
REGISTER_JOKER(JokerStencil, "Joker Stencil", 2, 8)
}  // namespace
