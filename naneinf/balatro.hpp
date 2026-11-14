// balatro.hpp — one instance = one self-contained White Stake Plasma run.
#pragma once
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "balatro_rng.hpp"
#include "bignum.hpp"
#include "card.hpp"
#include "events.hpp"
#include "hand_eval.hpp"
#include "blinds.hpp"
#include "joker.hpp"

// Consumables: tarots / planets / spectrals. Targeting reads game.sel.
class Consumable {
public:
    virtual bool canUse(const Balatro& g) const = 0;
    virtual void use(Balatro& g) = 0;
    virtual ~Consumable() = default;
    std::string name;
    int kind = 0;                 // 0 tarot, 1 planet, 2 spectral
    int edition = ED_NONE;
    int baseCost = 3;
    int bonusSell = 0;
    int sellValue() const { return baseCost / 2 + bonusSell; }
};

struct ConsumableInfo {
    std::string name; int kind; int cost;
    std::function<std::unique_ptr<Consumable>()> make;
};
std::vector<ConsumableInfo>& consumableRegistry();
struct ConsumableRegistrar { explicit ConsumableRegistrar(ConsumableInfo i); };
#define REGISTER_CONSUMABLE(ClassName, name_, kind_, cost_)               \
    static ConsumableRegistrar creg_##ClassName{{name_, kind_, cost_,     \
        [] { auto p = std::make_unique<ClassName>();                      \
             p->name = name_; p->kind = kind_; p->baseCost = cost_;       \
             return std::unique_ptr<Consumable>(std::move(p)); }}};

// Game phases. CASH_OUT is a real interactive phase: consumables may be
// used there (blue-seal planets, tarots) BEFORE the shop loads, which is
// strategically important — it avoids blocking consumable slots when the
// first shop's packs arrive.
enum Phase { PHASE_BLIND_SELECT, PHASE_PLAYING, PHASE_CASH_OUT, PHASE_SHOP,
             PHASE_PACK, PHASE_GAME_OVER };

enum PackType { PACK_ARCANA, PACK_CELESTIAL, PACK_STANDARD, PACK_BUFFOON,
                PACK_SPECTRAL, NUM_PACK_TYPES };
enum PackSize { PACKSZ_NORMAL, PACKSZ_JUMBO, PACKSZ_MEGA };

struct PackOffer {
    int type = PACK_ARCANA;
    int size = PACKSZ_NORMAL;
    int cost = 4;
    bool taken = false;
};

// One item inside an opened pack.
struct PackCard {
    int kind = 0;          // 0 consumable, 1 joker, 2 playing card
    int registryIdx = -1;
    Card card;             // for kind 2
    int edition = ED_NONE;
    bool taken = false;
};

const char* voucherName(int id);
const char* tagName(int id);

// All 32 vouchers IN THE GAME'S POOL ORDER (Immolate items.cl). A roll
// picks an index into this list, so the order is load-bearing even for the
// ones whose effects aren't implemented yet.
enum Voucher {
    V_NONE = 0,
    V_OVERSTOCK, V_OVERSTOCK_PLUS, V_CLEARANCE_SALE, V_LIQUIDATION,
    V_HONE, V_GLOW_UP, V_REROLL_SURPLUS, V_REROLL_GLUT,
    V_CRYSTAL_BALL, V_OMEN_GLOBE, V_TELESCOPE, V_OBSERVATORY,
    V_GRABBER, V_NACHO_TONG, V_WASTEFUL, V_RECYCLOMANCY,
    V_TAROT_MERCHANT, V_TAROT_TYCOON, V_PLANET_MERCHANT, V_PLANET_TYCOON,
    V_SEED_MONEY, V_MONEY_TREE, V_BLANK, V_ANTIMATTER,
    V_MAGIC_TRICK, V_ILLUSION, V_HIEROGLYPH, V_PETROGLYPH,
    V_DIRECTORS_CUT, V_RETCON, V_PAINT_BRUSH, V_PALETTE,
    NUM_VOUCHERS
};

