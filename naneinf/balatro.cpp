// balatro.cpp — definitions that exist exactly once.
#include <cstdio>
#include "balatro.hpp"

#include "blinds.hpp"
#include "keys.hpp"
#include "pools.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

// ---------------- registry storage ----------------
std::vector<JokerInfo>& jokerRegistry() {
    static std::vector<JokerInfo> r;
    return r;
}
JokerRegistrar::JokerRegistrar(JokerInfo info) {
    jokerRegistry().push_back(std::move(info));
}

// ---------------- construction ----------------
Balatro::Balatro(std::unique_ptr<RNG> r) : rng(std::move(r)) {
    // THE GAME'S DECK_ORDER (Immolate items.cl): suits run Clubs, Diamonds,
    // Hearts, Spades, and within a suit the ranks are in ALPHABETICAL order
    // of their symbol: 2 3 4 5 6 7 8 9 A J K Q T. Not numeric order, and
    // not spades-first. The shuffle permutes THIS array, so getting it wrong
    // gives a correctly-shuffled wrong deck.
    static const int SUIT_ORDER[4] = {CLUBS, DIAMONDS, HEARTS, SPADES};
    static const int RANK_ORDER[13] = {2, 3, 4, 5, 6, 7, 8, 9,
                                       14, 11, 13, 12, 10};  // A J K Q T
    for (int s : SUIT_ORDER)
        for (int rk : RANK_ORDER) {
            Card c; c.rank = rk; c.suit = s;
            addCardSilent(c);
        }
    recomputeMods();
}

Card& Balatro::addCardSilent(Card c) {
    c.id = nextCardId++;
    deck.push_back(c);                 // deque: existing Card* stay valid
    return deck.back();
}

Card& Balatro::addCard(Card c) {
    Card& ref = addCardSilent(c);
    EventData ev{}; ev.card = &ref;
    broadcast(CARD_ADDED, ev);         // Hologram
    return ref;
}

Card* Balatro::cardById(int id) {
    for (Card& c : deck) if (c.id == id) return &c;
    return nullptr;
}

Card* Balatro::pickDeckCard(const std::string& key) {
    // Build the candidate list exactly like Balatro: every playing card that
    // is not a Stone Card and not destroyed, then sort by sort_id (this
    // engine's Card::id is the sort_id equivalent — assigned in creation
    // order). pseudorandom_element re-sorts by sort_id, so we must too.
    std::vector<Card*> valid;
    for (Card& c : deck)
        if (!c.destroyed && c.enhancement != ENH_STONE) valid.push_back(&c);
    std::sort(valid.begin(), valid.end(),
              [](const Card* a, const Card* b) { return a->id < b->id; });
    if (valid.empty()) return nullptr;
    // pseudorandom_element does math.random(#list) -> 1-based index; our
    // randint(key, 1, N) is bit-identical to that.
    int i = rng->randint(key, 1, (int)valid.size());
    if (i < 1) i = 1;
    if (i > (int)valid.size()) i = (int)valid.size();
    return valid[i - 1];
}

void Balatro::resetRoundTargets() {
    // Mail-In Rebate: reset to Ace, then pick a deck card's rank.
    // (reset_mail_rank in Balatro.) Drawn every round, joker or not.
    mailCardRank = 14;                                // 'Ace'
    if (Card* c = pickDeckCard(keys::kMailRebate(ante))) mailCardRank = c->rank;
    // The Idol: pick a deck card; take BOTH its rank and suit (they come from
    // the same card, so they are correlated). reset_idol_card keeps the
    // previous target if the deck is empty, so only overwrite on a hit.
    if (Card* c = pickDeckCard(keys::kIdol(ante))) {
        idolCardRank = c->rank;
        idolCardSuit = c->suit;
    }
    // Ancient Joker: choose a suit from {Spades,Hearts,Clubs,Diamonds} MINUS
    // the current one (reset_ancient_card, key 'anc'..ante). The suit list is
    // strings in Balatro, so pseudorandom_element sorts by array index —
    // i.e. keeps enum order S,H,C,D, which this engine's Suit enum matches.
    {
        std::vector<int> anc;
        for (int s = 0; s < 4; ++s) if (s != ancientCardSuit) anc.push_back(s);
        int i = rng->randint(keys::kAncient(ante), 1, (int)anc.size());
        if (i < 1) i = 1;
        if (i > (int)anc.size()) i = (int)anc.size();
        ancientCardSuit = anc[i - 1];
    }
    // Castle: like Idol/Mail but reads the picked card's SUIT; defaults to
    // Spades if the deck is empty (reset_castle_card, key 'cas'..ante).
    castleCardSuit = SPADES;
    if (Card* c = pickDeckCard(keys::kCastle(ante))) castleCardSuit = c->suit;
}

void Balatro::startRound() {
    // NOTE: `deck` (the deque) is NEVER shuffled — shuffling it would move
    // Card contents between slots and silently repoint every Card*. We build
    // a pointer vector and shuffle that instead.
    drawPile.clear(); hand.clear(); discardPile.clear();
    for (Card& c : deck) if (!c.destroyed) { c.playedThisAnte = false;
                                             drawPile.push_back(&c); }
    std::vector<int> idx(drawPile.size());
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = (int)i;
    rng->shuffle(keys::shuffle(ante), idx);
    std::vector<Card*> shuffled;
    shuffled.reserve(idx.size());
    for (int i : idx) shuffled.push_back(drawPile[i]);
    drawPile.swap(shuffled);
    handsLeft = mods.hands;
    discardsLeft = mods.discards;
    discardsUsedThisRound = 0;
    cardsDiscardedThisRound = 0;
    sel = Selection{};
    score = BigNum{};                     // SCORE RESETS EVERY BLIND
    flintActive = false;
    verdantActive = false;
    drawFaceDownNextDraw = false;
    crimsonDisabledJoker = -1;
    ceruleanForcedCard = -1;
    mouthLockedType = -1;
    for (int i = 0; i < NUM_HAND_TYPES; ++i) handTypePlayedThisRound[i] = false;
    handTypePlayedBeforeThisHand = false;
    handsPlayedThisRound = 0;
    for (auto& j : jokers) j->disabled = false;
    resetRoundTargets();                  // roll Mail/Idol targets for this round
    drawToHandSize();
    phase = PHASE_PLAYING;
    if (bossActive()) boss::onSelect(*this, bossId);
    else if (blindIdx == 2 && bossId >= 0 && mods.chicotCount > 0) {
        // Chicot INVERTS numeric bosses, once per copy (wiki-documented).
        int n = mods.chicotCount;
        if (bossId == BOSS_WATER)   discardsLeft = mods.discards * n;
        if (bossId == BOSS_NEEDLE)  handsLeft = 1 + (mods.hands - 1) * n;
        if (bossId == BOSS_MANACLE) permanentHandSizeBonus += (n - 1);
        // Wall / Violet Vessel are handled in blindRequirement().
        // ⚠ user-reported: The Ox may add money per copy before zeroing —
        // undocumented on the wiki, not implemented.
    }
    broadcast(ROUND_START, EventData{});
}

void Balatro::drawToHandSize() { drawCards(effectiveHandSize() - (int)hand.size()); }

// Balatro keeps the hand sorted; the order is what index-based actions
// select against, so it must match the game's display exactly.
// Card:get_nominal(): nominal + suit_nominal*(mult) + tiny tiebreaks, where
// suit_nominal is Spades .04 > Hearts .03 > Clubs .02 > Diamonds .01.
//   rank sort ('desc'):      rank first, equal ranks tiebreak by suit S>H>C>D
//   suit sort ('suit desc'): suit_nominal*1000 dominates -> S,H,C,D groups,
//                            rank desc within each suit
// (Random in-hand pickers — Hook, Cerulean Bell, spectral replacers — do
// NOT use display order: pseudorandom_element sorts by sort_id first.)
void Balatro::sortHand() {
    auto suitVal = [](const Card* c) {          // S=4 H=3 C=2 D=1
        return 4 - c->suit;
    };
    if (handSortMode == 1) {
        std::stable_sort(hand.begin(), hand.end(),
            [&](const Card* a, const Card* b) {
                if (suitVal(a) != suitVal(b)) return suitVal(a) > suitVal(b);
                return a->rank > b->rank;
            });
    } else {
        std::stable_sort(hand.begin(), hand.end(),
            [&](const Card* a, const Card* b) {
                if (a->rank != b->rank) return a->rank > b->rank;
                return suitVal(a) > suitVal(b);
            });
    }
}

void Balatro::drawCards(int n) {
    for (int i = 0; i < n && !drawPile.empty(); ++i) {
        Card* c = drawPile.back();
        drawPile.pop_back();
        c->faceDown = false;
        if (bossActive()) {
            if (drawFaceDownNextDraw) c->faceDown = true;              // House/Fish
            if (bossId == BOSS_MARK && isFace(*c)) c->faceDown = true; // The Mark
            if (bossId == BOSS_WHEEL
                && chance(keys::kWheelFaceDown(), 1, 7)) c->faceDown = true; // The Wheel
        }
        hand.push_back(c);
    }
    drawFaceDownNextDraw = false;
    sortHand();
}

// ---------------- passives ----------------
void Balatro::recomputeMods() {
    mods = PassiveMods{};                 // defaults
    for (auto& j : jokers)
        if (!j->pendingDestroy)
            j->passiveModifiers(mods);    // never through copies
    // vouchers will write here too (spec §6)
}

void Balatro::addJoker(std::unique_ptr<Joker> j) {
    jokers.push_back(std::move(j));
    recomputeMods();
    broadcast(JOKERS_CHANGED, EventData{});
}

void Balatro::sweepJokers() {
    auto it = std::remove_if(jokers.begin(), jokers.end(),
        [](const std::unique_ptr<Joker>& j) { return j->pendingDestroy; });
    if (it != jokers.end()) {
        jokers.erase(it, jokers.end());
        recomputeMods();
        broadcast(JOKERS_CHANGED, EventData{});
    }
}

