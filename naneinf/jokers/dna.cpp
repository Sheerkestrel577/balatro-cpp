// PATTERN: duplicates a played card into hand AND deck.
#include "../balatro.hpp"

namespace {

class DNA : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type != HAND_DETERMINED || isCopy) return;
        if (g.handsPlayedThisRound != 1) return;      // first hand only
        if (g.scoring.played.size() != 1) return;
        Card& c = g.addCard(*g.scoring.played[0]);
        g.hand.push_back(&c);
    }
};
REGISTER_JOKER(DNA, "DNA", 3, 8)

}  // namespace
