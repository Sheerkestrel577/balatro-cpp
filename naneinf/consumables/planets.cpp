// consumables/planets.cpp — all 12 Planet cards. Each levels one hand type.
#include "../balatro.hpp"

namespace {

#define PLANET(Cls, Name, Cost, HAND)                                  \
    class Cls : public Consumable {                                    \
    public:                                                            \
        bool canUse(const Balatro&) const override { return true; }    \
        void use(Balatro& g) override { g.levelUpHand(HAND); }         \
    };                                                                 \
    REGISTER_CONSUMABLE(Cls, Name, 1, Cost)

PLANET(Pluto,   "Pluto",   3, HIGH_CARD)
PLANET(Mercury, "Mercury", 3, PAIR)
PLANET(Uranus,  "Uranus",  3, TWO_PAIR)
PLANET(Venus,   "Venus",   3, THREE_OAK)
PLANET(Saturn,  "Saturn",  3, STRAIGHT)
PLANET(Jupiter, "Jupiter", 3, FLUSH)
PLANET(Earth,   "Earth",   3, FULL_HOUSE)
PLANET(Mars,    "Mars",    3, FOUR_OAK)
PLANET(Neptune, "Neptune", 3, STRAIGHT_FLUSH)
PLANET(PlanetX, "Planet X", 3, FIVE_OAK)
PLANET(Ceres,   "Ceres",   3, FLUSH_HOUSE)
PLANET(Eris,    "Eris",    3, FLUSH_FIVE)

}  // namespace