// ---------------- Flower Pot's cover check ----------------
bool Balatro::suitCoverPossible(const std::vector<Card*>& cs) const {
    // assign each card one suit from its mask, cover all 4 (<=5 cards:
    // brute force). ⚠ real game may be greedy — verify via BalatroBot.
    int n = (int)cs.size();
    std::vector<uint8_t> masks(n);
    for (int i = 0; i < n; ++i) masks[i] = suitMask(*cs[i]);
    std::function<bool(int, uint8_t)> go = [&](int i, uint8_t covered) {
        if (covered == 0b1111) return true;
        if (i == n) return false;
        for (int s = 0; s < 4; ++s)
            if (masks[i] & (1u << s))
                if (go(i + 1, covered | (uint8_t)(1u << s))) return true;
        return go(i + 1, covered);
    };
    return go(0, 0);
}

// ---------------- the pipeline (spec §4) ----------------
BigNum Balatro::playHand(const std::vector<Card*>& played,
                         const std::vector<Card*>& heldAfter) {
    assert(!played.empty() && played.size() <= 5);
    ++handsPlayedTotal;
    bossTriggeredThisHand = 0;

    // step 1: hand evaluation (contains semantics)
    HandEvalResult ev = evaluateHand(played, mods);
    scoring = ScoringCtx{};
    scoring.handType = ev.scoredType;
    scoring.played = played;
    ++handPlayCount[ev.scoredType];
    handVisible[ev.scoredType] = true;   // evaluate_play: hands[text].visible = true
    ++handsPlayedThisRound;
    handTypePlayedBeforeThisHand = handTypePlayedThisRound[ev.scoredType];
    scoring.containedHands = ev.containedHands;
    for (int i : ev.scoringIdx) scoring.scored.push_back(played[i]);
    scoring.held = heldAfter;

    // step 2: boss zero-score gate (The Eye repeat / The Mouth mismatch).
    // Author's ruling: such hands ARE playable, for zero score. What runs:
    // hand counters above (Telescope/Supernova tick), BOSS_TRIGGERED
    // (Matador pays), hand consumed, cards discarded, death check. What
    // does NOT run: the scoring pipeline -- no HAND_DETERMINED, no
    // INDEPENDENT, no card/held triggers -- so Obelisk, Ride the Bus,
    // and Green Joker neither reset nor grow.
    if (boss::zeroesHand(*this, scoring.handType)) {
        bossTriggered();                    // Matador pays, counter ticks
        --handsLeft;
        checkDeath();
        return BigNum{};                    // zero score, hand consumed
    }

    // step 3: HAND_DETERMINED broadcast
    EventData ed{};
    ed.handType = scoring.handType;
    broadcast(HAND_DETERMINED, ed);

    // step 4: base chips/mult from hand type & level
    // (level table values: standard wiki numbers; verify at data entry ⚠)
    // VERIFIED against the planet-card table (Pluto..Eris): base chips,
    // base mult, and the per-level gain each planet grants.
    static const int base[NUM_HAND_TYPES][4] = {
        //  chips mult  c/lvl m/lvl        planet
        {   5,  1, 15, 1},   // high card       Pluto   +15c +1m
        {  10,  2, 15, 1},   // pair             Mercury +15c +1m
        {  20,  2, 20, 1},   // two pair         Uranus  +20c +1m
        {  30,  3, 20, 2},   // three oak        Venus   +20c +2m
        {  30,  4, 30, 3},   // straight         Saturn  +30c +3m
        {  35,  4, 15, 2},   // flush            Jupiter +15c +2m
        {  40,  4, 25, 2},   // full house       Earth   +25c +2m
        {  60,  7, 30, 3},   // four oak         Mars    +30c +3m
        { 100,  8, 40, 4},   // straight flush   Neptune +40c +4m
        { 120, 12, 35, 3},   // five oak         PlanetX +35c +3m
        { 140, 14, 40, 4},   // flush house      Ceres   +40c +4m
        { 160, 16, 50, 3},   // flush five       Eris    +50c +3m
    };
    int lvl = handLevels[scoring.handType];
    double baseC = base[scoring.handType][0] + (lvl - 1) * base[scoring.handType][2];
    double baseM = base[scoring.handType][1] + (lvl - 1) * base[scoring.handType][3];
    if (flintActive) { baseC /= 2.0; baseM /= 2.0; }     // The Flint
    scoring.chips = BigNum(baseC);
    scoring.mult  = BigNum(baseM);

    // step 5: scored cards, left to right
    for (Card* c : scoring.scored) {
        // 5a: retrigger count, once per card
        scoring.retriggers = 0;
        scoring.currentCard = c;
        EventData rq{}; rq.card = c; rq.handType = scoring.handType;
        broadcast(RETRIG_QUERY_SCORED, rq);
        if (c->seal == SEAL_RED) ++scoring.retriggers;
        int N = 1 + scoring.retriggers;

        // 5b: trigger loop
        for (int t = 0; t < N; ++t) {
            if (isDebuffed(*c)) continue;   // debuffed: no scoring effects
            // i. base chips (stone flat 50)
            if (c->enhancement == ENH_STONE)
                scoring.chips = scoring.chips + BigNum(50);
            else
                scoring.chips = scoring.chips
                              + BigNum((double)(c->rankChips() + c->bonusChips));
            // ii. enhancement
            switch (c->enhancement) {
                case ENH_BONUS: scoring.chips = scoring.chips + BigNum(30); break;
                case ENH_MULT:  scoring.mult  = scoring.mult  + BigNum(4);  break;
                case ENH_GLASS: scoring.mult  = scoring.mult  * BigNum(2);  break;
                case ENH_LUCKY: {
                    if (chance(keys::luckyMult(), 1, 5)) {
                        scoring.mult = scoring.mult + BigNum(20);
                        EventData le{}; le.card = c;
                        broadcast(LUCKY_TRIGGERED, le);
                    }
                    if (chance(keys::luckyMoney(), 1, 15)) {
                        money += 20;      // instant commit (Bull-correct)
                        EventData le{}; le.card = c;
                        broadcast(LUCKY_TRIGGERED, le);
                    }
                } break;
                default: break;
            }
            // iii. edition
            if (c->edition == ED_FOIL) scoring.chips = scoring.chips + BigNum(50);
            if (c->edition == ED_HOLO) scoring.mult  = scoring.mult  + BigNum(10);
            if (c->edition == ED_POLY) scoring.mult  = scoring.mult  * BigNum(1.5);
            // iv. seal: gold seal pays NOW (played & scored)
            if (c->seal == SEAL_GOLD) money += 3;
            // v. per-card joker effects
            EventData ce{}; ce.card = c; ce.handType = scoring.handType;
            broadcast(CARD_SCORED, ce);
        }
    }
    scoring.currentCard = nullptr;

    // step 6: held cards
    for (Card* c : scoring.held) {
        scoring.retriggers = 0;
        EventData rq{}; rq.card = c;
        broadcast(RETRIG_QUERY_HELD, rq);
        if (c->seal == SEAL_RED) ++scoring.retriggers;
        int N = 1 + scoring.retriggers;
        for (int t = 0; t < N; ++t) {
            if (isDebuffed(*c)) continue;
            if (c->enhancement == ENH_STEEL)
                scoring.mult = scoring.mult * BigNum(1.5);
            EventData he{}; he.card = c;
            broadcast(CARD_HELD, he);
        }
    }

    // step 7: per joker, left to right: foil/holo -> ability -> poly
    // (index loop over a size snapshot: interact must never invalidate
    //  the iteration if a future joker adds jokers here -- see broadcast)
    const size_t nj_ = jokers.size();
    for (size_t ji_ = 0; ji_ < nj_; ++ji_) {
        auto& j = jokers[ji_];
        if (j->pendingDestroy) continue;
        if (j->edition == ED_FOIL) scoring.chips = scoring.chips + BigNum(50);
        if (j->edition == ED_HOLO) scoring.mult  = scoring.mult  + BigNum(10);
        EventData ie{}; ie.handType = scoring.handType;
        j->interact(*this, INDEPENDENT, ie, false);
        if (j->edition == ED_POLY) scoring.mult = scoring.mult * BigNum(1.5);
    }

    // step 8: Observatory — N/A until vouchers exist

    // steps 9-10: PLASMA BALANCE. Chips and Mult are averaged and the
    // result is ROUNDED DOWN — user-confirmed: 77 x 76 becomes 76 x 76,
    // and 144 x 1 becomes 72 x 72 (not 72.5 x 72.5).
    BigNum bal = (scoring.chips + scoring.mult).half().floorVal();
    BigNum handScore = bal * bal;
    score = score + handScore;

    // step 11
    broadcast(HAND_SCORED, EventData{});

    // step 12: destruction window
    for (Card* c : scoring.scored)
        if (c->enhancement == ENH_GLASS && !isDebuffed(*c))
            if (chance(keys::kGlass(), 1, 4)) destroyCardLater(*c);
    destructionWindow();

    handTypePlayedThisRound[scoring.handType] = true;
    if (mouthLockedType < 0) mouthLockedType = scoring.handType;
    if (bossActive()) {
        // passive bosses signal here; active ones call bossTriggered() inline
        if (boss::triggeredThisHand(*this, bossId)) bossTriggered();
        boss::onHandPlayed(*this, bossId);
    }
    if (score.naneinf() && !naneinfReached) {
        naneinfReached = true;
        naneinfAnte = ante;
    }
    if (score >= bestBlindScore) bestBlindScore = score;
    --handsLeft;
    checkDeath();
    return handScore;
}

void Balatro::destructionWindow() {
    if (destroyQueue_.empty()) { sweepJokers(); return; }
    std::vector<Card*> destroyed;
    for (Card* c : destroyQueue_) {
        if (c->destroyed) continue;
        c->destroyed = true;
        destroyed.push_back(c);
        if (c->enhancement == ENH_GLASS) ++glassDestroyedTotal;
        if (isFace(*c)) ++facesDestroyedTotal;   // ask-time: Pareidolia
    }                                            // scales Canio here
    destroyQueue_.clear();
    if (!destroyed.empty()) {
        EventData ev{}; ev.cards = &destroyed;
        broadcast(CARDS_DESTROYED, ev);
    }
    sweepJokers();
}

