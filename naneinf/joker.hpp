// joker.hpp — Joker base (the two FROZEN virtuals, spec §1 & §10),
// PassiveMods (spec §5), and the self-registration registry.
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "events.hpp"

class Balatro;  // forward declaration

// Recomputed from scratch on JOKERS_CHANGED / voucher purchase.
// Jokers AND vouchers write here; nobody else does. (spec §5)
struct PassiveMods {
    int handSize = 8;
    int hands = 4, discards = 3;
    int jokerSlots = 5, consumableSlots = 2;
    bool allCardsScore = false;      // Splash
    bool allFaces = false;           // Pareidolia
    int straightFlushLen = 5;        // Four Fingers -> 4
    int straightGap = 1;             // Shortcut -> 2 (max rank diff)
    bool smearedSuits = false;       // Smeared Joker
    bool allowDuplicates = false;    // Showman
    int probNumeratorMult = 1;       // Oops! All 6s (stacks: *=2)
    // (Chaos the Clown does NOT live here: its free rerolls are a spendable
    // charge counter on Balatro, not a recomputed passive. See
    // jokers/chaos_the_clown.cpp.)
    bool disableBoss = false;        // Chicot / Luchador
    int  chicotCount = 0;            // copies of Chicot (they stack, see below)
    int  permanentHandSize = 0;      // Manacle-under-Chicot bonus (persists)
    bool planetsFree = false;        // Astronomer
    int creditLimit = 0;             // Credit Card -> 20 (debt floor)
    // economy (vouchers/jokers write here)
    int interestPer = 5;             // $1 per $5 held
    int interestCap = 5;             // Seed Money -> 10, Money Tree -> 20
    int interestRate = 1;            // To the Moon -> 2 ($2 per $5)
    int moneyPerHand = 1;            // $1 per unused hand
    bool noInterest = false;         // some decks/challenges
};

class Joker {
public:
    // FROZEN SIGNATURE. void return; effects mutate `game`; broadcast
    // order is effect order. isCopy: guard internal-state changes with
    // if(!isCopy); probability rolls are NOT guarded. (spec §1)
    virtual void interact(Balatro& game, int type,
                          const EventData& ev, bool isCopy) {}

    // FROZEN SIGNATURE. Passive rule contributions. Never called through
    // copies (Blueprint doesn't copy passives — matches the game).
    virtual void passiveModifiers(PassiveMods& m) const {}

    virtual ~Joker() = default;

    // instance data (spec §1). name/rarity/baseCost/cloneFn are filled in
    // by the REGISTER_JOKER factory, so every joker knows what it is —
    // Baseball Card needs `rarity`, Invisible Joker needs `clone()`.
    std::string name;
    int rarity = 1;
    std::unique_ptr<Joker> (*cloneFn)(const Joker&) = nullptr;
    // Deep copy INCLUDING scaling state (Invisible Joker; later Ankh/Death).
    std::unique_ptr<Joker> clone() const {
        return cloneFn ? cloneFn(*this) : nullptr;
    }

    int edition = ED_NONE;
    int baseCost = 0;
    int bonusSell = 0;
    bool eternal = false, perishable = false, rental = false;
    bool pendingDestroy = false;    // destruction queue mark
    bool faceDown = false;          // Amber Acorn
    bool disabled = false;          // Crimson Heart
    int sellValue() const { return baseCost / 2 + bonusSell; }
};

// ---------- self-registration ----------
struct JokerInfo {
    std::string name;
    int rarity;   // 1 common, 2 uncommon, 3 rare, 4 legendary
    int cost;
    std::function<std::unique_ptr<Joker>()> make;
};

std::vector<JokerInfo>& jokerRegistry();          // defined in balatro.cpp

struct JokerRegistrar { explicit JokerRegistrar(JokerInfo info); };

#define REGISTER_JOKER(ClassName, name_, rarity_, cost_)                  \
    static JokerRegistrar reg_##ClassName{{name_, rarity_, cost_,          \
        [] {                                                              \
            auto p = std::make_unique<ClassName>();                       \
            p->name = name_;  p->rarity = rarity_;  p->baseCost = cost_;  \
            p->cloneFn = [](const Joker& s) -> std::unique_ptr<Joker> {   \
                return std::make_unique<ClassName>(                       \
                    static_cast<const ClassName&>(s));                    \
            };                                                            \
            return std::unique_ptr<Joker>(std::move(p));                  \
        }}};
