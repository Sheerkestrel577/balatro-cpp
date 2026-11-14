// consumables/spectrals.cpp — all 18 Spectral cards. kind = 2.
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

// ---- card creators ----
class Familiar : public Consumable {          // destroy 1 random, add 3 face
public:
    bool canUse(const Balatro& g) const override { return !g.hand.empty(); }
    void use(Balatro& g) override { g.spectralReplace(3, 2); }
};
REGISTER_CONSUMABLE(Familiar, "Familiar", 2, 4)

class Grim : public Consumable {              // destroy 1 random, add 2 aces
public:
    bool canUse(const Balatro& g) const override { return !g.hand.empty(); }
    void use(Balatro& g) override { g.spectralReplace(2, 0); }
};
REGISTER_CONSUMABLE(Grim, "Grim", 2, 4)

class Incantation : public Consumable {       // destroy 1 random, add 4 numbered
public:
    bool canUse(const Balatro& g) const override { return !g.hand.empty(); }
    void use(Balatro& g) override { g.spectralReplace(4, 1); }
};
REGISTER_CONSUMABLE(Incantation, "Incantation", 2, 4)

class Cryptid : public Consumable {           // 2 copies of a selected card
public:
    bool canUse(const Balatro& g) const override { return g.sel.cards.size() == 1; }
    void use(Balatro& g) override {
        Card src = *g.hand[g.sel.cards[0]];
        for (int i = 0; i < 2; ++i) {
            Card& c = g.addCard(src);
            g.hand.push_back(&c);
        }
        g.clearSelectionAfterUse();
    }
};
REGISTER_CONSUMABLE(Cryptid, "Cryptid", 2, 4)

// ---- seal givers (1 selected card) ----
#define SEAL_SPECTRAL(Cls, Name, SEALV)                                 \
    class Cls : public Consumable {                                     \
    public:                                                             \
        bool canUse(const Balatro& g) const override {                  \
            return g.sel.cards.size() == 1; }                           \
        void use(Balatro& g) override {                                 \
            g.hand[g.sel.cards[0]]->seal = SEALV;                       \
            g.clearSelectionAfterUse(); }                               \
    };                                                                  \
    REGISTER_CONSUMABLE(Cls, Name, 2, 4)

SEAL_SPECTRAL(Talisman, "Talisman", SEAL_GOLD)
SEAL_SPECTRAL(DejaVu,   "Deja Vu",  SEAL_RED)
SEAL_SPECTRAL(Trance,   "Trance",   SEAL_BLUE)
SEAL_SPECTRAL(Medium,   "Medium",   SEAL_PURPLE)

// ---- edition / enhancement ----
class Aura : public Consumable {              // random edition on 1 card
public:
    bool canUse(const Balatro& g) const override { return g.sel.cards.size() == 1; }
    void use(Balatro& g) override {
        static const int eds[3] = {ED_FOIL, ED_HOLO, ED_POLY};
        g.hand[g.sel.cards[0]]->edition = eds[g.rng->randint(keys::kAura(), 0, 2)];
        g.clearSelectionAfterUse();
    }
};
REGISTER_CONSUMABLE(Aura, "Aura", 2, 4)

class Sigil : public Consumable {             // all cards in hand -> 1 suit
public:
    bool canUse(const Balatro& g) const override { return !g.hand.empty(); }
    void use(Balatro& g) override {
        int s = g.rng->randint(keys::sigil(), 0, 3);
        for (Card* c : g.hand) c->suit = s;
    }
};
REGISTER_CONSUMABLE(Sigil, "Sigil", 2, 4)

class Ouija : public Consumable {             // all cards -> 1 rank, -1 hand size
public:
    bool canUse(const Balatro& g) const override { return !g.hand.empty(); }
    void use(Balatro& g) override {
        int r = g.rng->randint(keys::ouija(), 2, 14);
        for (Card* c : g.hand) c->rank = r;
        g.permanentHandSizeBonus -= 1;
    }
};
REGISTER_CONSUMABLE(Ouija, "Ouija", 2, 4)