// ---------------- display names ----------------
// The enums are 1-based over the game-ordered pools, so the name is just a
// lookup into pools.hpp.
const char* voucherName(int id) {
    if (id <= 0 || id > pools::VOUCHERS_N) return "none";
    return pools::VOUCHERS[id - 1];
}
const char* tagName(int id) {
    if (id <= 0 || id > pools::TAGS_N) return "none";
    return pools::TAGS[id - 1];
}

// ---------------- consumable registry ----------------
std::vector<ConsumableInfo>& consumableRegistry() {
    static std::vector<ConsumableInfo> r;
    return r;
}
ConsumableRegistrar::ConsumableRegistrar(ConsumableInfo i) {
    consumableRegistry().push_back(std::move(i));
}

// ---------------- interaction layer ----------------
bool Balatro::isLegal(int a) const {
    if (a < 0 || a >= NUM_ACTIONS) return false;
    const bool playing  = (phase == PHASE_PLAYING);
    const bool cashout  = (phase == PHASE_CASH_OUT);
    // Joker/consumable manipulation is legal in EVERY phase — real Balatro
    // lets you rearrange and sell at blind select, mid-round, on the cash-out
    // screen and in the shop. Card selection needs a hand, so it is
    // PLAYING-only.
    const bool cursorOk = true;

    if (a >= A_TOGGLE_CARD_0 && a < A_TOGGLE_JOKER_0) {
        int i = a - A_TOGGLE_CARD_0;
        // selectable while playing AND while a pack is open (targeting)
        if (!(playing || phase == PHASE_PACK)) return false;
        if (i >= (int)hand.size()) return false;
        // Cerulean Bell: the forced card cannot be deselected
        if (bossActive() && bossId == BOSS_CERULEAN_BELL
            && hand[i]->id == ceruleanForcedCard && sel.hasCard(i)) return false;
        return sel.hasCard(i) || sel.cards.size() < 5;   // max 5 selected
    }
    if (a >= A_TOGGLE_JOKER_0 && a < A_TOGGLE_CONSUMABLE_0)
        return cursorOk && (a - A_TOGGLE_JOKER_0) < (int)jokers.size();
    if (a >= A_TOGGLE_CONSUMABLE_0 && a < A_SHIFT_CARD_LEFT)
        return cursorOk && (a - A_TOGGLE_CONSUMABLE_0) < (int)consumables.size();

    switch (a) {
    // shifts require EXACTLY one selected card/joker: unambiguous, and any
    // permutation is still reachable by repeated single shifts.
    // Legal while PLAYING and while a pack is open — Arcana/Spectral packs
    // put the hand on screen for targeting, and the real game lets you
    // rearrange it there. The single-selection guard makes this self-
    // limiting for packs that show no hand: nothing is selectable, so
    // nothing shifts.
    case A_SHIFT_CARD_LEFT:
        return (playing || phase == PHASE_PACK)
               && sel.cards.size() == 1 && sel.cards[0] > 0;
    case A_SHIFT_CARD_RIGHT:
        return (playing || phase == PHASE_PACK)
               && sel.cards.size() == 1
               && sel.cards[0] + 1 < (int)hand.size();
    case A_SHIFT_JOKER_LEFT:   return cursorOk && sel.joker > 0;
    case A_SHIFT_JOKER_RIGHT:  return cursorOk && sel.joker >= 0
                                      && sel.joker + 1 < (int)jokers.size();
    case A_PLAY:               return playing && !sel.cards.empty()
                                      && handsLeft > 0
                                      && (!bossActive()
                                          || boss::handAllowed(*this, bossId));
    case A_DISCARD:            return playing && !sel.cards.empty()
                                      && discardsLeft > 0;
    case A_USE_CONSUMABLE:
        return cursorOk && sel.consumable >= 0
               && sel.consumable < (int)consumables.size()
               && consumables[sel.consumable]->canUse(*this);
    case A_SELL_CONSUMABLE:    return cursorOk && sel.consumable >= 0;
    case A_SELL_JOKER:         return cursorOk && sel.joker >= 0
                                      && !jokers[sel.joker]->eternal;
    case A_CONFIRM_CASHOUT:    return cashout;
    case A_CLEAR_SELECTION:    return cursorOk;
    case A_SELECT_BLIND:       return phase == PHASE_BLIND_SELECT;
    case A_SKIP_BLIND:         return phase == PHASE_BLIND_SELECT
                                      && blindSkippable();
    case A_BUY_VOUCHER:        return phase == PHASE_SHOP
                                      && shopVoucher != V_NONE
                                      && canAfford(10);
    case A_REROLL_SHOP:        return phase == PHASE_SHOP
                                      && canAfford(rerollCost());
    case A_LEAVE_SHOP:         return phase == PHASE_SHOP
                                      && packContents.empty();
    case A_REROLL_BOSS:
        return phase == PHASE_BLIND_SELECT && blindIdx == 2
               && canAfford(10)
               && (ownedVouchers[V_RETCON]
                   || (ownedVouchers[V_DIRECTORS_CUT] && bossRerollsThisAnte == 0));
    case A_HOLD_JOKER:
        return phase == PHASE_BLIND_SELECT && blindIdx == 2
               && bossId == BOSS_AMBER_ACORN && sel.joker >= 0;
    case A_SKIP_PACK:          return phase == PHASE_PACK;
    default: break;
    }
    if (a >= A_BUY_PACK_0 && a < A_PICK_PACK_0) {
        int i = a - A_BUY_PACK_0;
        return phase == PHASE_SHOP && i < (int)packOffers.size()
               && !packOffers[i].taken && canAfford(packOffers[i].cost);
    }
    if (a >= A_PICK_PACK_0 && a < A_SKIP_PACK) {
        int i = a - A_PICK_PACK_0;
        if (phase != PHASE_PACK || i >= (int)packContents.size()) return false;
        const PackCard& pc = packContents[i];
        if (pc.taken) return false;
        // Arcana/Celestial/Spectral picks are USED IMMEDIATELY, so the pick
        // is legal only when the card can actually resolve.
        if (pc.kind == 0) return consumableUsableNow(pc.registryIdx);
        if (pc.kind == 1) return pc.edition == ED_NEGATIVE
                              || (int)jokers.size() < mods.jokerSlots;
        return true;                       // playing cards always fit
    }
    if (a >= A_BUY_SLOT_0 && a < A_BUY_VOUCHER) {
        int i = a - A_BUY_SLOT_0;
        if (phase != PHASE_SHOP || i >= (int)shop.size()) return false;
        if (shop[i].sold || !canAfford(shop[i].cost)) return false;
        // slot limits: jokers need a joker slot (negative edition exempt),
        // consumables need a consumable slot
        if (shop[i].kind == 0)
            return shop[i].edition == ED_NEGATIVE
                || (int)jokers.size() < mods.jokerSlots;
        if ((int)consumables.size() < mods.consumableSlots) return true;
        // Slots full: legal only as a BUY-AND-USE, i.e. if it could be used
        // right now (targeting tarots need a selection; creators need room).
        return consumableUsableNow(shop[i].registryIdx);
    }
    return false;
}

void Balatro::legalActions(bool out[NUM_ACTIONS]) const {
    for (int a = 0; a < NUM_ACTIONS; ++a) out[a] = isLegal(a);
}

bool Balatro::applyAction(int a) {
    if (!isLegal(a)) return false;

    if (a >= A_TOGGLE_CARD_0 && a < A_TOGGLE_JOKER_0) {
        int i = a - A_TOGGLE_CARD_0;
        for (size_t k = 0; k < sel.cards.size(); ++k)
            if (sel.cards[k] == i) { sel.cards.erase(sel.cards.begin() + k);
                                     return true; }
        sel.cards.push_back(i);          // selection ORDER is preserved
        return true;
    }
    if (a >= A_TOGGLE_JOKER_0 && a < A_TOGGLE_CONSUMABLE_0) {
        int i = a - A_TOGGLE_JOKER_0;
        sel.joker = (sel.joker == i) ? -1 : i;
        return true;
    }
    if (a >= A_TOGGLE_CONSUMABLE_0 && a < A_SHIFT_CARD_LEFT) {
        int i = a - A_TOGGLE_CONSUMABLE_0;
        sel.consumable = (sel.consumable == i) ? -1 : i;
        return true;
    }
    switch (a) {
    case A_SHIFT_CARD_LEFT: {
        int i = sel.cards[0];
        std::swap(hand[i], hand[i - 1]);
        sel.cards[0] = i - 1;
        return true; }
    case A_SHIFT_CARD_RIGHT: {
        int i = sel.cards[0];
        std::swap(hand[i], hand[i + 1]);
        sel.cards[0] = i + 1;
        return true; }
    case A_SHIFT_JOKER_LEFT: {
        std::swap(jokers[sel.joker], jokers[sel.joker - 1]);
        --sel.joker;
        broadcast(JOKERS_CHANGED, EventData{});
        return true; }
    case A_SHIFT_JOKER_RIGHT: {
        std::swap(jokers[sel.joker], jokers[sel.joker + 1]);
        ++sel.joker;
        broadcast(JOKERS_CHANGED, EventData{});
        return true; }
    case A_PLAY:      playSelected();    return true;
    case A_DISCARD:   discardSelected(); return true;
    case A_USE_CONSUMABLE: {
        // TAKE OWNERSHIP FIRST, then erase, THEN resolve. Creators (The
        // Emperor, High Priestess, The Fool) push_back during use(), which
        // reallocates `consumables` — holding a reference into the vector
        // across use() is a use-after-free. Consuming the card before its
        // effect also matches the game: it frees its own slot.
        std::unique_ptr<Consumable> con = std::move(consumables[sel.consumable]);
        consumables.erase(consumables.begin() + sel.consumable);
        sel.consumable = -1;

        if (con->kind == 0) ++tarotsUsed;
        // The Fool copies the last Tarot/Planet used — and never itself.
        int idx = -1;
        for (size_t i = 0; i < consumableRegistry().size(); ++i)
            if (consumableRegistry()[i].name == con->name) { idx = (int)i; break; }
        if (con->name != "The Fool" && (con->kind == 0 || con->kind == 1))
            lastTarotIdx = idx;

        con->use(*this);
        EventData ev{}; ev.consumableId = con->kind;
        broadcast(CONSUMABLE_USED, ev);
        return true; }
    case A_SELL_CONSUMABLE:
        addMoney(consumables[sel.consumable]->sellValue());
        consumables.erase(consumables.begin() + sel.consumable);
        sel.consumable = -1;
        return true;
    case A_SELL_JOKER: {
        Joker* j = jokers[sel.joker].get();
        addMoney(j->sellValue());
        ++cardsSoldThisRun;
        EventData self{}; self.joker = j;
        j->interact(*this, SELF_SOLD, self, false);   // before removal
        EventData ev{}; ev.joker = j;
        for (auto& other : jokers)
            if (other.get() != j) other->interact(*this, CARD_SOLD, ev, false);
        jokers.erase(jokers.begin() + sel.joker);
        sel.joker = -1;
        verdantActive = false;          // Verdant Leaf: "until 1 Joker sold"
        recomputeMods();
        broadcast(JOKERS_CHANGED, EventData{});
        return true; }
    case A_CONFIRM_CASHOUT: confirmCashOut(); return true;
    case A_CLEAR_SELECTION: sel = Selection{}; return true;
    case A_SELECT_BLIND:    selectBlind();     return true;
    case A_SKIP_BLIND:      skipBlind();       return true;
    case A_BUY_VOUCHER:     buyVoucher();      return true;
    case A_REROLL_SHOP:     rerollShop();      return true;
    case A_LEAVE_SHOP:      leaveShop();       return true;
    case A_REROLL_BOSS:     rerollBoss();      return true;
    case A_HOLD_JOKER:      heldJokerIdx = sel.joker; return true;
    case A_SKIP_PACK:       skipPack();        return true;
    default: break;
    }
    if (a >= A_BUY_SLOT_0 && a < A_BUY_VOUCHER) {
        buyShopItem(a - A_BUY_SLOT_0);
        return true;
    }
    if (a >= A_BUY_PACK_0 && a < A_PICK_PACK_0) {
        buyPack(a - A_BUY_PACK_0);
        return true;
    }
    if (a >= A_PICK_PACK_0 && a < A_SKIP_PACK) {
        pickPackCard(a - A_PICK_PACK_0);
        return true;
    }
    return false;
}

