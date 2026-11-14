// consumables/tarots.cpp — all 22 Tarot cards.
// Targeting comes from g.sel.cards (selection order matters for Death).
// Tarots mutate card FACTS — allowed; they change what a card IS.
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

struct Converter : public Consumable {
    int maxCards;
    explicit Converter(int n) : maxCards(n) {}
    bool canUse(const Balatro& g) const override {
        return !g.sel.cards.empty() && (int)g.sel.cards.size() <= maxCards;
    }
};

#define CONVERT_TAROT(Cls, Name, N, Cost, BODY)                        \
    class Cls : public Converter {                                     \
    public:                                                            \
        Cls() : Converter(N) {}                                        \
        void use(Balatro& g) override {                                \
            for (int i : g.sel.cards) { Card* c = g.hand[i]; BODY }     \
            g.clearSelectionAfterUse();                                \
        }                                                              \
    };                                                                 \
    REGISTER_CONSUMABLE(Cls, Name, 0, Cost)

CONVERT_TAROT(TheMagician,   "The Magician",   2, 3, c->enhancement = ENH_LUCKY;)
CONVERT_TAROT(TheEmpress,    "The Empress",    2, 3, c->enhancement = ENH_MULT;)
CONVERT_TAROT(TheHierophant, "The Hierophant", 2, 3, c->enhancement = ENH_BONUS;)
CONVERT_TAROT(TheLovers,     "The Lovers",     1, 3, c->enhancement = ENH_WILD;)
CONVERT_TAROT(TheChariot,    "The Chariot",    1, 3, c->enhancement = ENH_STEEL;)
CONVERT_TAROT(Justice,       "Justice",        1, 3, c->enhancement = ENH_GLASS;)
CONVERT_TAROT(TheDevil,      "The Devil",      1, 3, c->enhancement = ENH_GOLD;)
CONVERT_TAROT(TheTower,      "The Tower",      1, 3, c->enhancement = ENH_STONE;)
CONVERT_TAROT(TheStar,       "The Star",       3, 3, c->suit = DIAMONDS;)
CONVERT_TAROT(TheMoon,       "The Moon",       3, 3, c->suit = CLUBS;)
CONVERT_TAROT(TheSun,        "The Sun",        3, 3, c->suit = HEARTS;)
CONVERT_TAROT(TheWorld,      "The World",      3, 3, c->suit = SPADES;)
CONVERT_TAROT(Strength,      "Strength",       2, 3,
              c->rank = (c->rank == 14) ? 2 : c->rank + 1;)

class TheHangedMan : public Consumable {
public:
    bool canUse(const Balatro& g) const override {
        return !g.sel.cards.empty() && g.sel.cards.size() <= 2;
    }
    void use(Balatro& g) override {
        for (int i : g.sel.cards) g.destroyCardLater(*g.hand[i]);
        g.clearSelectionAfterUse();
        g.runDestructionWindow();
    }
};
REGISTER_CONSUMABLE(TheHangedMan, "The Hanged Man", 0, 3)

class Death : public Consumable {
public:
    bool canUse(const Balatro& g) const override { return g.sel.cards.size() == 2; }
    void use(Balatro& g) override {
        Card* left  = g.hand[g.sel.cards[0]];
        Card* right = g.hand[g.sel.cards[1]];
        int keepId = left->id;
        *left = *right;
        left->id = keepId;
        g.clearSelectionAfterUse();
    }
};
REGISTER_CONSUMABLE(Death, "Death", 0, 3)

class TheHermit : public Consumable {
public:
    bool canUse(const Balatro&) const override { return true; }
    void use(Balatro& g) override {
        int gain = g.money > 0 ? g.money : 0;
        if (gain > 20) gain = 20;
        g.addMoney(gain);
    }
};
REGISTER_CONSUMABLE(TheHermit, "The Hermit", 0, 3)

class Temperance : public Consumable {
public:
    bool canUse(const Balatro&) const override { return true; }
    void use(Balatro& g) override {
        int total = 0;
        for (auto& j : g.jokers) total += j->sellValue();
        if (total > 50) total = 50;
        g.addMoney(total);
    }
};
REGISTER_CONSUMABLE(Temperance, "Temperance", 0, 3)

class TheEmperor : public Consumable {
public:
    // Creators need ROOM: this card is consumed first (freeing its own slot),
    // so "room" means at least one free slot after that.
    bool canUse(const Balatro& g) const override {
        return (int)g.consumables.size() <= g.mods.consumableSlots;
    }
    void use(Balatro& g) override { g.createRandomConsumables(0, 2, keys::SRC_EMPEROR); }
};
REGISTER_CONSUMABLE(TheEmperor, "The Emperor", 0, 3)

class TheHighPriestess : public Consumable {
public:
    bool canUse(const Balatro& g) const override {
        return (int)g.consumables.size() <= g.mods.consumableSlots;
    }
    void use(Balatro& g) override { g.createRandomConsumables(1, 2, keys::SRC_PRIESTESS); }
};
REGISTER_CONSUMABLE(TheHighPriestess, "The High Priestess", 0, 3)

class Judgement : public Consumable {
public:
    bool canUse(const Balatro& g) const override {
        return (int)g.jokers.size() < g.mods.jokerSlots;
    }
    void use(Balatro& g) override { g.createRandomJoker(keys::SRC_JUDGEMENT); }
};
REGISTER_CONSUMABLE(Judgement, "Judgement", 0, 3)

class TheFool : public Consumable {
public:
    bool canUse(const Balatro& g) const override {
        return g.lastTarotIdx >= 0
            && (int)g.consumables.size() < g.mods.consumableSlots;
    }
    void use(Balatro& g) override { g.grantConsumable(g.lastTarotIdx); }
};
REGISTER_CONSUMABLE(TheFool, "The Fool", 0, 3)

// STUBBED BY DESIGN (overview doc §8): stays in the pool at full weight so
// shop/pack distributions are unchanged, but cannot be used.
class TheWheelOfFortune : public Consumable {
public:
    bool canUse(const Balatro&) const override { return false; }
    void use(Balatro&) override {}
};
REGISTER_CONSUMABLE(TheWheelOfFortune, "The Wheel of Fortune", 0, 3)

}  // namespace
