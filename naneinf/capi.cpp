// capi.cpp — flat C ABI for Python (ctypes). No C++ types cross the border.
//
// Build:
//   linux : g++ -std=c++20 -O2 -shared -fPIC capi.cpp <srcs> -o libnaneinf.so
//   win   : x86_64-w64-mingw32-g++ -std=c++20 -O2 -shared -static ... -o naneinf.dll
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <random>
#include "balatro.hpp"
#include "blinds.hpp"
#include "keys.hpp"

#ifdef _WIN32
#define NF_API extern "C" __declspec(dllexport)
#else
#define NF_API extern "C" __attribute__((visibility("default")))
#endif

// ---------------------------------------------------------------- lifecycle
NF_API void* nf_new(const char* seed) {
    std::string s = (seed && *seed) ? std::string(seed) : std::string();
    Balatro* g;
    if (s.empty()) {
        static std::mt19937_64 gen(std::random_device{}());
        g = new Balatro(BalatroRNG::random_seed_string(gen));
    } else {
        for (auto& c : s) c = (char)toupper((unsigned char)c);
        g = new Balatro(s);
    }
    g->enterBlindSelect();
    return g;
}
NF_API void nf_free(void* h) { delete static_cast<Balatro*>(h); }

NF_API void nf_seed(void* h, char* out, int n) {
    const std::string& s = static_cast<Balatro*>(h)->seedString;
    std::snprintf(out, n, "%s", s.c_str());
}

// ---------------------------------------------------------------- actions
NF_API int nf_num_actions() { return NUM_ACTIONS; }

NF_API void nf_legal(void* h, int32_t* out) {
    Balatro* g = static_cast<Balatro*>(h);
    bool mask[NUM_ACTIONS];
    g->legalActions(mask);
    for (int a = 0; a < NUM_ACTIONS; ++a) out[a] = mask[a] ? 1 : 0;
}

NF_API int nf_step(void* h, int action) {
    Balatro* g = static_cast<Balatro*>(h);
    int ok = g->applyAction(action) ? 1 : 0;
    // parity with play.cpp's loop: beating the blind auto-opens the
    // cash-out screen. Without this, the RL/GUI surface stayed stuck in
    // PHASE_PLAYING after the goal was reached (checkDeath early-returns
    // when score >= goal, so the win transition never happened either).
    if (ok && g->phase == PHASE_PLAYING && g->score >= g->blindGoal)
        g->computeCashOut(g->blindIdx);
    return ok;
}

// ---------------------------------------------------------------- scalars
static double log2big(const BigNum& b) {
    if (b.isZero()) return 0.0;
    return (double)b.e + std::log2(b.m);
}

NF_API int    nf_phase(void* h)   { return (int)static_cast<Balatro*>(h)->phase; }
NF_API int    nf_ante(void* h)    { return static_cast<Balatro*>(h)->ante; }
NF_API int    nf_blind(void* h)   { return static_cast<Balatro*>(h)->blindIdx; }
NF_API int    nf_boss(void* h)    { return static_cast<Balatro*>(h)->bossId; }
NF_API int    nf_money(void* h)   { return static_cast<Balatro*>(h)->money; }
NF_API int    nf_hands(void* h)   { return static_cast<Balatro*>(h)->handsLeft; }
NF_API int    nf_discards(void* h){ return static_cast<Balatro*>(h)->discardsLeft; }
NF_API double nf_score_log2(void* h){ return log2big(static_cast<Balatro*>(h)->score); }
NF_API double nf_goal_log2(void* h) { return log2big(static_cast<Balatro*>(h)->blindGoal); }
NF_API int    nf_done(void* h)    { Balatro* g = static_cast<Balatro*>(h);
                                    return (g->phase == PHASE_GAME_OVER || g->won()) ? 1 : 0; }
NF_API int    nf_won(void* h)     { return static_cast<Balatro*>(h)->won() ? 1 : 0; }
NF_API int    nf_naneinf_ante(void* h){ return static_cast<Balatro*>(h)->naneinfAnte; }
NF_API int    nf_cleared8(void* h) { return static_cast<Balatro*>(h)->clearedAnte8() ? 1 : 0; }