BigNum Balatro::playSelected() {
    std::vector<Card*> played, remaining;
    for (int i : sel.cards) played.push_back(hand[i]);        // selection order
    for (size_t i = 0; i < hand.size(); ++i)
        if (!sel.hasCard((int)i)) remaining.push_back(hand[i]);
    // press_play: The Hook eats 2 of the REMAINING cards before scoring,
    // and its victims hit the discard pile ahead of the played cards.
    if (bossActive() && bossId == BOSS_HOOK) boss::hookPressPlay(*this, remaining);
    BigNum s = playHand(played, remaining);
    for (Card* c : played) { c->playedThisAnte = true; discardPile.push_back(c); }
    hand = remaining;
    sel = Selection{};
    if (blindGoal.toDouble() > 0 && score >= blindGoal) {
        // Blind beaten: the real game goes straight to end_round — there is
        // NO redraw after the winning hand. (The phantom redraw here was
        // invisible everywhere except the shop pack deal, which reads deck
        // order downstream of it.)
    } else if (bossActive() && bossId == BOSS_SERPENT) {
        drawCards(3);                                  // ignores hand size
    } else {
        drawToHandSize();
    }
    return s;
}

bool Balatro::mrBonesSaves() {
    for (auto& j : jokers) {
        if (j->name != "Mr. Bones" || j->pendingDestroy) continue;
        BigNum quarter = blindGoal.half().half();     // 25% of the goal
        if (score >= quarter) { queueDestroyJoker(*j); sweepJokers(); return true; }
    }
    return false;
}

void Balatro::checkDeath() {
    if (phase != PHASE_PLAYING) return;
    if (score >= blindGoal) return;             // blind beaten
    if (handsLeft > 0) return;                  // still have hands
    if (mrBonesSaves()) {
        // Saved: the ROUND IS OVER, survived. Without this, the game sat
        // in PHASE_PLAYING with 0 hands forever (found via an agent
        // softlock on seed TDI6E2FP). Setting score to the goal lets the
        // existing win transitions fire in their proper places --
        // playSelected's no-redraw branch and the harness auto-cash-out
        // -- with correct end-of-round card collection ordering.
        score = blindGoal;
        return;
    }
    phase = PHASE_GAME_OVER;
}

void Balatro::discardSelected() {
    std::vector<Card*> gone;
    for (int i : sel.cards) gone.push_back(hand[i]);
    std::vector<Card*> remaining;
    for (size_t i = 0; i < hand.size(); ++i)
        if (!sel.hasCard((int)i)) remaining.push_back(hand[i]);
    --discardsLeft;
    ++discardsUsedThisRound;

    EventData ev{}; ev.cards = &gone;
    broadcast(DISCARD, ev);                       // per ACTION
    for (Card* c : gone) {                        // per CARD
        ++cardsDiscardedThisRound;
        if (c->seal == SEAL_PURPLE) { /* TODO: create a tarot */ }
        EventData ce{}; ce.card = c;
        broadcast(CARD_DISCARDED, ce);
        discardPile.push_back(c);
    }
    hand = remaining;
    sel = Selection{};
    if (bossActive() && bossId == BOSS_SERPENT) drawCards(3);  // ignores hand size
    else drawToHandSize();
    if (bossActive()) boss::onDiscardAction(*this, bossId);
    destructionWindow();
}

// Build a pool in the GAME'S order, skipping anything not implemented.
// Index order matters: a roll picks an index, so registry order would give
// the wrong item for the right number.
static std::vector<int> orderedJokerPool(int rarity) {
    const char* const* names; int n;
    switch (rarity) {
        case 2: names = pools::UNCOMMON_JOKERS;  n = pools::UNCOMMON_JOKERS_N;  break;
        case 3: names = pools::RARE_JOKERS;      n = pools::RARE_JOKERS_N;      break;
        case 4: names = pools::LEGENDARY_JOKERS; n = pools::LEGENDARY_JOKERS_N; break;
        default: names = pools::COMMON_JOKERS;   n = pools::COMMON_JOKERS_N;    break;
    }
    std::vector<int> out;
    for (int i = 0; i < n; ++i)
        for (size_t j = 0; j < jokerRegistry().size(); ++j)
            if (jokerRegistry()[j].name == names[i]) { out.push_back((int)j); break; }
    return out;
}

static std::vector<int> orderedConsumablePool(int kind) {
    const char* const* names; int n;
    if (kind == 0)      { names = pools::TAROTS;    n = pools::TAROTS_N; }
    else if (kind == 1) { names = pools::PLANETS;   n = pools::PLANETS_N; }
    else                { names = pools::SPECTRALS; n = pools::SPECTRALS_N; }
    std::vector<int> out;
    for (int i = 0; i < n; ++i)
        for (size_t j = 0; j < consumableRegistry().size(); ++j)
            if (consumableRegistry()[j].name == names[i]) { out.push_back((int)j); break; }
    return out;
}

// ---------------- boss blinds ----------------
void Balatro::bossTriggered() {
    ++bossTriggeredThisHand;
    broadcast(BOSS_TRIGGERED, EventData{});     // Matador
}

int Balatro::mostPlayedHand() const {
    int best = 0;
    for (int t = 1; t < NUM_HAND_TYPES; ++t)
        if (handPlayCount[t] > handPlayCount[best]) best = t;
    return best;
}

void Balatro::rotateCrimsonHeart() {
    for (auto& j : jokers) j->disabled = false;
    if (jokers.empty()) { crimsonDisabledJoker = -1; return; }
    crimsonDisabledJoker =
        rng->randint(keys::kCrimsonHeart() + std::to_string(handsPlayedTotal),
                     0, (int)jokers.size() - 1);
    jokers[crimsonDisabledJoker]->disabled = true;
}

void Balatro::rollCeruleanForce() {
    ceruleanForcedCard = -1;
    if (hand.empty()) return;
    int i = rng->randint(keys::kCeruleanBell() + std::to_string(handsPlayedTotal),
                         0, (int)hand.size() - 1);
    ceruleanForcedCard = hand[i]->id;
    if (!sel.hasCard(i)) {
        if (sel.cards.size() >= 5) sel.cards.pop_back();
        sel.cards.insert(sel.cards.begin(), i);
    }
}

int Balatro::selectionHandType() const {
    if (sel.cards.empty()) return -1;
    std::vector<Card*> cs;
    for (int i : sel.cards) cs.push_back(hand[i]);
    return evaluateHand(cs, mods).scoredType;
}

bool Balatro::isDebuffed(const Card& c) const {
    if (c.debuffed) return true;                       // persistent facts
    if (!bossActive()) return false;
    return boss::debuffsCard(*this, bossId, c);        // interpretation
}

void Balatro::rollBoss() {
    // "Only the Boss Blinds with the fewest appearances will be selected"
    // and showdown blinds only on multiples of 8.
    bool wantShowdown = (ante % 8 == 0);
    // walk the GAME'S boss order so the picked index resolves correctly
    static std::vector<int> gameOrder;
    if (gameOrder.empty())
        for (int k = 0; k < pools::BOSSES_N; ++k)
            for (int i = 0; i < NUM_BOSSES; ++i)
                if (std::string(BOSS_DEFS[i].name) == pools::BOSSES[k])
                    { gameOrder.push_back(i); break; }
    std::vector<int> pool;
    int fewest = 1 << 30;
    for (int i : gameOrder) {
        if (BOSS_DEFS[i].showdown != wantShowdown) continue;
        if (ante < BOSS_DEFS[i].minAnte) continue;
        fewest = std::min(fewest, bossAppearances[i]);
    }
    for (int i : gameOrder) {
        if (BOSS_DEFS[i].showdown != wantShowdown) continue;
        if (ante < BOSS_DEFS[i].minAnte) continue;
        if (bossAppearances[i] == fewest) pool.push_back(i);
    }
    if (pool.empty()) { bossId = wantShowdown ? BOSS_AMBER_ACORN : BOSS_HOOK; return; }
    // dedicated stream, isolated from play decisions
    // The real game uses a single "boss" stream with no ante component.
    bossId = pool[rng->randint(keys::boss(), 0, (int)pool.size() - 1)];
    ++bossAppearances[bossId];
}