class Ectoplasm : public Consumable {         // negative on a random joker, -1 hand
public:
    bool canUse(const Balatro& g) const override { return !g.jokers.empty(); }
    void use(Balatro& g) override {
        int i = g.rng->randint(keys::kEctoplasm(), 0, (int)g.jokers.size() - 1);
        g.jokers[i]->edition = ED_NEGATIVE;
        g.permanentHandSizeBonus -= 1;
    }
};
REGISTER_CONSUMABLE(Ectoplasm, "Ectoplasm", 2, 4)

class Immolate : public Consumable {          // destroy 5 random cards, +$20
public:
    bool canUse(const Balatro& g) const override { return !g.hand.empty(); }
    void use(Balatro& g) override {
        for (int n = 0; n < 5 && !g.hand.empty(); ++n) {
            int i = g.rng->randint("immolate" + std::to_string(n),
                                   0, (int)g.hand.size() - 1);
            g.destroyCardLater(*g.hand[i]);
            g.hand.erase(g.hand.begin() + i);
        }
        g.runDestructionWindow();
        g.addMoney(20);                                   // ⚠ verify
    }
};
REGISTER_CONSUMABLE(Immolate, "Immolate", 2, 4)

// ---- joker manipulation ----
class Ankh : public Consumable {   // copy a joker, destroy the others
public:
    bool canUse(const Balatro& g) const override { return !g.jokers.empty(); }
    void use(Balatro& g) override {
        int i = g.rng->randint(keys::kAnkh(), 0, (int)g.jokers.size() - 1);
        auto copy = g.jokers[i]->clone();      // deep copy INCLUDING state
        if (copy) copy->edition = ED_NONE;
        for (size_t k = 0; k < g.jokers.size(); ++k)
            if ((int)k != i) g.queueDestroyJoker(*g.jokers[k]);
        g.sweepJokers();
        if (copy) g.addJoker(std::move(copy));
    }
};
REGISTER_CONSUMABLE(Ankh, "Ankh", 2, 4)

class Hex : public Consumable {    // polychrome a random joker, destroy the rest
public:
    bool canUse(const Balatro& g) const override { return !g.jokers.empty(); }
    void use(Balatro& g) override {
        int i = g.rng->randint(keys::kHex(), 0, (int)g.jokers.size() - 1);
        g.jokers[i]->edition = ED_POLY;
        for (size_t k = 0; k < g.jokers.size(); ++k)
            if ((int)k != i) g.queueDestroyJoker(*g.jokers[k]);
        g.sweepJokers();
    }
};
REGISTER_CONSUMABLE(Hex, "Hex", 2, 4)

class Wraith : public Consumable { // a random RARE joker, sets money to 0
public:
    bool canUse(const Balatro& g) const override {
        return (int)g.jokers.size() < g.mods.jokerSlots;
    }
    void use(Balatro& g) override {
        g.createJokerOfRarity(3, keys::SRC_WRAITH);
        g.money = 0;
    }
};
REGISTER_CONSUMABLE(Wraith, "Wraith", 2, 4)

class TheSoul : public Consumable { // a LEGENDARY joker
public:
    bool canUse(const Balatro& g) const override {
        return (int)g.jokers.size() < g.mods.jokerSlots;
    }
    void use(Balatro& g) override { g.createJokerOfRarity(4, keys::SRC_SOUL); }
};
REGISTER_CONSUMABLE(TheSoul, "The Soul", 2, 4)

class BlackHole : public Consumable { // level up EVERY poker hand
public:
    bool canUse(const Balatro&) const override { return true; }
    void use(Balatro& g) override {
        for (int t = 0; t < NUM_HAND_TYPES; ++t) g.levelUpHand(t);
    }
};
REGISTER_CONSUMABLE(BlackHole, "Black Hole", 2, 4)

}  // namespace
