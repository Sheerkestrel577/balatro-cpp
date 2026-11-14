// PATTERN: creates a Spectral on an exact hand type.
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class Seance : public Joker {
public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type != HAND_DETERMINED || isCopy) return;
        if (g.scoring.handType == STRAIGHT_FLUSH)            // ⚠ verify
            g.createRandomConsumables(2, 1, keys::SRC_SEANCE);
    }
};
REGISTER_JOKER(Seance, "Séance", 2, 6)

}  // namespace
