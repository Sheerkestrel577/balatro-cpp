// Mail-In Rebate — $5 per DISCARDED card whose rank matches the round's
// target. The target rank is picked from ONE deck card at round start by the
// engine (Balatro's reset_mail_rank, key 'mail'..ante) — drawn every round
// whether or not this joker is owned, so the stream stays aligned. This
// joker just reads g.mailCardRank.
#include "../balatro.hpp"

namespace {

class MailInRebate : public Joker {
public:
    void interact(Balatro& g, int type, const EventData& ev, bool) override {
        if (type != CARD_DISCARDED) return;
        if (ev.card->rank == g.mailCardRank) g.addMoney(5);   // ⚠ verify
    }
};
REGISTER_JOKER(MailInRebate, "Mail-In Rebate", 1, 4)

}  // namespace — internal linkage: no cross-file ODR collisions
