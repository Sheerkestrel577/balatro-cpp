// clone.cpp — Balatro::cloneGame(): deep copy for search and rollouts.
//
// Strategy: construct a shell (cheap: 52 cards, no RNG draws), then
// overwrite EVERY member. Interior Card* are remapped via card id, which
// is stable by design (card.hpp). Jokers deep-copy via cloneFn (includes
// scaling state). Consumables rebuild via the registry make() — which sets
// all per-instance ctor state (e.g. Converter::maxCards) — then copy the
// mutable base fields. RNG clones via RNG::clone().
//
// Transient-only members (destroyQueue_, copyDepth_, freezingPayout_,
// pendingPayout_) are empty/zero between actions, which is the only time
// cloneGame may be called; they are reset, not copied. pending_ (the
// frozen cash-out) IS live state during PHASE_CASH_OUT and IS copied.
//
// If you add a member to Balatro and forget it here, python/test_clone.py
// diverges within a few hundred steps -- run it after any engine change.
#include "balatro.hpp"

#include <unordered_map>

std::unique_ptr<Balatro> Balatro::cloneGame() const {
    auto c = std::unique_ptr<Balatro>(new Balatro(rng->clone()));

    // ---- deck & pointer remap ----------------------------------------
    c->deck = deck;                          // deque<Card>: value copy
    std::unordered_map<int, Card*> byId;
    byId.reserve(c->deck.size() * 2);
    for (Card& cd : c->deck) byId[cd.id] = &cd;
    auto remap = [&](Card* p) -> Card* {
        if (!p) return nullptr;
        auto it = byId.find(p->id);
        return it == byId.end() ? nullptr : it->second;
    };
    auto remapVec = [&](const std::vector<Card*>& src) {
        std::vector<Card*> out; out.reserve(src.size());
        for (Card* p : src) out.push_back(remap(p));
        return out;
    };
    c->drawPile    = remapVec(drawPile);
    c->hand        = remapVec(hand);
    c->discardPile = remapVec(discardPile);
    c->nextCardId  = nextCardId;

    // ---- scoring ctx (stale between hands, but remap for safety) -----
    c->scoring.chips = scoring.chips;
    c->scoring.mult = scoring.mult;
    c->scoring.handType = scoring.handType;
    c->scoring.containedHands = scoring.containedHands;
    c->scoring.played = remapVec(scoring.played);
    c->scoring.scored = remapVec(scoring.scored);
    c->scoring.held = remapVec(scoring.held);
    c->scoring.retriggers = scoring.retriggers;
    c->scoring.currentCard = remap(scoring.currentCard);
    c->scoring.firstHandOfRound = scoring.firstHandOfRound;

    // ---- jokers & consumables ----------------------------------------
    c->jokers.clear();
    for (const auto& j : jokers) {
        auto cj = j->clone();                // REGISTER_JOKER cloneFn:
        c->jokers.push_back(std::move(cj));  // deep copy incl. scaling
    }
    c->consumables.clear();
    for (const auto& u : consumables) {
        std::unique_ptr<Consumable> v;
        for (const auto& info : consumableRegistry())
            if (info.name == u->name) { v = info.make(); break; }
        if (!v) continue;                    // unknown: skip (shouldn't happen)
        v->edition = u->edition;
        v->baseCost = u->baseCost;
        v->bonusSell = u->bonusSell;
        c->consumables.push_back(std::move(v));
    }

    // ---- scalars & aggregates, in header order -----------------------
    c->money = money; c->ante = ante;
    c->handsLeft = handsLeft; c->discardsLeft = discardsLeft;
    c->score = score; c->bestBlindScore = bestBlindScore;
    c->naneinfReached = naneinfReached; c->naneinfAnte = naneinfAnte;
    c->mods = mods;                          // recomputed below anyway
    for (int i = 0; i < NUM_HAND_TYPES; ++i) {
        c->handLevels[i] = handLevels[i];
        c->planetUsed[i] = planetUsed[i];
        c->handVisible[i] = handVisible[i];
        c->handPlayCount[i] = handPlayCount[i];
        c->handTypePlayedThisRound[i] = handTypePlayedThisRound[i];
    }
    c->bossDisabled = bossDisabled;
    c->discardsUsedThisRound = discardsUsedThisRound;
    c->investmentTagPending = investmentTagPending;
    c->tarotsUsed = tarotsUsed; c->blindsSkipped = blindsSkipped;
    c->handsPlayedTotal = handsPlayedTotal;
    c->uniquePlanetsUsed = uniquePlanetsUsed;
    c->lastTarotIdx = lastTarotIdx; c->lastPlanetIdx = lastPlanetIdx;
    c->cardsSoldThisRun = cardsSoldThisRun;
    c->cardsDiscardedThisRound = cardsDiscardedThisRound;
    c->phase = phase;
    c->glassDestroyedTotal = glassDestroyedTotal;
    c->facesDestroyedTotal = facesDestroyedTotal;
    c->grosMichelExtinct = grosMichelExtinct;
    c->seedString = seedString;

    c->shop = shop;
    c->shopVoucher = shopVoucher; c->voucherBought = voucherBought;
    c->rerollCostIncrease = rerollCostIncrease;
    c->freeRerolls = freeRerolls;
    c->rerollsThisShop = rerollsThisShop;
    for (int i = 0; i < NUM_VOUCHERS; ++i)
        c->ownedVouchers[i] = ownedVouchers[i];
    c->pendingTags = pendingTags;
    c->packOffers = packOffers;
    c->packContents = packContents;          // PackCard holds Card BY VALUE
    c->packPicksLeft = packPicksLeft;
    c->openPackType = openPackType;
    c->dealtForPack = dealtForPack;
    c->blindIdx = blindIdx;

    c->bossId = bossId;
    for (int i = 0; i < NUM_BOSSES; ++i)
        c->bossAppearances[i] = bossAppearances[i];
    c->bossRerollsThisAnte = bossRerollsThisAnte;
    c->anteVoucher = anteVoucher;
    c->anteTagA = anteTagA; c->anteTagB = anteTagB;
    c->shopsThisAnte = shopsThisAnte;
    c->anteStarted = anteStarted;
    c->generatedFirstPack = generatedFirstPack;
    c->flintActive = flintActive;
    c->verdantActive = verdantActive;
    c->drawFaceDownNextDraw = drawFaceDownNextDraw;
    c->handSortMode = handSortMode;
    c->crimsonDisabledJoker = crimsonDisabledJoker;
    c->ceruleanForcedCard = ceruleanForcedCard;
    c->mailCardRank = mailCardRank;
    c->idolCardRank = idolCardRank; c->idolCardSuit = idolCardSuit;
    c->castleCardSuit = castleCardSuit;
    c->ancientCardSuit = ancientCardSuit;
    c->heldJokerIdx = heldJokerIdx;
    c->mouthLockedType = mouthLockedType;
    c->handTypePlayedBeforeThisHand = handTypePlayedBeforeThisHand;
    c->handsPlayedThisRound = handsPlayedThisRound;
    c->bossTriggeredThisHand = bossTriggeredThisHand;
    c->pendingDoubleTags = pendingDoubleTags;
    c->unusedDiscardsThisRun = unusedDiscardsThisRun;
    c->blindGoal = blindGoal;
    c->sel = sel;
    c->permanentHandSizeBonus = permanentHandSizeBonus;
    c->skipTag = skipTag;

    // ---- privates (this is a member function, so allowed) ------------
    c->pending_ = pending_;                  // LIVE during PHASE_CASH_OUT
    c->copyDepth_ = 0;
    c->freezingPayout_ = false;
    c->pendingPayout_ = 0;
    c->destroyQueue_.clear();                // transient between actions

    // Passives depend only on jokers/vouchers, both copied: recompute so
    // mods can never silently drift from its inputs.
    c->recomputeMods();
    return c;
}
