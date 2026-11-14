// PATTERN: reads live run state at trigger time. User-confirmed: Bull sees
// money earned EARLIER IN THIS SAME HAND (gold seals, Rough Gem), because
// the engine commits money instantly.
// Copy for: Fortune Teller (g.tarotsUsed), Constellation, Stuntman, Swashbuckler.
#include "../balatro.hpp"

namespace {

class Acrobat : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != INDEPENDENT) return;
        // ==1 is correct: playHand() decrements handsLeft at the END of
        // the pipeline, so during scoring the final hand still reads 1.
        if (g.handsLeft == 1)
            g.scoring.mult = g.scoring.mult * BigNum(3.0);  // ⚠ verify
    }
};
REGISTER_JOKER(Acrobat, "Acrobat", 2, 6)

}  // namespace — internal linkage: no cross-file ODR collisions