// All 24 tags IN THE GAME'S POOL ORDER (Immolate items.cl). Unimplemented
// ones still occupy their slot so the roll distribution stays correct.
enum Tag {
    TAG_NONE = 0,
    TAG_UNCOMMON, TAG_RARE, TAG_NEGATIVE, TAG_FOIL, TAG_HOLOGRAPHIC,
    TAG_POLYCHROME, TAG_INVESTMENT, TAG_VOUCHER, TAG_BOSS, TAG_STANDARD,
    TAG_CHARM, TAG_METEOR, TAG_BUFFOON, TAG_HANDY, TAG_GARBAGE,
    TAG_ETHEREAL, TAG_COUPON, TAG_DOUBLE, TAG_JUGGLE, TAG_D6,
    TAG_TOP_UP, TAG_SPEED, TAG_ORBITAL, TAG_ECONOMY,
    NUM_TAGS
};

// One purchasable slot in the shop.
struct ShopItem {
    int kind = 0;         // 0 joker, 1 consumable, 2 playing card
    int registryIdx = -1; // index into the joker/consumable registry
    int cost = 0;
    int edition = ED_NONE;
    bool sold = false;
};

// ---- interaction layer ----
// Caps are fixed so the AI's action space is fixed-width; illegal indices
// are masked per-state rather than resized.
enum { MAX_HAND = 12, MAX_JOKERS = 10, MAX_CONSUMABLES = 4, MAX_SHOP = 6 };

// The player's cursor. Consumable targeting reads `cards` directly, so
// "Death with exactly 2 selected" needs no separate targeting mechanism.
struct Selection {
    std::vector<int> cards;   // indices into hand, in SELECTION order
    int joker = -1;           // index into jokers, -1 = none
    int consumable = -1;      // index into consumables, -1 = none
    bool hasCard(int i) const {
        for (int x : cards) if (x == i) return true;
        return false;
    }
};

enum Action {
    A_TOGGLE_CARD_0 = 0,
    A_TOGGLE_JOKER_0 = A_TOGGLE_CARD_0 + MAX_HAND,
    A_TOGGLE_CONSUMABLE_0 = A_TOGGLE_JOKER_0 + MAX_JOKERS,
    A_SHIFT_CARD_LEFT = A_TOGGLE_CONSUMABLE_0 + MAX_CONSUMABLES,
    A_SHIFT_CARD_RIGHT,
    A_SHIFT_JOKER_LEFT,
    A_SHIFT_JOKER_RIGHT,
    A_PLAY,
    A_DISCARD,
    A_USE_CONSUMABLE,
    A_SELL_CONSUMABLE,
    A_SELL_JOKER,
    A_CONFIRM_CASHOUT,
    A_CLEAR_SELECTION,
    A_SELECT_BLIND,
    A_SKIP_BLIND,
    A_BUY_SLOT_0,
    A_BUY_VOUCHER = A_BUY_SLOT_0 + MAX_SHOP,
    A_REROLL_SHOP,
    A_LEAVE_SHOP,
    A_REROLL_BOSS,          // Director's Cut / Retcon
    A_HOLD_JOKER,           // Amber Acorn: exempt the selected joker
    A_BUY_PACK_0,
    A_PICK_PACK_0 = A_BUY_PACK_0 + 2,
    A_SKIP_PACK = A_PICK_PACK_0 + 5,
    NUM_ACTIONS
};

// Per-hand mutable scoring state (spec §5). Jokers read/write via game.
struct ScoringCtx {
    BigNum chips, mult;               // SEPARATE until plasma balance:
                                      // xMult multiplies mult ONLY
    int handType = HIGH_CARD;
    uint16_t containedHands = 0;      // "contains" jokers read THIS
    std::vector<Card*> played;        // ALL played cards (Half Joker cares
                                      //   about played count, not scored)
    std::vector<Card*> scored, held;
    int retriggers = 0;               // accumulator for the CURRENT card
    Card* currentCard = nullptr;      // the card currently triggering
    bool firstHandOfRound = false;
};

