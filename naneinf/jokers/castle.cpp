// Castle — permanently gains +3 chips per DISCARDED card of the round's
// target suit; the accumulated chips are added on every scored hand. The
// target suit is a deck-weighted pick chosen by the engine at round start
// (Balatro's reset_castle_card, key 'cas'..ante); this joker reads
// g.castleCardSuit. NOTE: the chip total PERSISTS across rounds (Castle is a
// scaling joker — card.lua accumulates extra.chips and never resets it).
#include "../balatro.hpp"

namespace {

class Castle : public Joker {
    double chips_ = 0;
public:
    void interact(Balatro& g, int type, const EventData& ev, bool isCopy) override {
        if (type == INDEPENDENT) {
            g.scoring.chips = g.scoring.chips + BigNum(chips_);
            return;
        }
        if (isCopy) return;
        if (type == CARD_DISCARDED && ev.card
            && g.suitMatches(*ev.card, (Suit)g.castleCardSuit))
            chips_ += 3;                                     // ⚠ verify
    }
};
REGISTER_JOKER(Castle, "Castle", 2, 6)

}  // namespace
