// PATTERN: cash-out payout computed from DECK CONTENTS at round end.
#include "../balatro.hpp"

namespace {

class Cloud9 : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool) override {
        if (type != ROUND_END) return;
        int nines = 0;
        for (const Card& c : g.deck)
            if (!c.destroyed && c.rank == 9) ++nines;
        g.addMoney(nines);                                // ⚠ verify
    }
};
REGISTER_JOKER(Cloud9, "Cloud 9", 2, 7)

}  // namespace — internal linkage: no cross-file ODR collisions