class Balatro {
public:
    // ---- run state: all of it lives here, no globals ----
    int money = 4;
    int ante = 1;
    int handsLeft = 4, discardsLeft = 3;
    // Score RESETS every blind (real Balatro zeroes chips per blind).
    // naneinf is therefore a per-blind overflow.
    BigNum score;                     // this blind's score
    BigNum bestBlindScore;            // best single blind, for stats
    bool naneinfReached = false;      // sticky: latched the moment it happens
    int naneinfAnte = 0;              // which ante it happened on
    // deque, not vector: pointers into it stay valid when the deck grows
    // (tarots, packs, DNA). NEVER change this to a vector.
    std::deque<Card> deck;            // owning storage of every card
    std::vector<Card*> drawPile, hand, discardPile;
    std::vector<std::unique_ptr<Joker>> jokers;
    std::unique_ptr<RNG> rng;
    PassiveMods mods;                 // recomputed, never hand-edited
    ScoringCtx scoring;

    // poker hand levels (planet upgrades; The Arm decrements)
    int handLevels[NUM_HAND_TYPES] = {1,1,1,1,1,1,1,1,1,1,1,1};

    bool bossDisabled = false;        // set by Luchador on sell / rerolls
    // Chicot disables passively; blinds should consult this, not the flag.
    bool bossIsDisabled() const { return bossDisabled || mods.disableBoss; }
    int discardsUsedThisRound = 0;    // Delayed Gratification
    int investmentTagPending = 0;     // Investment Tag ($25 after boss)

    // run counters (spec §5; not frozen — extend freely)
    int tarotsUsed = 0, blindsSkipped = 0, handsPlayedTotal = 0;
    int uniquePlanetsUsed = 0;        // Satellite (count of distinct planets)
    bool planetUsed[NUM_HAND_TYPES] = {};
    int lastTarotIdx = -1;            // The Fool
    int lastPlanetIdx = -1;
    int cardsSoldThisRun = 0;         // Campfire
    int cardsDiscardedThisRound = 0;  // Yorick, Hit the Road
    int handPlayCount[NUM_HAND_TYPES] = {};  // Supernova, Card Sharp, Obelisk
    // game.lua init_game_object: all hands visible=true EXCEPT the secret
    // hands (Five of a Kind, Flush House, Flush Five), which flip to
    // visible when played this run (state_events.lua evaluate_play).
    // Consumers: To Do List's target pool.
    bool handVisible[NUM_HAND_TYPES] =
        {true,true,true,true,true,true,true,true,true, false,false,false};
    Phase phase = PHASE_BLIND_SELECT;
    int glassDestroyedTotal = 0, facesDestroyedTotal = 0;

    // pool flags (game.lua: G.GAME.pool_flags). Jokers carry no_pool_flag /
    // yes_pool_flag and are marked UNAVAILABLE — not removed — when gated,
    // so pool INDICES never shift and seed parity is preserved.
    //   Gros Michel: no_pool_flag  'gros_michel_extinct'  (leaves pools)
    //   Cavendish:   yes_pool_flag 'gros_michel_extinct'  (enters pools)
    bool grosMichelExtinct = false;

    explicit Balatro(std::unique_ptr<RNG> r);

    // Seed-exact construction: an 8-char Balatro seed string drives the
    // real pseudohash/LCG/LuaJIT stack (BalatroRNG, bit-verified vs LuaJIT).
    // `seedString` is kept so a winning run can be typed into the real game.
    explicit Balatro(const std::string& seed8)
        : Balatro(std::unique_ptr<RNG>(new BalatroRNG(seed8))) {
        seedString = seed8;
    }
    // Roll a random 8-char seed the way an unseeded run does.
    static Balatro unseeded(std::mt19937_64& g) {
        return Balatro(BalatroRNG::random_seed_string(g));
    }
    std::string seedString;           // empty when using MersenneRNG

