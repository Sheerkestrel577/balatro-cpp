// blinds.cpp — all 28 boss blinds. Numbers from the wiki's blind table.
#include <algorithm>
#include "blinds.hpp"

#include "balatro.hpp"
#include "keys.hpp"

const BossDef BOSS_DEFS[NUM_BOSSES] = {
    {"The Hook",       1, 2.0, false},
    {"The Ox",         6, 2.0, false},
    {"The House",      2, 2.0, false},
    {"The Wall",       2, 4.0, false},   // extra large
    {"The Wheel",      2, 2.0, false},
    {"The Arm",        2, 2.0, false},
    {"The Club",       1, 2.0, false},
    {"The Fish",       2, 2.0, false},
    {"The Psychic",    1, 2.0, false},
    {"The Goad",       1, 2.0, false},
    {"The Water",      2, 2.0, false},
    {"The Window",     1, 2.0, false},
    {"The Manacle",    1, 2.0, false},
    {"The Eye",        3, 2.0, false},
    {"The Mouth",      2, 2.0, false},
    {"The Plant",      4, 2.0, false},
    {"The Serpent",    5, 2.0, false},
    {"The Pillar",     1, 2.0, false},
    {"The Needle",     2, 1.0, false},   // small blind sized
    {"The Head",       1, 2.0, false},
    {"The Tooth",      3, 2.0, false},
    {"The Flint",      2, 2.0, false},
    {"The Mark",       2, 2.0, false},
    {"Amber Acorn",    8, 2.0, true},
    {"Verdant Leaf",   8, 2.0, true},
    {"Violet Vessel",  8, 6.0, true},    // very large
    {"Crimson Heart",  8, 2.0, true},
    {"Cerulean Bell",  8, 2.0, true},
};

