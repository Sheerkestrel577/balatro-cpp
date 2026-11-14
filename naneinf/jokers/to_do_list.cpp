// PATTERN: per-round random hand target, pays on match.
//
// Verified against card.lua (balatro-src, asset-main):
//  * set_ability (~L311): initial target rolled when the CARD IS CREATED,
//    pool = VISIBLE hands only, pseudoseed('to_do').
//  * calculate_joker, end_of_round (~L2975): reroll guarded by
//    `not context.blueprint` (copies don't reroll); pool = visible hands
//    EXCLUDING the current target, so the target always changes.
//  * calculate_joker, context.before (~L3491): pays $4 when
//    scoring_name == target. NO blueprint guard -> Blueprint/Brainstorm
//    copies DO pay, keyed to this joker's target. Money lands before
//    scoring (Bull sees it).
//
// ⚠ bit-parity caveat 1: base's pool order is the pairs(G.GAME.hands)
//   iteration order (the table.sort in pseudorandom_element compares the
//   integer array keys — a no-op), i.e. LuaJIT's hash order, NOT enum or
//   alphabetical order. Enum order below must be checked against LuaJIT
//   before trusting seed-exact picks (see rng_selftest).
// ⚠ bit-parity caveat 2: the real game consumes 'to_do' when the shop
//   card is GENERATED (set_ability), even if never bought. We roll at
//   add-to-jokers time. Same-stream picks desync for shop-skipped copies.
#include "../balatro.hpp"
#include "../keys.hpp"

namespace {

class ToDoList : public Joker {
    int want_ = -1;                       // -1 = not yet rolled

    void roll(Balatro& g, bool excludeCurrent) {
        int pool[NUM_HAND_TYPES], n = 0;
        for (int h = 0; h < NUM_HAND_TYPES; ++h)
            if (g.handVisible[h] && !(excludeCurrent && h == want_))
                pool[n++] = h;
        want_ = pool[g.rng->randint(keys::kToDoList(g.ante), 0, n - 1)];
    }

public:
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type == JOKERS_CHANGED) {     // fires right after addJoker()
            if (!isCopy && want_ < 0) roll(g, false);
            return;
        }
        if (type == ROUND_END) {
            if (!isCopy) roll(g, true);   // not context.blueprint
            return;
        }
        if (type != HAND_DETERMINED) return;
        if (g.scoring.handType == want_) g.addMoney(4);   // copies pay too
    }
};
REGISTER_JOKER(ToDoList, "To Do List", 1, 4)

}  // namespace