    // ---- shop / ante state ----
    std::vector<ShopItem> shop;
    int shopVoucher = V_NONE;       // one per ante, first shop only
    bool voucherBought = false;
    // ---- reroll pricing (common_events.lua calculate_reroll_cost) ----
    // cost = freeRerolls > 0 ? 0 : (base + increase)
    //  base     : 5, reduced $2 per Reroll Surplus / Reroll Glut owned
    //  increase : +1 per PAID reroll, reset each round; a FREE reroll does
    //             not escalate it (reroll_shop passes skip_increment when
    //             the reroll it just spent was free)
    //  freeRerolls: a CHARGE COUNTER, not a per-shop flag. Set to the number
    //             of Chaos the Clown jokers at round start, +1 on acquiring
    //             one, -1 on losing one, -1 (floored at 0) per reroll. This
    //             is why buying Chaos mid-shop grants a reroll immediately,
    //             and why sell-then-rebuy grants another in the same shop.
    int rerollCostIncrease = 0;
    int freeRerolls = 0;
    int rerollBaseCost() const {
        int base = 5;
        if (ownedVouchers[V_REROLL_SURPLUS]) base -= 2;
        if (ownedVouchers[V_REROLL_GLUT])    base -= 2;
        return base < 0 ? 0 : base;
    }
    int rerollCost() const {
        return freeRerolls > 0 ? 0 : rerollBaseCost() + rerollCostIncrease;
    }
    // Chaos entering/leaving play adjusts the counter live (card.lua
    // add_to_deck / remove_from_deck).
    int countJokersNamed(const std::string& nm) const {
        int n = 0;
        for (auto& j : jokers) if (j->name == nm && !j->pendingDestroy) ++n;
        return n;
    }
    void addFreeRerolls(int d) {
        freeRerolls += d;
        if (freeRerolls < 0) freeRerolls = 0;
    }
    int rerollsThisShop = 0;
    bool ownedVouchers[NUM_VOUCHERS] = {};
    std::vector<int> pendingTags;   // applied at their trigger points
    std::vector<PackOffer> packOffers;   // the 2 packs in the shop
    std::vector<PackCard> packContents;  // the pack currently open
    int packPicksLeft = 0;
    int openPackType = -1;
    bool dealtForPack = false;
    int blindIdx = 0;               // 0 small, 1 big, 2 boss
    // ---- boss blind state ----
    int  bossId = -1;
    int  bossAppearances[NUM_BOSSES] = {};   // "fewest appearances" rule
    int  bossRerollsThisAnte = 0;
    int  anteVoucher = V_NONE;      // rolled at ante start
    int  anteTagA = TAG_NONE, anteTagB = TAG_NONE;
    int  shopsThisAnte = 0;
    bool anteStarted = false;
    bool generatedFirstPack = false;   // first pack of the run is Buffoon
    bool flintActive = false;                // The Flint
    bool verdantActive = false;              // Verdant Leaf (until joker sold)
    bool drawFaceDownNextDraw = false;       // The House / The Fish
    int handSortMode = 0;                    // 0 = rank desc, 1 = suit desc
    int  crimsonDisabledJoker = -1;          // Crimson Heart
    int  ceruleanForcedCard = -1;            // Cerulean Bell (card id)
    // Per-round joker targets, rolled ONCE at round start regardless of
    // whether the relevant joker is owned (mirrors reset_mail_rank /
    // reset_idol_card in Balatro — the 'mail'/'idol' streams advance every
    // round). The jokers just READ these.
    int  mailCardRank = 14;                  // Mail-In Rebate (14=Ace default)
    int  idolCardRank = 14, idolCardSuit = SPADES;   // The Idol
    // Castle's target suit (deck-weighted pick each round; Spades default).
    int  castleCardSuit = SPADES;
    // Ancient Joker's target suit. Picked from the 4 suits MINUS the current
    // one, so it never repeats round-to-round; -1 = none yet (first pick sees
    // all 4). Persists across rounds — only reset at construction.
    int  ancientCardSuit = -1;
    int  heldJokerIdx = -1;                  // Amber Acorn "hold"
    int  mouthLockedType = -1;               // The Mouth
    bool handTypePlayedThisRound[NUM_HAND_TYPES] = {};
    bool handTypePlayedBeforeThisHand = false;   // Card Sharp
    int  handsPlayedThisRound = 0;               // DNA
    int  bossTriggeredThisHand = 0;          // Matador
    int  pendingDoubleTags = 0;              // Double tag
    int  unusedDiscardsThisRun = 0;          // Garbage tag