namespace boss {

void onSelect(Balatro& g, int id) {
    switch (id) {
    case BOSS_WATER:   g.discardsLeft = 0; break;
    case BOSS_NEEDLE:  g.handsLeft = 1; break;
    case BOSS_MANACLE: /* hand size handled in effectiveHandSize() */ break;
    case BOSS_HOUSE:   g.drawFaceDownNextDraw = true; break;   // first hand
    case BOSS_AMBER_ACORN: {
        // flip and shuffle the jokers. The player may "hold" one exempt —
        // exposed as an action (see A_HOLD_JOKER).
        std::vector<int> idx((int)g.jokers.size());
        for (size_t i = 0; i < idx.size(); ++i) idx[i] = (int)i;
        g.rng->shuffle("amber_acorn" + std::to_string(g.ante), idx);
        std::vector<std::unique_ptr<Joker>> shuffled;
        int held = g.heldJokerIdx;
        for (int i : idx) {
            if (i == held) continue;
            g.jokers[i]->faceDown = true;
            shuffled.push_back(std::move(g.jokers[i]));
        }
        if (held >= 0 && held < (int)g.jokers.size() && g.jokers[held])
            shuffled.insert(shuffled.begin() + std::min((size_t)held,
                            shuffled.size()), std::move(g.jokers[held]));
        g.jokers.swap(shuffled);
        g.recomputeMods();
        break; }
    case BOSS_VERDANT_LEAF: g.verdantActive = true; break;
    case BOSS_CRIMSON_HEART: g.rotateCrimsonHeart(); break;
    case BOSS_CERULEAN_BELL: g.rollCeruleanForce(); break;
    default: break;
    }
    if (id == BOSS_FLINT) modifyBase(g, id);
}

void modifyBase(Balatro& g, int id) {
    if (id == BOSS_FLINT) g.flintActive = true;   // halves base chips & mult
}

// Blind:press_play, The Hook (blind.lua): builds a COPY of the remaining
// hand, then twice does pseudorandom_element(copy, pseudoseed('hook')) and
// removes the pick from the copy. pseudorandom_element orders card lists by
// sort_id (creation order == our Card::id), NOT by hand display position —
// getting this wrong desyncs the pick whenever display order != id order.
// The two victims enter the discard pile HERE, before the played cards do.
void hookPressPlay(Balatro& g, std::vector<Card*>& remaining) {
    std::vector<Card*> cand = remaining;
    std::sort(cand.begin(), cand.end(),
              [](const Card* a, const Card* b) { return a->id < b->id; });
    for (int n = 0; n < 2 && !cand.empty(); ++n) {
        int i = g.rng->randint(keys::kHook(), 0, (int)cand.size() - 1);
        Card* c = cand[i];
        cand.erase(cand.begin() + i);
        remaining.erase(std::find(remaining.begin(), remaining.end(), c));
        g.discardPile.push_back(c);
        EventData ce{}; ce.card = c;
        g.broadcast(CARD_DISCARDED, ce);
    }
    g.bossTriggered();
}

void onHandPlayed(Balatro& g, int id) {
    switch (id) {
    // BOSS_HOOK is handled in hookPressPlay() — it fires at press_play,
    // before scoring, not after the hand resolves.
    case BOSS_OX:                  // most played hand sets money to $0
        if (g.scoring.handType == g.mostPlayedHand()) {
            g.money = 0; g.bossTriggered();
        }
        break;
    case BOSS_ARM:                 // permanently lower the played hand level
        if (g.handLevels[g.scoring.handType] > 1) {
            --g.handLevels[g.scoring.handType];
            g.bossTriggered();
        }
        break;
    case BOSS_TOOTH:               // -$1 per card played
        g.money -= (int)g.scoring.played.size();
        g.bossTriggered();
        break;
    case BOSS_FISH:                // cards drawn face down after each hand
        g.drawFaceDownNextDraw = true; break;
    case BOSS_CRIMSON_HEART:       // a different joker disabled each hand
        g.rotateCrimsonHeart(); break;
    case BOSS_CERULEAN_BELL:
        g.rollCeruleanForce(); break;
    default: break;
    }
}

void onDiscardAction(Balatro& g, int id) {
    if (id == BOSS_CERULEAN_BELL) g.rollCeruleanForce();
}

// Per the wiki's Matador column, these bosses "trigger" when their ability
// affects the played hand. Hook/Ox/Arm/Tooth signal inline; the passive
// ones (debuffs, hand restrictions, Flint) are detected after scoring.
bool triggeredThisHand(const Balatro& g, int id) {
    switch (id) {
    case BOSS_CLUB: case BOSS_GOAD: case BOSS_WINDOW: case BOSS_HEAD:
    case BOSS_PLANT: case BOSS_PILLAR: case BOSS_VERDANT_LEAF: {
        for (Card* c : g.scoring.played)      // did it debuff anything played?
            if (debuffsCard(g, id, *c)) return true;
        return false; }
    case BOSS_FLINT:   return true;           // always halves the base
    case BOSS_PSYCHIC: return true;           // the restriction was in force
    case BOSS_EYE: case BOSS_MOUTH: return true;
    default: return false;                    // inline signallers
    }
}

bool debuffsCard(const Balatro& g, int id, const Card& c) {
    switch (id) {
    case BOSS_CLUB:   return c.suit == CLUBS;
    case BOSS_GOAD:   return c.suit == SPADES;
    case BOSS_WINDOW: return c.suit == DIAMONDS;
    case BOSS_HEAD:   return c.suit == HEARTS;
    // NOTE: these read the PRINTED suit; Wild/Smeared do not dodge them.
    case BOSS_PLANT:  return g.isFace(c);
    case BOSS_PILLAR: return c.playedThisAnte;
    case BOSS_VERDANT_LEAF: return g.verdantActive;
    default: return false;
    }
}

bool handAllowed(const Balatro& g, int id) {
    switch (id) {
    case BOSS_PSYCHIC: return g.sel.cards.size() == 5;
    // The Eye / The Mouth no longer BLOCK plays (that softlocked runs
    // with no matching hand and no discards). Restricted hands are now
    // PLAYABLE FOR ZERO SCORE -- see boss::zeroesHand and playHand:
    // counters (Telescope/Supernova) tick, Matador triggers, but the
    // scoring pipeline (and thus Obelisk / Ride the Bus / Green Joker
    // state) never runs. Ruling by the game author.
    default: return true;
    }
}

bool forcesSelection(int id) { return id == BOSS_CERULEAN_BELL; }

// True when the active boss makes this hand type score ZERO this play.
bool zeroesHand(const Balatro& g, int handType) {
    if (g.blindIdx != 2 || g.bossDisabled) return false;
    switch (g.bossId) {
    case BOSS_EYE:   return g.handTypePlayedThisRound[handType];
    case BOSS_MOUTH: return g.mouthLockedType >= 0
                         && handType != g.mouthLockedType;
    default: return false;
    }
}

}  // namespace boss