// ---- search/rollout support (see clone.cpp) ----
NF_API void* nf_clone(void* h) {
    return static_cast<Balatro*>(h)->cloneGame().release();
}
// Reshuffle the draw pile with an INDEPENDENT generator. Card identities
// are legal knowledge (the in-game deck panel shows them); their order is
// not. Call on a clone before any rollout that draws cards, so imagination
// samples the distribution instead of peeking at the run's true shuffle.
NF_API void nf_reshuffle_draws(void* h, uint64_t s) {
    Balatro* g = static_cast<Balatro*>(h);
    std::mt19937_64 im(s);
    for (int i = (int)g->drawPile.size() - 1; i >= 1; --i) {
        int j = (int)(im() % (uint64_t)(i + 1));
        std::swap(g->drawPile[i], g->drawPile[j]);
    }
}
// Replace the game RNG with a fresh independently-seeded BalatroRNG, so a
// rollout's future shops / packs / probability procs are samples, not the
// run's actual future. Only ever call this on a clone.
NF_API void nf_reseed_rng(void* h, uint64_t s) {
    Balatro* g = static_cast<Balatro*>(h);
    std::mt19937_64 im(s);
    g->rng = std::unique_ptr<RNG>(new BalatroRNG(
        BalatroRNG::random_seed_string(im)));
}
// Live deck composition: rank,suit,enhancement,edition,seal per card that
// still exists (destroyed cards skipped). Buffer needs 5*deck ints.
NF_API int nf_deck(void* h, int32_t* out) {
    Balatro* g = static_cast<Balatro*>(h);
    int n = 0;
    for (const Card& c : g->deck) {
        if (c.destroyed) continue;
        out[n*5+0] = c.rank; out[n*5+1] = c.suit;
        out[n*5+2] = c.enhancement; out[n*5+3] = c.edition;
        out[n*5+4] = c.seal; ++n;
    }
    return n;
}
// reroll pricing — cost is what the NEXT reroll actually charges (0 while a
// Chaos the Clown charge is unspent); free_rerolls is the remaining charges.
NF_API int    nf_reroll_cost(void* h) { return static_cast<Balatro*>(h)->rerollCost(); }
NF_API int    nf_free_rerolls(void* h){ return static_cast<Balatro*>(h)->freeRerolls; }

// ---------------------------------------------------------------- structured
// hand: 7 ints per card -> id, rank, suit, enhancement, edition, seal, flags
// flags bit0 = selected, bit1 = face down, bit2 = debuffed
NF_API int nf_hand(void* h, int32_t* out) {
    Balatro* g = static_cast<Balatro*>(h);
    int n = 0;
    for (size_t i = 0; i < g->hand.size() && i < MAX_HAND; ++i, ++n) {
        Card* c = g->hand[i];
        int flags = (g->sel.hasCard((int)i) ? 1 : 0)
                  | (c->faceDown ? 2 : 0)
                  | (g->isDebuffed(*c) ? 4 : 0);
        int32_t* p = out + n * 7;
        p[0] = c->id;
        p[1] = c->faceDown ? 0 : c->rank;      // hidden info stays hidden
        p[2] = c->faceDown ? -1 : c->suit;
        p[3] = c->faceDown ? 0 : c->enhancement;
        p[4] = c->faceDown ? 0 : c->edition;
        p[5] = c->faceDown ? 0 : c->seal;
        p[6] = flags;
    }
    return n;
}

// jokers: 6 ints -> registry idx, rarity, edition, sellValue, flags, reserved
// flags bit0 = selected, bit1 = face down, bit2 = disabled
NF_API int nf_jokers(void* h, int32_t* out) {
    Balatro* g = static_cast<Balatro*>(h);
    int n = 0;
    for (size_t i = 0; i < g->jokers.size() && i < MAX_JOKERS; ++i, ++n) {
        Joker* j = g->jokers[i].get();
        int idx = -1;
        for (size_t k = 0; k < jokerRegistry().size(); ++k)
            if (jokerRegistry()[k].name == j->name) { idx = (int)k; break; }
        int flags = ((int)i == g->sel.joker ? 1 : 0)
                  | (j->faceDown ? 2 : 0) | (j->disabled ? 4 : 0);
        int32_t* p = out + n * 6;
        p[0] = j->faceDown ? -1 : idx;         // Amber Acorn hides identity
        p[1] = j->faceDown ? 0 : j->rarity;
        p[2] = j->edition;
        p[3] = j->sellValue();
        p[4] = flags;
        p[5] = 0;
    }
    return n;
}

// consumables: 3 ints -> registry idx, kind, selected
NF_API int nf_consumables(void* h, int32_t* out) {
    Balatro* g = static_cast<Balatro*>(h);
    int n = 0;
    for (size_t i = 0; i < g->consumables.size() && i < MAX_CONSUMABLES; ++i, ++n) {
        Consumable* c = g->consumables[i].get();
        int idx = -1;
        for (size_t k = 0; k < consumableRegistry().size(); ++k)
            if (consumableRegistry()[k].name == c->name) { idx = (int)k; break; }
        int32_t* p = out + n * 3;
        p[0] = idx;
        p[1] = c->kind;
        p[2] = ((int)i == g->sel.consumable) ? 1 : 0;
    }
    return n;
}