void Balatro::rerollBoss() {
    spendMoney(10);
    ++bossRerollsThisAnte;
    if (bossId >= 0) --bossAppearances[bossId];
    rollBoss();
    blindGoal = blindRequirement(ante, blindIdx);
}

// ---------------- ante / shop loop ----------------
// RNG KEY DISCIPLINE (critical for the AI experiment):
// shop contents, skip tags and vouchers each use their OWN keys, embedding
// the ante and slot. They are therefore completely isolated from play
// decisions — two agents on the same seed see identical shops regardless of
// which cards they chose to play or how many discards they burned. Mixing
// these into a shared stream would make agents effectively play different
// seeds from ante 2 onward.  ⚠ phase 2: match the real key strings.
// Per-ante roll order, straight from Immolate's analyzer:
//   next_boss(ante) -> next_voucher(ante) -> next_tag(ante) x2 -> shop queue
// All of it happens when the ante BEGINS, not when each blind is reached.
// Ante-gated locks (Immolate init_locks). Locked items are resampled past.
// The same table gates boss blinds, which BOSS_DEFS already encodes as
// minAnte — these values match it exactly.
static bool tagLockedAtAnte(int tag, int ante) {
    if (ante >= 2) return false;
    switch (tag) {
        case TAG_NEGATIVE: case TAG_STANDARD: case TAG_METEOR:
        case TAG_BUFFOON:  case TAG_HANDY:    case TAG_GARBAGE:
        case TAG_ETHEREAL: case TAG_TOP_UP:   case TAG_ORBITAL:
            return true;
        default: return false;
    }
}

int Balatro::rollTag() {
    // randchoice_common: base key, then _resample2/3/... past locked items
    int t = 1 + rng->randint(keys::tag(ante), 0, pools::TAGS_N - 1);
    int resampleNum = 1;
    while (tagLockedAtAnte(t, ante) && resampleNum < 20) {
        t = 1 + rng->randint(keys::resample(keys::tag(ante), resampleNum),
                             0, pools::TAGS_N - 1);
        ++resampleNum;
    }
    return t;
}

void Balatro::beginAnte() {
    bossId = -1;
    rollBoss();                                    // 1. boss
    anteVoucher = rollVoucher(keys::voucher(ante));            // 2. voucher
    // 3. two tags: TWO CONSECUTIVE DRAWS from the same key, not resamples
    anteTagA = rollTag();
    anteTagB = rollTag();
    rng->random(keys::orbitalTag(ante));           // 4. burned call (boss blind)
    shopsThisAnte = 0;
    voucherBought = false;
    bossRerollsThisAnte = 0;
    heldJokerIdx = -1;
}

void Balatro::enterBlindSelect() {
    phase = PHASE_BLIND_SELECT;
    if (!anteStarted) { anteStarted = true; beginAnte(); }
    blindGoal = blindRequirement(ante, blindIdx);
    sel = Selection{};
    // the ante's two tags were rolled up front: small gets A, big gets B
    skipTag = (blindIdx == 0) ? anteTagA
            : (blindIdx == 1) ? anteTagB : TAG_NONE;
}

void Balatro::selectBlind() {
    broadcast(BLIND_SELECTED, EventData{});
    startRound();                       // startRound sets PHASE_PLAYING
}

void Balatro::skipBlind() {
    ++blindsSkipped;
    if (skipTag == TAG_DOUBLE) pendingDoubleTags++;
    else if (skipTag == TAG_INVESTMENT) {
        investmentTagPending += 1 + pendingDoubleTags;
        pendingDoubleTags = 0;
    } else if (skipTag != TAG_NONE) {
        for (int n = 0; n < 1 + pendingDoubleTags; ++n)
            pendingTags.push_back(skipTag);
        pendingDoubleTags = 0;
    }
    broadcast(BLIND_SKIPPED, EventData{});
    ++blindIdx;                          // skipping goes straight to the next
    if (blindIdx > 2) blindIdx = 2;      // the boss can never be skipped
    enterBlindSelect();
}

void Balatro::openShop() {
    phase = PHASE_SHOP;
    shop.clear();
    rerollsThisShop = 0;
    // state_events.lua new_round(): reroll_cost_increase = 0 and
    // free_rerolls = (number of Chaos the Clown jokers held). freeRerolls is
    // RECHARGED from the current joker count, not merely reset to 0 — a
    // player holding two Chaos starts the shop with two free rerolls.
    rerollCostIncrease = 0;
    freeRerolls = countJokersNamed("Chaos the Clown");
    // One voucher per ante, rolled in beginAnte(). It PERSISTS across every
    // shop of that ante until purchased — it is not first-shop-only.
    // (beginAnte() clears voucherBought, so a new ante offers a new one.)
    shopVoucher = voucherBought ? V_NONE : anteVoucher;
    ++shopsThisAnte;
    int slots = 2 + (ownedVouchers[V_OVERSTOCK] ? 1 : 0);
    for (int i = 0; i < slots; ++i) rollShopSlot(i);
    // two booster pack offers, own stream
    packOffers.clear();
    for (int i = 0; i < 2; ++i) {
        PackOffer p;
        // THE FIRST PACK OF THE RUN IS A FORCED BUFFOON PACK (ante <= 2,
        // once per run — Immolate's `generatedFirstPack`). User-confirmed:
        // playing ante 1's small blind and entering the shop always offers
        // one. Critically it returns BEFORE the weighted roll, so it does
        // NOT consume a draw from shop_pack<ante>.
        if (ante <= 2 && !generatedFirstPack) {
            generatedFirstPack = true;
            p.type = PACK_BUFFOON;
            p.size = PACKSZ_NORMAL;
            p.cost = 4;
            packOffers.push_back(p);
            continue;
        }
        // Otherwise ONE weighted roll picks BOTH type and size
        // (randweightedchoice); consecutive packs are consecutive draws.
        double poll = rng->random(keys::shopPack(ante)) * keys::PACK_TOTAL_WEIGHT;
        double acc = 0;
        int pick = keys::PACK_TABLE_N - 1;
        for (int e = 0; e < keys::PACK_TABLE_N; ++e) {
            acc += keys::PACK_TABLE[e].weight;
            if (poll < acc) { pick = e; break; }
        }
        p.type = keys::PACK_TABLE[pick].type;
        p.size = keys::PACK_TABLE[pick].size;
        p.cost = (p.size == PACKSZ_NORMAL) ? 4 : (p.size == PACKSZ_JUMBO ? 6 : 8);
        packOffers.push_back(p);
    }
    broadcast(SHOP_ENTERED, EventData{});
    // pending tags fire on shop entry
    for (size_t i = 0; i < pendingTags.size();) {
        int tag = pendingTags[i];
        if (tag == TAG_UNCOMMON || tag == TAG_RARE || tag == TAG_COUPON
            || tag == TAG_VOUCHER || tag == TAG_ECONOMY || tag == TAG_SPEED
            || tag == TAG_HANDY || tag == TAG_GARBAGE) {
            applyTag(tag);
            pendingTags.erase(pendingTags.begin() + i);
        } else ++i;
    }
}

// ---------------- booster packs ----------------
// Sizes: normal 3 cards/pick 1, jumbo 5/pick 1, mega 5/pick 2.
// Buffoon packs are 2/pick 1 (jumbo 4/1, mega 4/2).   ⚠ verify exact counts.
static void packShape(int type, int size, int& count, int& picks) {
    for (int i = 0; i < keys::PACK_TABLE_N; ++i)
        if (keys::PACK_TABLE[i].type == type && keys::PACK_TABLE[i].size == size) {
            count = keys::PACK_TABLE[i].count;
            picks = keys::PACK_TABLE[i].picks;
            return;
        }
    count = 3; picks = 1;
}

Card Balatro::randomPlayingCard(const std::string& key) {
    Card c;
    // real keys; `key` still distinguishes the slot until call order is matched
    c.rank = rng->randint(keys::cardFront(keys::SRC_STANDARD, ante) + key, 2, 14);
    c.suit = rng->randint(keys::cardFront(keys::SRC_STANDARD, ante) + key + "s", 0, 3);
    if (rng->random(keys::cardHasEnhancement(ante) + key) > 0.6) {   // ⚠ rate
        static const int enh[] = {ENH_BONUS, ENH_MULT, ENH_WILD, ENH_GLASS,
                                  ENH_STEEL, ENH_STONE, ENH_GOLD, ENH_LUCKY};
        c.enhancement = enh[rng->randint(
            keys::enhancement(keys::SRC_STANDARD, ante) + key, 0, 7)];
    }
    double e = rng->random(keys::cardEdition(ante) + key);
    if (e > 0.988) c.edition = ED_POLY;
    else if (e > 0.96) c.edition = ED_HOLO;
    else if (e > 0.92) c.edition = ED_FOIL;
    double s = rng->random(keys::cardHasSeal(ante) + key);
    if (s > 0.8) {                                      // ⚠ verify rate
        static const int seals[] = {SEAL_RED, SEAL_BLUE, SEAL_GOLD, SEAL_PURPLE};
        c.seal = seals[rng->randint(keys::cardSealType(ante) + key, 0, 3)];
    }
    return c;
}

void Balatro::runDestructionWindow() { destructionWindow(); }

// `src` is the game's source tag (keys::SRC_PRIESTESS, SRC_EMPEROR, ...).
// Multiple cards are CONSECUTIVE draws from one key with duplicates
// resampled away, exactly like a pack (randlist).
void Balatro::createRandomConsumables(int kind, int n, const char* src) {
    std::vector<int> pool = orderedConsumablePool(kind);
    if (pool.empty()) return;
    std::string k = (kind == 0) ? keys::tarot(src, ante)
                  : (kind == 1) ? keys::planet(src, ante)
                                : keys::spectral(src, ante);
    std::vector<int> taken;
    for (int i = 0; i < n; ++i) {
        if ((int)consumables.size() >= mods.consumableSlots) break;
        int p = drawFromPool(k, pool, taken, /*consumables=*/true);
        if (p < 0) break;
        grantConsumable(p);
    }
}

