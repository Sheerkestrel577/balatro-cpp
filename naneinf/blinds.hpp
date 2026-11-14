// blinds.hpp — all 28 boss blinds (23 regular + 5 showdown).
#pragma once
#include <string>
#include <vector>

class Balatro;
struct Card;

// Boss ids. Order matters only for the registry table below.
enum BossId {
    BOSS_HOOK = 0, BOSS_OX, BOSS_HOUSE, BOSS_WALL, BOSS_WHEEL, BOSS_ARM,
    BOSS_CLUB, BOSS_FISH, BOSS_PSYCHIC, BOSS_GOAD, BOSS_WATER, BOSS_WINDOW,
    BOSS_MANACLE, BOSS_EYE, BOSS_MOUTH, BOSS_PLANT, BOSS_SERPENT,
    BOSS_PILLAR, BOSS_NEEDLE, BOSS_HEAD, BOSS_TOOTH, BOSS_FLINT, BOSS_MARK,
    // showdown (ante 8, 16, 24, 32)
    BOSS_AMBER_ACORN, BOSS_VERDANT_LEAF, BOSS_VIOLET_VESSEL,
    BOSS_CRIMSON_HEART, BOSS_CERULEAN_BELL,
    NUM_BOSSES
};

struct BossDef {
    const char* name;
    int minAnte;        // earliest ante this boss may appear
    double mult;        // score multiplier (2x except Wall/Needle/Vessel)
    bool showdown;
};

extern const BossDef BOSS_DEFS[NUM_BOSSES];

// Hook set (engine spec §1). One implementation switches on the id; the
// blind is pure behavior, all state lives in Balatro.
namespace boss {
void onSelect(Balatro& g, int id);                 // start-of-blind effects
void onHandPlayed(Balatro& g, int id);             // after a hand resolves
// The Hook fires at press_play (blind.lua Blind:press_play), AFTER the
// played cards leave the hand and BEFORE scoring: eats 2 from `remaining`,
// pushing them onto the discard pile ahead of the played cards.
void hookPressPlay(Balatro& g, std::vector<Card*>& remaining);
void onDiscardAction(Balatro& g, int id);          // after a discard
bool debuffsCard(const Balatro& g, int id, const Card& c);
bool handAllowed(const Balatro& g, int id);        // legality of A_PLAY
void modifyBase(Balatro& g, int id);               // The Flint
bool forcesSelection(int id);
bool zeroesHand(const Balatro& g, int handType);                      // Cerulean Bell
bool triggeredThisHand(const Balatro& g, int id);  // Matador
}