// shop: 5 ints -> kind(0 joker,1 consumable), registry idx, cost, edition, sold
NF_API int nf_shop(void* h, int32_t* out) {
    Balatro* g = static_cast<Balatro*>(h);
    int n = 0;
    for (size_t i = 0; i < g->shop.size() && i < MAX_SHOP; ++i, ++n) {
        int32_t* p = out + n * 5;
        p[0] = g->shop[i].kind;
        p[1] = g->shop[i].registryIdx;
        p[2] = g->shop[i].cost;
        p[3] = g->shop[i].edition;
        p[4] = g->shop[i].sold ? 1 : 0;
    }
    return n;
}

// packs on offer: 3 ints -> type, size, cost
NF_API int nf_packs(void* h, int32_t* out) {
    Balatro* g = static_cast<Balatro*>(h);
    int n = 0;
    for (size_t i = 0; i < g->packOffers.size() && i < 2; ++i, ++n) {
        int32_t* p = out + n * 3;
        p[0] = g->packOffers[i].type;
        p[1] = g->packOffers[i].size;
        p[2] = g->packOffers[i].cost;
    }
    return n;
}

// open pack contents: 4 ints -> kind, registry idx, rank(for cards), taken
// 8 fields per row. The old 4-field version exposed only `rank` for
// playing cards, making seals/enhancements INVISIBLE to an agent -- fatal
// for any strategy shopping for red-seal or gold/steel cards in Standard
// packs (the only source of them). Widened; nf_pack_contents_width() lets
// a binding detect which layout it is talking to.
NF_API int nf_pack_contents_width() { return 8; }
NF_API int nf_pack_contents(void* h, int32_t* out) {
    Balatro* g = static_cast<Balatro*>(h);
    int n = 0;
    for (size_t i = 0; i < g->packContents.size() && i < 5; ++i, ++n) {
        const PackCard& pc = g->packContents[i];
        int32_t* p = out + n * 8;
        p[0] = pc.kind;
        p[1] = pc.registryIdx;
        p[2] = pc.card.rank;
        p[3] = pc.taken ? 1 : 0;
        p[4] = pc.card.suit;
        p[5] = pc.card.enhancement;
        p[6] = (pc.edition != ED_NONE) ? pc.edition : pc.card.edition;
        p[7] = pc.card.seal;
    }
    return n;
}

NF_API int nf_voucher(void* h)   { return static_cast<Balatro*>(h)->shopVoucher; }
NF_API int nf_skip_tag(void* h)  { return static_cast<Balatro*>(h)->skipTag; }
NF_API int nf_hand_level(void* h, int t) {
    Balatro* g = static_cast<Balatro*>(h);
    return (t >= 0 && t < NUM_HAND_TYPES) ? g->handLevels[t] : 0;
}
NF_API int nf_draw_pile(void* h) { return (int)static_cast<Balatro*>(h)->drawPile.size(); }
NF_API int nf_deck_size(void* h) {
    Balatro* g = static_cast<Balatro*>(h);
    int n = 0;
    for (const Card& c : g->deck) if (!c.destroyed) ++n;
    return n;
}

// ---------------------------------------------------------------- names
NF_API int nf_num_jokers()      { return (int)jokerRegistry().size(); }
NF_API int nf_num_consumables() { return (int)consumableRegistry().size(); }
NF_API const char* nf_joker_name(int i) {
    return (i >= 0 && i < (int)jokerRegistry().size())
        ? jokerRegistry()[i].name.c_str() : "";
}
NF_API int nf_joker_rarity(int i) {
    return (i >= 0 && i < (int)jokerRegistry().size()) ? jokerRegistry()[i].rarity : 0;
}
NF_API const char* nf_consumable_name(int i) {
    return (i >= 0 && i < (int)consumableRegistry().size())
        ? consumableRegistry()[i].name.c_str() : "";
}
NF_API const char* nf_voucher_name(int i) { return voucherName(i); }
NF_API const char* nf_tag_name(int i)     { return tagName(i); }
NF_API const char* nf_boss_name(int i) {
    return (i >= 0 && i < NUM_BOSSES) ? BOSS_DEFS[i].name : "";
}
NF_API const char* nf_hand_name(int t) {
    return (t >= 0 && t < NUM_HAND_TYPES) ? HAND_NAMES[t] : "";
}

// ---------------------------------------------------------------- observation
// A fixed-width float vector. Layout is documented in naneinf.py; keep the
// two in sync. Identities are given as raw indices (divide-by-N normalised)
// so the Python side can use embeddings instead of huge one-hots.
NF_API int nf_obs_size() {
    return 6 + 14                      // phase one-hot + scalars
         + MAX_HAND * 8
         + MAX_JOKERS * 5
         + MAX_CONSUMABLES * 3
         + MAX_SHOP * 4
         + 2 * 3 + 2                   // packs + voucher/tag
         + NUM_HAND_TYPES;             // hand levels
}