void Balatro::createRandomJoker(const char* src) {
    if ((int)jokers.size() >= mods.jokerSlots) return;
    double r = rng->random(keys::jokerRarity(src, ante));
    int want = (r > keys::RARITY_RARE) ? 3 : (r > keys::RARITY_UNCOMMON ? 2 : 1);
    std::vector<int> pool = orderedJokerPool(want);
    if (pool.empty()) return;
    std::vector<int> taken;
    int p = drawFromPool(keys::jokerByRarity(want, src, ante), pool, taken);
    if (p >= 0) addJoker(jokerRegistry()[p].make());
}

void Balatro::copyConsumable(int idx, int edition) {
    if (idx < 0 || idx >= (int)consumables.size()) return;
    // find its registry entry by name and mint a fresh one
    const std::string& nm = consumables[idx]->name;
    for (size_t i = 0; i < consumableRegistry().size(); ++i)
        if (consumableRegistry()[i].name == nm) {
            // Negative consumables ignore the slot cap.
            if (edition != ED_NEGATIVE
                && (int)consumables.size() >= mods.consumableSlots) return;
            grantConsumable((int)i, edition);
            return;
        }
}

void Balatro::createJokerOfRarity(int rarity, const char* src) {
    if ((int)jokers.size() >= mods.jokerSlots) return;
    std::vector<int> pool = orderedJokerPool(rarity);
    if (pool.empty()) return;
    std::vector<int> taken;
    int p = drawFromPool(keys::jokerByRarity(rarity, src, ante), pool, taken);
    if (p >= 0) addJoker(jokerRegistry()[p].make());
}

// Familiar / Grim / Incantation: destroy 1 random card in hand, add N new
// ones of a given family with random enhancements.
void Balatro::spectralReplace(int add, int kind) {
    if (!hand.empty()) {
        int i = rng->randint(keys::kSpectralDestroy(), 0, (int)hand.size() - 1);
        destroyCardLater(*hand[i]);
        hand.erase(hand.begin() + i);
        runDestructionWindow();
    }
    static const int enh[] = {ENH_BONUS, ENH_MULT, ENH_WILD, ENH_GLASS,
                              ENH_STEEL, ENH_GOLD, ENH_LUCKY};
    for (int n = 0; n < add; ++n) {
        std::string k = "spectral_add" + std::to_string(n);
        Card c;
        if (kind == 0) c.rank = 14;                                // aces
        else if (kind == 1) c.rank = rng->randint(k + "r", 2, 10); // numbered
        else c.rank = rng->randint(k + "r", 11, 13);               // face
        c.suit = rng->randint(k + "s", 0, 3);
        c.enhancement = enh[rng->randint(k + "e", 0, 6)];
        Card& ref = addCard(c);
        hand.push_back(&ref);
    }
}

void Balatro::levelUpHand(int handType) {
    if (handType < 0 || handType >= NUM_HAND_TYPES) return;
    ++handLevels[handType];
    if (!planetUsed[handType]) { planetUsed[handType] = true; ++uniquePlanetsUsed; }
}

void Balatro::grantConsumable(int registryIdx, int edition) {
    auto c = consumableRegistry()[registryIdx].make();
    c->edition = edition;
    consumables.push_back(std::move(c));
}

void Balatro::buyPack(int which) {
    PackOffer& p = packOffers[which];
    spendMoney(p.cost);
    p.taken = true;
    openPackType = p.type;
    int count, picks;
    packShape(p.type, p.size, count, picks);
    packPicksLeft = picks;
    packContents.clear();
    std::string base = "pack" + std::to_string(ante) + "_"
                     + std::to_string(blindIdx) + "_" + std::to_string(which);
    std::vector<int> takenC, takenJ;
    for (int i = 0; i < count; ++i) {
        std::string k = base + "_" + std::to_string(i);
        PackCard pc;
        if (p.type == PACK_STANDARD) {
            pc.kind = 2;
            pc.card = randomPlayingCard(k);
        } else if (p.type == PACK_BUFFOON) {
            pc.kind = 1;
            double r = rng->random(keys::jokerRarity(keys::SRC_BUFFOON, ante));
            int want = (r > keys::RARITY_RARE) ? 3
                     : (r > keys::RARITY_UNCOMMON ? 2 : 1);
            std::vector<int> pool = orderedJokerPool(want);
            if (pool.empty()) pool = orderedJokerPool(1);
            pc.registryIdx = drawFromPool(
                keys::jokerByRarity(want, keys::SRC_BUFFOON, ante),
                pool, takenJ);
        } else {
            pc.kind = 0;
            int wantKind = (p.type == PACK_ARCANA) ? 0
                         : (p.type == PACK_CELESTIAL) ? 1 : 2;
            const char* src = (wantKind == 0) ? keys::SRC_ARCANA
                            : (wantKind == 1) ? keys::SRC_CELESTIAL
                                              : keys::SRC_SPECTRAL;
            std::string kk = (wantKind == 0) ? keys::tarot(src, ante)
                           : (wantKind == 1) ? keys::planet(src, ante)
                                             : keys::spectral(src, ante);
            std::vector<int> pool = orderedConsumablePool(wantKind);
            if (pool.empty()) pool = orderedConsumablePool(0);
            pc.registryIdx = drawFromPool(kk, pool, takenC, true);
        }
        packContents.push_back(pc);
    }
    // Arcana/Spectral packs (ONLY — Standard packs never show your hand)
    // deal hand-size cards off the top of the deck so card-targeting
    // consumables can be used. The deal consumes NO RNG: the deck sits in
    // end_round collection order (see computeCashOut 2c), so the cards
    // shown are the first hand-size cards that entered the discard pile
    // last round, in entry order, then rank-sorted for display.
    // Closing the pack returns them to the deck top (skipPack), and the
    // next round's shuffle ignores physical order entirely.
    if (hand.empty() && (p.type == PACK_ARCANA || p.type == PACK_SPECTRAL)) {
        drawToHandSize();
        dealtForPack = true;
    }
    phase = PHASE_PACK;
    broadcast(PACK_OPENED, EventData{});    // Hallucination
}

void Balatro::pickPackCard(int i) {
    PackCard& pc = packContents[i];
    pc.taken = true;
    if (pc.kind == 0) {
        // Arcana / Celestial / Spectral packs are "to be USED IMMEDIATELY" —
        // the card never enters your consumable slots. (Buffoon packs give
        // jokers and Standard packs give playing cards; those are kept.)
        auto con = consumableRegistry()[pc.registryIdx].make();
        con->edition = pc.edition;
        useConsumableNow(std::move(con));
    }
    else if (pc.kind == 1) {
        auto j = jokerRegistry()[pc.registryIdx].make();
        j->edition = pc.edition;
        addJoker(std::move(j));
    } else                 addCard(pc.card);
    if (--packPicksLeft <= 0) skipPack();
}

// Can this consumable resolve right now? Guards the two cases that would
// otherwise silently no-op: card-targeting tarots with nothing selected,
// and creators with no room for what they make.
bool Balatro::consumableUsableNow(int registryIdx) const {
    auto probe = consumableRegistry()[registryIdx].make();
    return probe && probe->canUse(*this);
}

// Resolve a consumable without it ever occupying a slot: used by pack picks
// and by "buy and use" when the consumable slots are full.
void Balatro::useConsumableNow(std::unique_ptr<Consumable> con) {
    if (!con) return;
    if (con->kind == 0) ++tarotsUsed;
    int idx = -1;
    for (size_t i = 0; i < consumableRegistry().size(); ++i)
        if (consumableRegistry()[i].name == con->name) { idx = (int)i; break; }
    if (con->name != "The Fool" && (con->kind == 0 || con->kind == 1))
        lastTarotIdx = idx;
    con->use(*this);                      // the card is already "gone"
    EventData ev{}; ev.consumableId = con->kind;
    broadcast(CONSUMABLE_USED, ev);
}

void Balatro::skipPack() {
    bool wasSkippedWhole = (packPicksLeft > 0);
    if (dealtForPack) {                 // return the cards dealt for the pack
        for (Card* c : hand) drawPile.push_back(c);
        hand.clear();
        sel = Selection{};
        dealtForPack = false;
    }
    packContents.clear();
    packPicksLeft = 0;
    openPackType = -1;
    phase = PHASE_SHOP;
    if (wasSkippedWhole) broadcast(PACK_SKIPPED, EventData{});   // Red Card
}

// ---------------- tags ----------------
void Balatro::applyTag(int tag) {
    switch (tag) {
    case TAG_UNCOMMON: case TAG_RARE: {
        int want = (tag == TAG_UNCOMMON) ? 2 : 3;
        std::vector<int> pool;
        for (size_t j = 0; j < jokerRegistry().size(); ++j)
            if (jokerRegistry()[j].rarity == want) pool.push_back((int)j);
        if (!pool.empty() && (int)shop.size() < MAX_SHOP) {
            ShopItem it;
            it.kind = 0;
            it.registryIdx = pool[rng->randint(keys::kTagJoker(want, ante),
                                               0, (int)pool.size() - 1)];
            it.cost = 0;                      // free
            shop.push_back(it);
        }
        break; }
    case TAG_COUPON: {
        for (auto& it : shop) it.cost = 0;
        for (auto& p : packOffers) p.cost = 0;
        break; }
    case TAG_ECONOMY: {
        int gain = money > 0 ? money : 0;
        if (gain > 40) gain = 40;
        addMoney(gain);
        break; }
    case TAG_SPEED:   { addMoney(5 * blindsSkipped); break; }
    case TAG_HANDY:   { addMoney(1 * handsPlayedTotal); break; }
    case TAG_GARBAGE: { addMoney(1 * unusedDiscardsThisRun); break; }
    case TAG_VOUCHER: shopVoucher =
                          rollVoucher("tagvoucher" + std::to_string(ante));
                      break;
    case TAG_DOUBLE:  /* duplicates the next tag — handled at queue time */ break;
    default: break;
    }
}