    void bossTriggered();
    void rotateCrimsonHeart();
    void rollCeruleanForce();
    int  mostPlayedHand() const;
    int  selectionHandType() const;          // -1 if unknown/empty
    void rollBoss();                         // fewest-appearances rule
    void rerollBoss();
    double bossMultiplier(int id) const { return BOSS_DEFS[id].mult; }
    bool bossActive() const { return blindIdx == 2 && bossId >= 0
                                     && !bossIsDisabled(); }
    // Debuff is an INTERPRETATION (spec §5): never stored for boss effects.
    bool isDebuffed(const Card& c) const;
    BigNum blindGoal;
    bool blindSkippable() const { return blindIdx < 2; }

    // ---- interaction layer ----
    Selection sel;
    std::vector<std::unique_ptr<Consumable>> consumables;

    // Fixed-width legality mask; the AI samples only over legal actions.
    void legalActions(bool out[NUM_ACTIONS]) const;
    bool isLegal(int action) const;
    bool applyAction(int action);     // false if illegal (no state change)

    // ---- deck / round mechanics ----
    int nextCardId = 0;
    Card& addCard(Card c);            // assigns id, fires CARD_ADDED
    Card& addCardSilent(Card c);      // no event (deck construction)
    Card* cardById(int id);
    void startRound();                // rebuild draw pile, shuffle, draw up
    // Picks ONE card from the deck the way Balatro's pseudorandom_element
    // does: candidates = all non-Stone, non-destroyed playing cards in
    // sort_id (creation) order; one draw of math.random(N) over that list.
    // Weighted by deck composition, NOT uniform over ranks. nullptr if none.
    Card* pickDeckCard(const std::string& key);
    // Roll the per-round joker targets (Mail-In Rebate, The Idol). Called
    // unconditionally at round start so the streams stay aligned with the
    // real game even when the joker isn't owned yet.
    void resetRoundTargets();
    // ---- ante / shop loop ----
    void beginAnte();                 // boss -> voucher -> 2 tags
    int  rollTag();
    void enterBlindSelect();          // sets blindGoal, offers the skip tag
    void selectBlind();               // -> PHASE_PLAYING
    void skipBlind();                 // take the tag, advance
    void openShop();                  // generate slots (dedicated RNG keys)
    void leaveShop();                 // -> next blind select
    void buyShopItem(int slot);
    void buyVoucher();
    void rerollShop();
    void buyPack(int which);
    void pickPackCard(int i);
    void skipPack();
    void applyTag(int tag);
    void grantConsumable(int registryIdx, int edition = ED_NONE);
    void useConsumableNow(std::unique_ptr<Consumable> con);
    bool consumableUsableNow(int registryIdx) const;
    // helpers the consumables call
    void clearSelectionAfterUse() { sel.cards.clear(); }
    void runDestructionWindow();
    void createRandomConsumables(int kind, int n, const char* src);
    void createRandomJoker(const char* src);
    void levelUpHand(int handType);
    void createJokerOfRarity(int rarity, const char* src);
    void spectralReplace(int add, int kind);   // 0 aces, 1 numbered, 2 face
    void copyConsumable(int idx, int edition);   // Perkeo
    Card randomPlayingCard(const std::string& key);
    void rerollShop_unused();
    void rollShopSlot(int i);
    ShopItem nextShopItem();          // draw the next item from the ante queue
    int  drawFromPool(const std::string& key, const std::vector<int>& pool,
                      std::vector<int>& taken, bool isConsumablePool = false);
    bool consumableLocked(int registryIdx) const;
    bool jokerAvailable(int registryIdx) const;
    bool jokerInPool(int registryIdx) const;   // yes_/no_pool_flag gate
    bool consumableAvailable(int registryIdx) const;
    void applyVouchers();
    // Voucher pool draw (spec §6): mirrors Balatro's get_next_voucher_key.
    // A slot is UNAVAILABLE if already owned, or if it is an upgrade whose
    // base voucher is not owned this run; the draw resamples past those,
    // exactly like the real game. `id` is a 1-based V_* voucher id.
    bool voucherInPool(int id) const;
    int  rollVoucher(const std::string& key);   // returns a 1-based V_* id
    int  skipTag = TAG_NONE;          // tag offered for skipping this blind
    bool blindBeaten() const { return score >= blindGoal; }
    void drawToHandSize();
    void drawCards(int n);
    void sortHand();
    int permanentHandSizeBonus = 0;   // earned via multi-Chicot on Manacle
    int effectiveHandSize() const {
        int n = mods.handSize + permanentHandSizeBonus;
        if (bossActive() && bossId == BOSS_MANACLE) --n;
        return n < 1 ? 1 : n;
    }

