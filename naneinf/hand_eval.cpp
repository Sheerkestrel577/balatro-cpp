// hand_eval.cpp — subset brute force. With <=5 cards there are at most 31
// non-empty subsets, so exhaustive checking is both trivial and exact.
#include "hand_eval.hpp"

#include <algorithm>
#include <array>

#include "joker.hpp"   // PassiveMods

const char* HAND_NAMES[NUM_HAND_TYPES] = {
    "High Card", "Pair", "Two Pair", "Three of a Kind", "Straight",
    "Flush", "Full House", "Four of a Kind", "Straight Flush",
    "Five of a Kind", "Flush House", "Flush Five"};

uint8_t detSuitMask(const Card& c, const PassiveMods& mods) {
    if (c.enhancement == ENH_STONE) return 0;
    if (c.enhancement == ENH_WILD) return 0b1111;
    uint8_t m = (uint8_t)(1u << c.suit);
    if (mods.smearedSuits) {
        // H<->D, S<->C
        if (m & ((1u << HEARTS) | (1u << DIAMONDS)))
            m |= (1u << HEARTS) | (1u << DIAMONDS);
        if (m & ((1u << SPADES) | (1u << CLUBS)))
            m |= (1u << SPADES) | (1u << CLUBS);
    }
    return m;
}

// Do these ranks (with an Ace-low retry) form a straight of exactly this
// subset's size, with per-step gap <= mods.straightGap?
static bool ranksFormStraight(std::vector<int> ranks, int maxGap) {
    std::sort(ranks.begin(), ranks.end());
    if (std::adjacent_find(ranks.begin(), ranks.end()) != ranks.end())
        return false;  // duplicates: not a straight
    auto ok = [&](const std::vector<int>& r) {
        for (size_t i = 1; i < r.size(); ++i)
            if (r[i] - r[i - 1] > maxGap) return false;
        return true;
    };
    if (ok(ranks)) return true;
    if (!ranks.empty() && ranks.back() == 14) {   // Ace low retry
        ranks.back() = 1;
        std::sort(ranks.begin(), ranks.end());
        return ok(ranks);
    }
    return false;
}

HandEvalResult evaluateHand(const std::vector<Card*>& played,
                            const PassiveMods& mods) {
    HandEvalResult res;
    const int n = (int)played.size();

    // Non-stone cards participate in hand determination.
    std::vector<int> det;               // indices into played
    for (int i = 0; i < n; ++i)
        if (played[i]->enhancement != ENH_STONE) det.push_back(i);
    const int dn = (int)det.size();

    const int L = mods.straightFlushLen;     // 5, or 4 with Four Fingers
    const int gap = mods.straightGap;        // 1, or 2 with Shortcut

    struct Best { int size = -1; uint32_t bits = 0; };
    std::array<Best, NUM_HAND_TYPES> best{};   // best subset per type
    auto note = [&](HandType t, int size, uint32_t bits) {
        res.containedHands |= (1u << t);
        if (size > best[t].size) best[t] = {size, bits};
    };

    if (dn > 0) {
        // rank-count based types over ALL determination cards
        std::array<int, 15> cnt{};
        for (int i : det) cnt[played[i]->rank]++;
        auto bitsOfRank = [&](int r, int howMany) {
            uint32_t b = 0;
            for (int i : det)
                if (played[i]->rank == r && howMany > 0) { b |= 1u << i; --howMany; }
            return b;
        };
        int bestSingle = -1;
        std::vector<int> pairs, trips;
        for (int r = 14; r >= 2; --r) {
            if (cnt[r] >= 1 && bestSingle < 0) bestSingle = r;
            if (cnt[r] >= 2) pairs.push_back(r);
            if (cnt[r] >= 3) trips.push_back(r);
            if (cnt[r] >= 4) note(FOUR_OAK, 4, bitsOfRank(r, 4));
            if (cnt[r] >= 5) note(FIVE_OAK, 5, bitsOfRank(r, 5));
        }
        note(HIGH_CARD, 1, bitsOfRank(bestSingle, 1));
        if (!pairs.empty()) note(PAIR, 2, bitsOfRank(pairs[0], 2));
        if (pairs.size() >= 2)
            note(TWO_PAIR, 4, bitsOfRank(pairs[0], 2) | bitsOfRank(pairs[1], 2));
        // ⚠ verify: does the game count 4oaK as Two Pair (two pairs of the
        // same rank)? Implemented YES pending in-game confirmation:
        else if (!pairs.empty() && cnt[pairs[0]] >= 4)
            note(TWO_PAIR, 4, bitsOfRank(pairs[0], 4));
        if (!trips.empty()) note(THREE_OAK, 3, bitsOfRank(trips[0], 3));
        for (int t : trips)
            for (int p : pairs)
                if (p != t)
                    note(FULL_HOUSE, 5, bitsOfRank(t, 3) | bitsOfRank(p, 2));

        // subset-based types (straight/flush families)
        for (uint32_t sub = 1; sub < (1u << dn); ++sub) {
            int size = __builtin_popcount(sub);
            if (size < L) continue;
            uint32_t bits = 0;
            uint8_t common = 0b1111;
            std::vector<int> ranks;
            for (int j = 0; j < dn; ++j)
                if (sub & (1u << j)) {
                    int i = det[j];
                    bits |= 1u << i;
                    common &= detSuitMask(*played[i], mods);
                    ranks.push_back(played[i]->rank);
                }
            bool flush = common != 0;
            bool straight = (size >= L) && ranksFormStraight(ranks, gap);
            if (flush && size >= L) {
                note(FLUSH, size, bits);
                // flush house / flush five: 5 same-suited cards shaped so
                if (size == 5) {
                    std::array<int, 15> c2{};
                    for (int r : ranks) c2[r]++;
                    bool fh = false, ff = false;
                    for (int r = 2; r <= 14; ++r) {
                        if (c2[r] == 5) ff = true;
                        if (c2[r] == 3)
                            for (int r2 = 2; r2 <= 14; ++r2)
                                if (r2 != r && c2[r2] == 2) fh = true;
                    }
                    if (fh) note(FLUSH_HOUSE, 5, bits);
                    if (ff) note(FLUSH_FIVE, 5, bits);
                }
            }
            if (straight) note(STRAIGHT, size, bits);
            if (straight && flush) note(STRAIGHT_FLUSH, size, bits);
        }
    } else if (n > 0) {
        // all stones: scores as High Card, stones score via the stone rule
        res.containedHands |= (1u << HIGH_CARD);
    }

    // scored type = highest contained
    for (int t = NUM_HAND_TYPES - 1; t >= 0; --t)
        if (res.containedHands & (1u << t)) { res.scoredType = t; break; }

    // scoring set: constituents of the scored type + stones + Splash
    uint32_t bits = best[res.scoredType].bits;
    for (int i = 0; i < n; ++i) {
        bool scores = mods.allCardsScore || (bits & (1u << i)) ||
                      played[i]->enhancement == ENH_STONE;
        if (scores) res.scoringIdx.push_back(i);
    }
    return res;
}