// randchoice_common + randlist: draw from `pool` using `key`. The BASE key
// is drawn once per item; if the result is unavailable (already taken in
// this list), retry on "<key>_resample2", "_resample3", ... Showman
// disables the exclusion.
// Secret consumables are locked until their condition is met, and locked
// items are RESAMPLED PAST (same mechanism as ante-gated tags):
//   Planet X / Ceres / Eris  - until Five of a Kind / Flush House / Flush
//                              Five has been played this run
//   The Soul / Black Hole    - never appear in normal pools (The Soul has
//                              its own 0.3% pre-roll)
bool Balatro::consumableLocked(int registryIdx) const {
    const std::string& n = consumableRegistry()[registryIdx].name;
    if (n == "Planet X")   return handPlayCount[FIVE_OAK] == 0;
    if (n == "Ceres")      return handPlayCount[FLUSH_HOUSE] == 0;
    if (n == "Eris")       return handPlayCount[FLUSH_FIVE] == 0;
    if (n == "The Soul" || n == "Black Hole") return true;
    return false;
}

int Balatro::drawFromPool(const std::string& key,
                          const std::vector<int>& pool,
                          std::vector<int>& taken,
                          bool isConsumablePool) {
    if (pool.empty()) return -1;
    int idx = pool[rng->randint(key, 0, (int)pool.size() - 1)];
    auto unavailable = [&](int i) {
        if (isConsumablePool && consumableLocked(i)) return true;   // locked
        // pool flags first: Showman does NOT unlock a flag-gated joker
        if (!isConsumablePool && !jokerInPool(i)) return true;
        if (mods.allowDuplicates) return false;                     // Showman
        return std::find(taken.begin(), taken.end(), i) != taken.end();
    };
    int resampleNum = 1;
    while (unavailable(idx) && resampleNum < 25) {
        idx = pool[rng->randint(keys::resample(key, resampleNum),
                                0, (int)pool.size() - 1)];
        ++resampleNum;
    }
    if (!mods.allowDuplicates) taken.push_back(idx);
    return idx;
}

// next_shop_item(): draw the NEXT item from this ante's shop queue.
//
// KEY INSIGHT (Immolate analyzer.cl): the game keeps ONE shop queue per
// ante and calls this repeatedly — across all three blinds' shops and every
// reroll. There is no per-slot key and no per-slot resample; each call
// simply advances the same per-key streams ("cdt<ante>", "rarity<ante>sho",
// "Joker1sho<ante>", ...). An earlier version of this engine faked a
// resample index per slot, which split one stream into many. Fixed.
//
// Resampling is used ONLY when the drawn item is unavailable (locked, or a
// duplicate without Showman): randchoice_common retries with "_resample2",
// "_resample3", ... on the same key.
ShopItem Balatro::nextShopItem() {
    ShopItem item;
    double total = keys::RATE_JOKER + keys::RATE_TAROT + keys::RATE_PLANET;
    double roll = rng->random(keys::cardType(ante)) * total;

    if (roll < keys::RATE_JOKER) {
        item.kind = 0;
        double r = rng->random(keys::jokerRarity(keys::SRC_SHOP, ante));
        int want = (r > keys::RARITY_RARE) ? 3
                 : (r > keys::RARITY_UNCOMMON) ? 2 : 1;
        std::vector<int> pool = orderedJokerPool(want);
        if (pool.empty()) pool = orderedJokerPool(1);
        std::string k = keys::jokerByRarity(want, keys::SRC_SHOP, ante);
        int idx = -1;
        for (int attempt = 0; attempt < 20; ++attempt) {
            idx = pool[rng->randint(keys::resample(k, attempt),
                                    0, (int)pool.size() - 1)];
            if (mods.allowDuplicates) break;          // Showman
            if (!jokerAvailable(idx)) continue;       // duplicate -> resample
            break;
        }
        item.registryIdx = idx;
        item.cost = jokerRegistry()[idx].cost;
        double e = rng->random(keys::jokerEdition(keys::SRC_SHOP, ante));
        if (e > keys::ED_NEG)       item.edition = ED_NEGATIVE;
        else if (e > keys::ED_POLY) item.edition = ED_POLY;
        else if (e > keys::ED_HOLO) item.edition = ED_HOLO;
        else if (e > keys::ED_FOIL) item.edition = ED_FOIL;
    } else {
        item.kind = 1;
        int wantKind = (roll < keys::RATE_JOKER + keys::RATE_TAROT) ? 0 : 1;
        std::vector<int> pool = orderedConsumablePool(wantKind);
        if (pool.empty()) pool = orderedConsumablePool(0);
        std::string k = wantKind == 0 ? keys::tarot(keys::SRC_SHOP, ante)
                                      : keys::planet(keys::SRC_SHOP, ante);
        std::vector<int> taken;
        for (size_t s = 0; s < shop.size(); ++s)
            if (!shop[s].sold && shop[s].kind == 1) taken.push_back(shop[s].registryIdx);
        int idx = drawFromPool(k, pool, taken, /*consumables=*/true);
        item.registryIdx = idx;
        item.cost = consumableRegistry()[idx].cost;
    }
    if (ownedVouchers[V_CLEARANCE_SALE]) item.cost = item.cost * 3 / 4;
    return item;
}

// A joker is unavailable if it is already owned or already sitting in this
// shop (the game excludes duplicates unless Showman is present), or if a
// pool flag gates it (common_events.lua get_current_pool).
bool Balatro::jokerAvailable(int registryIdx) const {
    if (!jokerInPool(registryIdx)) return false;    // flag gate beats Showman
    const std::string& nm = jokerRegistry()[registryIdx].name;
    for (auto& j : jokers) if (j->name == nm) return false;
    for (auto& s : shop)
        if (!s.sold && s.kind == 0 && s.registryIdx == registryIdx) return false;
    return true;
}

// yes_pool_flag / no_pool_flag (game.lua). These are NOT duplicate checks —
// Showman does not bypass them — and the gated entry stays in the pool as an
// UNAVAILABLE slot so indices (and therefore seeds) do not shift.
bool Balatro::jokerInPool(int registryIdx) const {
    const std::string& nm = jokerRegistry()[registryIdx].name;
    if (nm == "Gros Michel") return !grosMichelExtinct;   // no_pool_flag
    if (nm == "Cavendish")   return  grosMichelExtinct;   // yes_pool_flag
    return true;
}

bool Balatro::consumableAvailable(int registryIdx) const {
    const std::string& nm = consumableRegistry()[registryIdx].name;
    for (auto& c : consumables) if (c->name == nm) return false;
    for (auto& s : shop)
        if (!s.sold && s.kind == 1 && s.registryIdx == registryIdx) return false;
    return true;
}

void Balatro::rollShopSlot(int i) {
    if ((int)shop.size() <= i) shop.resize(i + 1);
    shop[i] = nextShopItem();
}

void Balatro::buyShopItem(int slot) {
    ShopItem& it = shop[slot];
    spendMoney(it.cost);
    if (it.kind == 0) {
        auto j = jokerRegistry()[it.registryIdx].make();
        j->edition = it.edition;
        EventData ev{}; ev.joker = j.get();
        addJoker(std::move(j));
        broadcast(CARD_BOUGHT, ev);
    } else {
        auto con = consumableRegistry()[it.registryIdx].make();
        if ((int)consumables.size() < mods.consumableSlots) {
            consumables.push_back(std::move(con));
        } else {
            // No slot free: the game lets you BUY AND USE immediately.
            useConsumableNow(std::move(con));
        }
        broadcast(CARD_BOUGHT, EventData{});
    }
    it.sold = true;
}

void Balatro::buyVoucher() {
    spendMoney(10);                       // ⚠ verify voucher cost
    ownedVouchers[shopVoucher] = true;
    applyVouchers();
    voucherBought = true;
    shopVoucher = V_NONE;
}

void Balatro::applyVouchers() {
    // vouchers write into PassiveMods alongside jokers (spec §6)
    recomputeMods();
    if (ownedVouchers[V_SEED_MONEY]) mods.interestCap = 10;
    if (ownedVouchers[V_GRABBER])    mods.hands += 1;
    if (ownedVouchers[V_WASTEFUL])   mods.discards += 1;
    if (ownedVouchers[V_PAINT_BRUSH]) mods.handSize += 1;
}

// A voucher slot is available to be drawn iff it is not already owned this
// run AND, when it is an upgrade voucher (the second of a pair), its base
// voucher IS owned. In the V_* enum bases sit at ODD ids and their upgrade
// at the next EVEN id, so an upgrade's base is simply id-1. (Profile-level
// "unlocked" gating is assumed satisfied, matching every seed analyzer.)
bool Balatro::voucherInPool(int id) const {
    if (id <= 0 || id >= NUM_VOUCHERS) return false;
    if (ownedVouchers[id]) return false;            // already taken -> UNAVAILABLE
    bool isUpgrade = (id % 2 == 0);                 // V_OVERSTOCK_PLUS=2, etc.
    if (isUpgrade && !ownedVouchers[id - 1]) return false;  // base not owned
    return true;
}

// Draw one voucher the way Balatro's get_next_voucher_key does: pick a slot
// across the FULL 32-entry pool, and if it landed on an UNAVAILABLE slot,
// resample with "<key>_resample<it>" until it lands on an available one.
// Returns a 1-based V_* id. This is the fix for the shop showing an upgrade
// voucher (e.g. Reroll Glut) when its base was never bought — the old code
// took whatever index it drew instead of resampling past locked slots.
int Balatro::rollVoucher(const std::string& key) {
    int idx = rng->randint(key, 0, pools::VOUCHERS_N - 1);
    int resampleNum = 1;
    while (!voucherInPool(idx + 1) && resampleNum < 40) {
        idx = rng->randint(keys::resample(key, resampleNum),
                           0, pools::VOUCHERS_N - 1);
        ++resampleNum;
    }
    return idx + 1;                                 // pool index -> V_* id
}

void Balatro::rerollShop() {
    // button_callbacks.lua G.FUNCS.reroll_shop: pay only if the cost is
    // nonzero, then spend one free charge (floored at 0), then recompute.
    // The recompute SKIPS the escalation when the reroll just spent was
    // free — so free rerolls never raise the price of the next paid one.
    spendMoney(rerollCost());
    const bool wasFree = freeRerolls > 0;
    if (freeRerolls > 0) --freeRerolls;
    if (!wasFree) ++rerollCostIncrease;
    ++rerollsThisShop;
    broadcast(SHOP_REROLL, EventData{});
    // A reroll draws the NEXT items from the same per-ante queue; it does
    // not restart or re-key anything. It refreshes EVERY card slot — a slot
    // emptied by an earlier purchase is restocked with a freshly drawn item,
    // exactly like the real game (a bought slot does not stay blank across a
    // reroll). nextShopItem() returns a fresh ShopItem with sold=false, so
    // the assignment also clears the old slot's sold flag.
    for (size_t i = 0; i < shop.size(); ++i)
        shop[i] = nextShopItem();
}