    // ---- event broadcast (order = effect order) ----
    void broadcast(int type, const EventData& ev) {
        // Index loop over a SNAPSHOT of the size: a joker's interact may
        // ADD jokers (Riff-Raff on blind select), and push_back can
        // reallocate the vector -- a range-for here is a use-after-free.
        // Jokers added mid-broadcast do not see the event that created
        // them. (Erase is likewise forbidden: queue + sweep, spec §4.)
        const size_t n = jokers.size();
        for (size_t i = 0; i < n; ++i)
            if (!jokers[i]->pendingDestroy)
                jokers[i]->interact(*this, type, ev, /*isCopy=*/false);
    }

    // Copy-joker resolution with the depth guard (pigeonhole: any chain
    // longer than the joker count contains a cycle -> no effect).
    void invokeAsCopy(Joker& target, int type, const EventData& ev) {
        if (copyDepth_ >= (int)jokers.size()) return;
        ++copyDepth_;
        target.interact(*this, type, ev, /*isCopy=*/true);
        --copyDepth_;
    }

    // ---- money (spec: debt floor is -creditLimit, Credit Card) ----
    // During cash-out COMPUTATION, additions are diverted into the pending
    // payout instead of the wallet. That is what stops The Hermit (used on
    // the cash-out screen) from doubling Golden Joker / To the Moon money —
    // an exploit the real game does not allow.
    void addMoney(int amount) {
        if (freezingPayout_) pendingPayout_ += amount; else money += amount;
    }
    bool canAfford(int cost) const { return money - cost >= -mods.creditLimit; }
    void spendMoney(int cost) { money -= cost; }   // caller checks canAfford