NF_API void nf_obs(void* h, float* o) {
    Balatro* g = static_cast<Balatro*>(h);
    int k = 0;
    for (int p = 0; p < 6; ++p) o[k++] = (g->phase == p) ? 1.f : 0.f;
    o[k++] = g->ante / 16.f;
    o[k++] = g->blindIdx / 2.f;
    o[k++] = g->bossId < 0 ? 0.f : (g->bossId + 1) / (float)NUM_BOSSES;
    o[k++] = g->money / 50.f;
    o[k++] = g->handsLeft / 8.f;
    o[k++] = g->discardsLeft / 8.f;
    o[k++] = (float)(log2big(g->score) / 1024.0);
    o[k++] = (float)(log2big(g->blindGoal) / 1024.0);
    o[k++] = g->drawPile.size() / 52.f;
    o[k++] = (float)g->hand.size() / (float)MAX_HAND;
    o[k++] = (float)g->jokers.size() / (float)g->mods.jokerSlots;
    o[k++] = (float)g->consumables.size() / (float)g->mods.consumableSlots;
    o[k++] = g->mods.handSize / 12.f;
    o[k++] = g->won() ? 1.f : 0.f;

    for (int i = 0; i < MAX_HAND; ++i) {
        bool has = i < (int)g->hand.size();
        Card* c = has ? g->hand[i] : nullptr;
        bool hidden = has && c->faceDown;
        o[k++] = has ? 1.f : 0.f;
        o[k++] = (has && !hidden) ? c->rank / 14.f : 0.f;
        o[k++] = (has && !hidden) ? (c->suit + 1) / 4.f : 0.f;
        o[k++] = (has && !hidden) ? c->enhancement / 8.f : 0.f;
        o[k++] = (has && !hidden) ? c->edition / 4.f : 0.f;
        o[k++] = (has && !hidden) ? c->seal / 4.f : 0.f;
        o[k++] = (has && g->sel.hasCard(i)) ? 1.f : 0.f;
        o[k++] = hidden ? 1.f : 0.f;
    }
    for (int i = 0; i < MAX_JOKERS; ++i) {
        bool has = i < (int)g->jokers.size();
        Joker* j = has ? g->jokers[i].get() : nullptr;
        int idx = -1;
        if (has && !j->faceDown)
            for (size_t m = 0; m < jokerRegistry().size(); ++m)
                if (jokerRegistry()[m].name == j->name) { idx = (int)m; break; }
        o[k++] = has ? 1.f : 0.f;
        o[k++] = idx < 0 ? 0.f : (idx + 1) / 150.f;
        o[k++] = has ? j->edition / 4.f : 0.f;
        o[k++] = (has && i == g->sel.joker) ? 1.f : 0.f;
        o[k++] = (has && j->disabled) ? 1.f : 0.f;
    }
    for (int i = 0; i < MAX_CONSUMABLES; ++i) {
        bool has = i < (int)g->consumables.size();
        int idx = -1;
        if (has)
            for (size_t m = 0; m < consumableRegistry().size(); ++m)
                if (consumableRegistry()[m].name == g->consumables[i]->name)
                    { idx = (int)m; break; }
        o[k++] = has ? 1.f : 0.f;
        o[k++] = idx < 0 ? 0.f : (idx + 1) / 52.f;
        o[k++] = (has && i == g->sel.consumable) ? 1.f : 0.f;
    }
    for (int i = 0; i < MAX_SHOP; ++i) {
        bool has = i < (int)g->shop.size() && !g->shop[i].sold;
        o[k++] = has ? 1.f : 0.f;
        o[k++] = has ? (float)g->shop[i].kind : 0.f;
        o[k++] = has ? (g->shop[i].registryIdx + 1) / 150.f : 0.f;
        o[k++] = has ? g->shop[i].cost / 20.f : 0.f;
    }
    for (int i = 0; i < 2; ++i) {
        bool has = i < (int)g->packOffers.size() && !g->packOffers[i].taken;
        o[k++] = has ? 1.f : 0.f;
        o[k++] = has ? (g->packOffers[i].type + 1) / 5.f : 0.f;
        o[k++] = has ? (g->packOffers[i].size + 1) / 3.f : 0.f;
    }
    o[k++] = g->shopVoucher / 32.f;
    o[k++] = g->skipTag / 24.f;
    for (int t = 0; t < NUM_HAND_TYPES; ++t) o[k++] = g->handLevels[t] / 20.f;
}