void Balatro::leaveShop() {
    broadcast(SHOP_EXITED, EventData{});
    // blindIdx / ante already advanced in confirmCashOut()
    enterBlindSelect();
}

// ---------------- cash out (two phases) ----------------
// ORDER within the computation is authoritative (Balatro wiki "Money"
// cash-out table):
//   1. blind reward  2. unused hands  3. joker effects
//   4. Investment Tag  5. interest
// but that is DISPLAY order. INTEREST IS COMPUTED ON THE WALLET ONLY
// (user-confirmed): in-round money counts (gold seals, Rough Gem,
// Business Card — all commit instantly), while none of the cash-out
// lines do, jokers like Golden Joker included.
//
// CRUCIALLY the whole list is FROZEN here and credited only on
// confirmCashOut(). Two user-confirmed consequences:
//   * The Hermit used on the cash-out screen doubles the WALLET only —
//     it cannot double Golden Joker / To the Moon payouts.
//   * Crossing an interest threshold during the screen does not update
//     the already-computed interest line.
const Balatro::CashOut& Balatro::computeCashOut(int blindIdx) {
    CashOut co;
    freezingPayout_ = true;
    pendingPayout_ = 0;

    // 1. blind reward
    static const int reward[4] = {3, 4, 5, 8};   // small/big/boss/showdown
    int bi = blindIdx < 0 ? 0 : (blindIdx > 3 ? 3 : blindIdx);
    co.blindReward = reward[bi];
    addMoney(co.blindReward);

    // 2. unused hands
    co.handsBonus = handsLeft * mods.moneyPerHand;
    addMoney(co.handsBonus);

    // 2b. END-OF-ROUND HELD PASS, retrigger-aware. This runs AFTER the
    // scoring pass returned, so the player may have rearranged jokers in
    // between (real Balatro snapshots the joker order per pass; the
    // rearrange only affects the NEXT pass — which is this one). That is
    // the late-game engine: copiers pointed at Mime double held gold
    // cards and blue seals here.
    for (Card* c2 : hand) {
        if (isDebuffed(*c2)) continue;
        scoring.retriggers = 0;
        EventData rq{}; rq.card = c2;
        broadcast(RETRIG_QUERY_HELD, rq);
        if (c2->seal == SEAL_RED) ++scoring.retriggers;
        int N = 1 + scoring.retriggers;
        for (int t = 0; t < N; ++t) {
            if (c2->enhancement == ENH_GOLD) co.goldCards += 3;
            if (c2->seal == SEAL_BLUE) { /* TODO: create a planet card */ }
        }
    }
    addMoney(co.goldCards);

    // 2c. END-OF-ROUND DECK COLLECTION (state_events.lua end_round, AFTER
    // the held-in-hand pass above and BEFORE the round-eval joker money):
    //   draw_from_hand_to_discard  — hand popped FRONT-first (display order)
    //                                onto the back of the discard pile;
    //   draw_from_discard_to_deck  — discard popped BACK-first onto the top
    //                                of the deck, i.e. the discard pile is
    //                                REVERSED onto the deck top.
    // Net: top of deck = FIRST card that entered the discard this round.
    // This order is what an Arcana/Spectral pack deals from in the shop.
    // (Next round's shuffle pre-sorts by sort_id, so this order affects
    // ONLY the shop; it can never leak into later rounds.)
    for (Card* c : hand) discardPile.push_back(c);
    hand.clear();
    while (!discardPile.empty()) {
        drawPile.push_back(discardPile.back());   // back of drawPile = top
        discardPile.pop_back();
    }

    // 3. joker effects — THIS is all "econ jokers on the cash out screen"
    // means: subscribe to ROUND_END and call g.addMoney(). Diverted into
    // the pending payout by the freeze flag.
    const int beforeJokers = pendingPayout_;
    broadcast(ROUND_END, EventData{});
    sweepJokers();                       // Popcorn / Gros Michel may die here
    co.jokerTotal = pendingPayout_ - beforeJokers;

    // 4. Investment Tag (boss blinds only)
    if (investmentTagPending && bi >= 2) {
        co.investmentTag = 25 * investmentTagPending;
        addMoney(co.investmentTag);
        investmentTagPending = 0;
    }

    // 5. interest — basis is the WALLET, not the pending payout.
    // (Golden Joker's $4 can never push you over a breakpoint.)
    if (!mods.noInterest) {
        int basis = money;
        if (basis > 0) {
            // To the Moon raises the RATE ($1 extra per $5), not the cap.
            int steps = (basis / mods.interestPer) * mods.interestRate;
            if (steps > mods.interestCap * mods.interestRate)
                steps = mods.interestCap * mods.interestRate;   // ⚠ does the
                                        // cap scale too? assumed yes
            co.interest = steps;
            addMoney(co.interest);
        }
    }

    freezingPayout_ = false;
    pending_ = co;
    phase = PHASE_CASH_OUT;
    return pending_;
}

void Balatro::confirmCashOut() {
    money += pending_.total();          // credited only now
    pending_ = CashOut{};
    pendingPayout_ = 0;
    handsLeft = mods.hands;
    discardsLeft = mods.discards;
    unusedDiscardsThisRun += discardsLeft;
    discardsUsedThisRound = 0;
    cardsDiscardedThisRound = 0;
    broadcast(ROUND_START, EventData{});
    // THE ANTE ADVANCES ON BOSS DEFEAT, before the shop opens. The shop you
    // see after a boss is the FIRST SHOP OF THE NEXT ANTE — it draws from
    // that ante's queue and shows that ante's voucher. Verified against
    // seed QJSKRTWR: ante 2's queue opens Venus, Wrathful Joker, which is
    // exactly what the post-boss shop shows in the reference run.
    if (blindIdx == 2) {
        broadcast(BOSS_DEFEATED, EventData{});
        ++ante;
        blindIdx = 0;
        for (Card& c : deck) c.playedThisAnte = false;
        beginAnte();
    } else {
        ++blindIdx;
    }
    // G.FUNCS.cash_out: the deck is SHUFFLED AGAIN on the button press,
    // key 'cashout'<ante> (post-increment ante after a boss), before the
    // shop loads. pseudoshuffle pre-sorts by sort_id, so we rebuild the
    // pile in creation order first — this makes the shop deck order a pure
    // function of the seed, independent of every in-round action. It is
    // what Arcana/Spectral packs deal from.
    {
        drawPile.clear();
        for (Card& c : deck) if (!c.destroyed) drawPile.push_back(&c);
        std::vector<int> idx(drawPile.size());
        for (size_t i = 0; i < idx.size(); ++i) idx[i] = (int)i;
        rng->shuffle(keys::cashoutShuffle(ante), idx);
        std::vector<Card*> shuffled;
        shuffled.reserve(idx.size());
        for (int i : idx) shuffled.push_back(drawPile[i]);
        drawPile.swap(shuffled);
        discardPile.clear();
        hand.clear();
    }
    openShop();
}

// ---------------- blind requirements (spec §8c) ----------------
BigNum Balatro::blindRequirement(int a, int bIdx) const {
    // Base chip table for antes 0..8 (wiki "Blinds and Antes").
    static const double tbl[9] = {100, 300, 800, 2000, 5000,
                                  11000, 20000, 35000, 50000};
    double baseAmt;
    if (a <= 8) {
        baseAmt = tbl[a < 0 ? 0 : a];
    } else {
        // Endless: req = ante8 * (1.6 + (0.75*(A-8))^(1+0.2*(A-8)))^(A-8),
        // then rounded to TWO SIGNIFICANT DIGITS.
        double k = a - 8;
        double inner = 1.6 + std::pow(0.75 * k, 1.0 + 0.2 * k);
        baseAmt = tbl[8] * std::pow(inner, k);
        if (std::isfinite(baseAmt) && baseAmt > 0) {
            // TRUNCATE to two significant digits (not round-to-nearest).
            // Verified against the wiki's published table: antes 9/10/11 are
            // 110k / 560k / 7.2e6, which only match with flooring.
            double mag = std::floor(std::log10(baseAmt));
            double f = std::pow(10.0, mag - 1);
            baseAmt = std::floor(baseAmt / f) * f;
        }
    }
    // blind multiplier: small 1x, big 1.5x, boss 2x — overridden by the
    // boss itself (The Wall 4x, The Needle 1x, Violet Vessel 6x).
    double mult = (bIdx == 0) ? 1.0 : (bIdx == 1) ? 1.5 : 2.0;
    if (bIdx == 2 && bossId >= 0) {
        if (bossIsDisabled()) {
            int n = mods.chicotCount;
            // Chicot divides the oversized blinds once per copy; other
            // bosses just fall back to the normal 2x.
            if (n > 0 && bossId == BOSS_WALL)
                mult = 4.0 / std::pow(2.0, n);
            else if (n > 0 && bossId == BOSS_VIOLET_VESSEL)
                mult = 6.0 / std::pow(3.0, n);
            else if (bossId == BOSS_NEEDLE) mult = 1.0;   // never increased
            else mult = 2.0;
        } else {
            mult = bossMultiplier(bossId);
        }
    }
    BigNum req;
    if (!std::isfinite(baseAmt)) {
        // Deep endless overflows a double long before it overflows BigNum;
        // rebuild it in BigNum instead of saturating.
        double k = a - 8;
        BigNum acc = BigNum(tbl[8]);
        double inner = 1.6 + std::pow(0.75 * k, 1.0 + 0.2 * k);
        for (int i = 0; i < (int)k; ++i) acc = acc * BigNum(inner);
        req = acc;
    } else {
        req = BigNum(baseAmt);
    }
    return req * BigNum(mult) * BigNum(2.0);   // Plasma x2
}