    // ---- cash out: a PHASE, not a single call ----
    struct CashOut {
        int blindReward = 0, handsBonus = 0, goldCards = 0;
        int jokerTotal = 0, investmentTag = 0, interest = 0;
        int total() const { return blindReward + handsBonus + goldCards
                                 + jokerTotal + investmentTag + interest; }
    };
    // 1) Round ends: compute and FREEZE the itemized list. Nothing is
    //    credited yet. Enters PHASE_CASH_OUT, where the AI may still use
    //    consumables / sell jokers. The frozen interest line does NOT
    //    recompute if money changes afterwards (user-confirmed).
    //    blindIdx: 0 small, 1 big, 2 boss, 3 showdown boss.
    const CashOut& computeCashOut(int blindIdx);
    // 2) Leaving for the shop: credit the frozen total, reset the round.
    void confirmCashOut();
    const CashOut& pendingCashOut() const { return pending_; }

    // ---- interpretation helpers (spec §5: facts vs interpretations) ----
    bool isFace(const Card& c) const {
        if (c.enhancement == ENH_STONE) return false;
        return mods.allFaces || (c.rank >= 11 && c.rank <= 13);
    }
    uint8_t suitMask(const Card& c) const {   // scoring-time: debuff-aware
        if (c.debuffed) return 0;
        return detSuitMask(c, mods);
    }
    bool suitMatches(const Card& c, Suit s) const {
        return suitMask(c) & (1u << s);
    }
    // Flower Pot's question (⚠ vs possible in-game greedy — spec §5)
    bool suitCoverPossible(const std::vector<Card*>& cs) const;

    // probability roll; Oops-aware. Fire LUCKY_TRIGGERED at call sites
    // that represent Lucky card procs.
    bool chance(const std::string& key, int num, int den) {
        return rng->random(key) < (double)(num * mods.probNumeratorMult) / den;
    }

    // ---- passives ----
    void recomputeMods();             // clears to defaults, asks every joker

    // ---- joker set management (queue + sweep) ----
    void addJoker(std::unique_ptr<Joker> j);
    void queueDestroyJoker(Joker& j) { j.pendingDestroy = true; }
    void sweepJokers();               // erase pendingDestroy, fire JOKERS_CHANGED

    // ---- player-driven round actions ----
    BigNum playSelected();            // play the selected cards
    void   discardSelected();         // discard them (fires DISCARD events)
    void   checkDeath();              // out of hands & goal unmet -> death
    bool   mrBonesSaves();            // >=25% of goal: survive, self-destruct

    // ---- the pipeline (spec §4). Returns this hand's banked score. ----
    BigNum playHand(const std::vector<Card*>& played,
                    const std::vector<Card*>& heldAfter);

    // ---- blind requirements (spec §8c) ----
    // small=0, big=1, boss=2. Plasma x2 applied. BigNum for deep endless.
    BigNum blindRequirement(int ante, int blindIdx) const;

    bool won() const { return naneinfReached; }
    // Cut-off for evaluation: "did this run clear ante 8?"
    bool clearedAnte8() const { return ante > 8; }

    // Deep copy for search/rollouts (clone.cpp). Copies EVERY member,
    // remaps interior Card* by id, clones jokers via cloneFn, rebuilds
    // consumables from the registry, clones the RNG. If you add a member
    // to this class, add it to clone.cpp or test_clone will catch you.
    std::unique_ptr<Balatro> cloneGame() const;

private:
    int copyDepth_ = 0;
    bool freezingPayout_ = false;
    int pendingPayout_ = 0;
    CashOut pending_;
    std::vector<Card*> destroyQueue_;
    void queueDestroyCard(Card& c) { destroyQueue_.push_back(&c); }
    void destructionWindow();         // spec §4 step 12
    friend class SixthSenseLikeAccess; // (placeholder for tests)
public:
    // Returns TRUE only if this call is the one that claimed the card.
    // False if it is already destroyed or already queued by someone else —
    // which is how "effect tied to the destruction" jokers (Sixth Sense)
    // avoid double-firing, while ungated ones (Trading Card) ignore it.
    bool destroyCardLater(Card& c) {
        if (c.destroyed) return false;
        for (Card* q : destroyQueue_) if (q == &c) return false;
        destroyQueue_.push_back(&c);
        return true;
    }
};
