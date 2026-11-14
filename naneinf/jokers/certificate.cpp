// PATTERN: adds a random card WITH A SEAL at round start.
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class Certificate : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type != ROUND_START || isCopy) return;
        Card c;
        // real: front = pseudorandom_element(G.P_CARDS, 'cert_fr') [single pick];
        // this engine splits rank/suit off the front key, then rolls the seal
        // on its own key 'certsl' (card.lua:2469).
        c.rank = g.rng->randint(keys::kCertificate(g.ante) + "r", 2, 14);
        c.suit = g.rng->randint(keys::kCertificate(g.ante) + "s", 0, 3);
        static const int seals[4] = {SEAL_RED, SEAL_BLUE, SEAL_GOLD, SEAL_PURPLE};
        c.seal = seals[g.rng->randint("certsl", 0, 3)];
        Card& ref = g.addCard(c);
        g.hand.push_back(&ref);
    }
};
REGISTER_JOKER(Certificate, "Certificate", 2, 6)

}  // namespace
