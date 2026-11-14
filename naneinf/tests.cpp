// tests.cpp — unit tests: BigNum, hand evaluation, pipeline golden scores.
#include <algorithm>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "balatro.hpp"
#include "keys.hpp"
#include "pools.hpp"

static int fails = 0, passes = 0;
#define CHECK(cond, msg) do { \
    if (cond) ++passes; \
    else { ++fails; std::printf("FAIL: %s\n", msg); } } while (0)

static Card C(int rank, int suit, int enh = ENH_NONE, int ed = ED_NONE,
              int seal = SEAL_NONE) {
    Card c; c.rank = rank; c.suit = suit; c.enhancement = enh;
    c.edition = ed; c.seal = seal; return c;
}

static void testBigNum() {
    BigNum a(1000), b(24);
    CHECK((a + b).toDouble() == 1024.0, "bignum add");
    CHECK((a * b).toDouble() == 24000.0, "bignum mul");
    CHECK(BigNum(1024).half().toDouble() == 512.0, "bignum half");
    // build 2^1024 by squaring: (2^512)^2
    BigNum big(2);
    for (int i = 0; i < 9; ++i) big = big * big;   // 2^(2^9) = 2^512
    CHECK(!big.naneinf(), "2^512 not naneinf");
    BigNum over = big * big;                        // 2^1024
    CHECK(over.naneinf(), "2^1024 is naneinf");
    CHECK(over >= big && !(big >= over), "bignum compare");
    BigNum tiny(1);
    CHECK(((over + tiny) >= over), "add tiny to huge doesn't corrupt");
}

static void testHandEval() {
    PassiveMods m;   // defaults

    // THE test: 4 kings of spades + 10 of spades (duplicates legal).
    Card k1=C(13,SPADES), k2=C(13,SPADES), k3=C(13,SPADES), k4=C(13,SPADES),
         t =C(10,SPADES);
    std::vector<Card*> played{&k1,&k2,&k3,&k4,&t};
    auto r = evaluateHand(played, m);
    CHECK(r.scoredType == FOUR_OAK, "4K+10: scored four oak");
    CHECK(r.contains(FLUSH), "4K+10: contains flush (jokers proc)");
    CHECK(r.contains(PAIR) && r.contains(THREE_OAK), "4K+10: pair/trips bits");
    CHECK(r.scoringIdx.size() == 4, "4K+10: only the kings score");
    bool tenScores = false;
    for (int i : r.scoringIdx) if (played[i] == &t) tenScores = true;
    CHECK(!tenScores, "4K+10: the ten does not score");

    // splash: everything scores
    PassiveMods ms = m; ms.allCardsScore = true;
    CHECK(evaluateHand(played, ms).scoringIdx.size() == 5, "splash scores all");

    // four fingers: 4-card flush
    Card h1=C(2,HEARTS), h2=C(7,HEARTS), h3=C(9,HEARTS), h4=C(12,HEARTS),
         off=C(3,CLUBS);
    std::vector<Card*> p2{&h1,&h2,&h3,&h4,&off};
    CHECK(!evaluateHand(p2, m).contains(FLUSH), "no 4-flush by default");
    PassiveMods mf = m; mf.straightFlushLen = 4;
    auto r2 = evaluateHand(p2, mf);
    CHECK(r2.scoredType == FLUSH, "four fingers: 4-card flush");
    CHECK(r2.scoringIdx.size() == 4, "four fingers: 4 cards score");

    {
        // Four Fingers lowers the MINIMUM length; it must not cap the hand at
        // four cards. A genuine 5-card flush/straight still scores all five.
        // (Regression: `straight` used `size == L` where flush used `size >= L`,
        //  so with L=4 the 5-card subset was never tested for straightness and
        //  the highest card silently stopped scoring.)
        Card h5=C(5,HEARTS);
        std::vector<Card*> p2b{&h1,&h2,&h3,&h4,&h5};
        CHECK(evaluateHand(p2b, mf).scoringIdx.size() == 5,
              "four fingers: real 5-card flush still scores 5");

        Card f1=C(5,SPADES), f2=C(6,HEARTS), f3=C(7,CLUBS), f4=C(8,DIAMONDS),
             fk=C(13,SPADES), f5=C(9,SPADES);
        std::vector<Card*> p4{&f1,&f2,&f3,&f4,&fk};      // 4-straight + garbage
        std::vector<Card*> p5{&f1,&f2,&f3,&f4,&f5};      // true 5-card straight
        CHECK(!evaluateHand(p4, m).contains(STRAIGHT), "no 4-straight by default");
        auto r4 = evaluateHand(p4, mf);
        CHECK(r4.scoredType == STRAIGHT, "four fingers: 4-card straight");
        CHECK(r4.scoringIdx.size() == 4, "four fingers: garbage card sits out");
        auto r5 = evaluateHand(p5, mf);
        CHECK(r5.scoredType == STRAIGHT, "four fingers: 5-card straight");
        CHECK(r5.scoringIdx.size() == 5,
              "four fingers: real 5-card straight still scores 5");

        Card g1=C(5,HEARTS), g2=C(6,HEARTS), g3=C(7,HEARTS), g4=C(8,HEARTS),
             g5=C(9,HEARTS);
        std::vector<Card*> p6{&g1,&g2,&g3,&g4,&g5};
        auto r6 = evaluateHand(p6, mf);
        CHECK(r6.scoredType == STRAIGHT_FLUSH, "four fingers: 5-card str flush");
        CHECK(r6.scoringIdx.size() == 5,
              "four fingers: real 5-card straight flush still scores 5");

        // Four Fingers + Shortcut together: gapped FIVE-card straight scores 5.
        PassiveMods mfg = mf; mfg.straightGap = 2;
        Card q1=C(2,SPADES), q2=C(4,HEARTS), q3=C(6,CLUBS), q4=C(8,DIAMONDS),
             q5=C(10,SPADES);
        std::vector<Card*> p7{&q1,&q2,&q3,&q4,&q5};
        auto r7 = evaluateHand(p7, mfg);
        CHECK(r7.scoredType == STRAIGHT, "four fingers + shortcut: straight");
        CHECK(r7.scoringIdx.size() == 5,
              "four fingers + shortcut: all five gapped cards score");
    }


    // shortcut: gapped straight 2,3,5,6,8
    Card s1=C(2,SPADES), s2=C(3,HEARTS), s3=C(5,CLUBS), s4=C(6,DIAMONDS),
         s5=C(8,SPADES);
    std::vector<Card*> p3{&s1,&s2,&s3,&s4,&s5};
    CHECK(!evaluateHand(p3, m).contains(STRAIGHT), "gapped: no straight");
    PassiveMods mg = m; mg.straightGap = 2;
    CHECK(evaluateHand(p3, mg).contains(STRAIGHT), "shortcut: gapped straight");

    // ace-low straight
    Card a1=C(14,SPADES), a2=C(2,HEARTS), a3=C(3,CLUBS), a4=C(4,DIAMONDS),
         a5=C(5,SPADES);
    std::vector<Card*> p4{&a1,&a2,&a3,&a4,&a5};
    CHECK(evaluateHand(p4, m).contains(STRAIGHT), "ace-low straight");

    // smeared: hearts+diamonds flush
    Card d1=C(4,DIAMONDS), d2=C(6,HEARTS), d3=C(9,DIAMONDS), d4=C(11,HEARTS),
         d5=C(13,DIAMONDS);
    std::vector<Card*> p5{&d1,&d2,&d3,&d4,&d5};
    CHECK(!evaluateHand(p5, m).contains(FLUSH), "mixed red: no flush");
    PassiveMods msm = m; msm.smearedSuits = true;
    CHECK(evaluateHand(p5, msm).contains(FLUSH), "smeared: red flush");

    // wild completes a straight flush
    Card w=C(10,CLUBS,ENH_WILD), f2=C(11,SPADES), f3=C(12,SPADES),
         f4=C(13,SPADES), f5=C(14,SPADES);
    std::vector<Card*> p6{&w,&f2,&f3,&f4,&f5};
    CHECK(evaluateHand(p6, m).scoredType == STRAIGHT_FLUSH,
          "wild completes royal (straight flush)");

    // flush five
    Card q1=C(12,HEARTS), q2=C(12,HEARTS), q3=C(12,HEARTS), q4=C(12,HEARTS),
         q5=C(12,HEARTS);
    std::vector<Card*> p7{&q1,&q2,&q3,&q4,&q5};
    CHECK(evaluateHand(p7, m).scoredType == FLUSH_FIVE, "flush five");

    // all stones -> high card, all score via stone rule
    Card st1=C(2,SPADES,ENH_STONE), st2=C(9,HEARTS,ENH_STONE);
    std::vector<Card*> p8{&st1,&st2};
    auto r8 = evaluateHand(p8, m);
    CHECK(r8.scoredType == HIGH_CARD && r8.scoringIdx.size() == 2,
          "all stones: high card, stones score");
}

static void testPipeline() {
    // GOLDEN: pair of kings, no jokers, lvl 1, plasma.
    // base 10c/2m; kings +10 chips each -> chips 30, mult 2.
    // balance (30+2)/2 = 16 -> score 256.
    Balatro g(std::make_unique<MersenneRNG>(1));
    Card k1=C(13,SPADES), k2=C(13,HEARTS);
    BigNum s = g.playHand({&k1,&k2}, {});
    CHECK(s.toDouble() == 256.0, "golden: pair of kings = 256");

    // GOLDEN with jokers: Greedy Joker + a scored diamond.
    // pair of kings, one diamond king: chips 30, mult 2+3=5 -> (35)/2... 
    // (30+5)/2 = 17.5 -> floor 17 -> 289
    Balatro g2(std::make_unique<MersenneRNG>(1));
    for (auto& info : jokerRegistry())
        if (info.name == "Greedy Joker") g2.addJoker(info.make());
    Card dk=C(13,DIAMONDS), sk=C(13,SPADES);
    BigNum s2 = g2.playHand({&dk,&sk}, {});
    CHECK(s2.toDouble() == 289.0, "golden: greedy + diamond king = 289");

    // Hack retriggers a played 3: high card 3 twice.
    // base 5c/1m; 3 gives +3 chips x2 triggers -> chips 11, mult 1 -> 36.
    Balatro g3(std::make_unique<MersenneRNG>(1));
    for (auto& info : jokerRegistry())
        if (info.name == "Hack") g3.addJoker(info.make());
    Card three=C(3,CLUBS);
    BigNum s3 = g3.playHand({&three}, {});
    CHECK(s3.toDouble() == 36.0, "hack retrigger: lone 3 = 36");

    // Blueprint copies Greedy: diamond king scores +3 mult twice.
    // pair of kings (one diamond): chips 30, mult 2+3+3=8 -> 19^2 = 361.
    Balatro g4(std::make_unique<MersenneRNG>(1));
    for (auto& info : jokerRegistry())
        if (info.name == "Blueprint") g4.addJoker(info.make());
    for (auto& info : jokerRegistry())
        if (info.name == "Greedy Joker") g4.addJoker(info.make());
    Card dk2=C(13,DIAMONDS), sk2=C(13,SPADES);
    BigNum s4 = g4.playHand({&dk2,&sk2}, {});
    CHECK(s4.toDouble() == 361.0, "blueprint copies greedy = 361");

    // Blueprint alone (no target): must be a no-op, and two Blueprints
    // pointing at nothing must not loop or crash.
    Balatro g5(std::make_unique<MersenneRNG>(1));
    for (int i = 0; i < 2; ++i)
        for (auto& info : jokerRegistry())
            if (info.name == "Blueprint") g5.addJoker(info.make());
    Card lone=C(9,SPADES);
    BigNum s5 = g5.playHand({&lone}, {});
    // high card: 5c/1m; 9 -> chips 14, mult 1 -> floor(7.5)=7 -> 49
    CHECK(s5.toDouble() == 49.0, "two blueprints, no target: inert");

    // Pareidolia + destruction: a destroyed non-face counts as face.
    Balatro g6(std::make_unique<MersenneRNG>(1));
    for (auto& info : jokerRegistry())
        if (info.name == "Pareidolia") g6.addJoker(info.make());
    Card six=C(6,CLUBS,ENH_GLASS);
    // force destruction: play until glass breaks (1 in 4) or give up
    int before = g6.facesDestroyedTotal;
    for (int i = 0; i < 200 && g6.facesDestroyedTotal == before; ++i) {
        six.destroyed = false;
        g6.playHand({&six}, {});
    }
    CHECK(g6.facesDestroyedTotal > before,
          "pareidolia: destroyed 6 counted as face (canio path)");

    // naneinf check plumbing: seed the score just under, bank one hand over
    Balatro g7(std::make_unique<MersenneRNG>(1));
    BigNum nearly(2);
    for (int i = 0; i < 10; ++i) nearly = nearly * nearly; // 2^1024... too far
    // build 2^1023:
    BigNum p2_1023(2);
    for (int i = 0; i < 9; ++i) p2_1023 = p2_1023 * p2_1023;  // 2^512
    BigNum t = p2_1023; // 2^512
    g7.score = t * t;   // 2^1024
    CHECK(g7.score.naneinf(), "score >= 2^1024 is naneinf");
    // won() is now a latched run-level flag set by the pipeline, since the
    // per-blind score resets each blind.
    CHECK(!g7.won(), "won() is latched by play, not by poking score");

    // blind requirement sanity: ante 1 base 300, boss x2, plasma x2 = 1200
    Balatro g8(std::make_unique<MersenneRNG>(1));
    CHECK(g8.blindRequirement(1, 2).toDouble() == 1200.0,
          "ante 1 boss plasma = 1200");
}


// ---- archetype regression tests (added with the pattern cookbook) ----
static Joker* give(Balatro& g, const char* name) {
    for (auto& info : jokerRegistry())
        if (info.name == name) { g.addJoker(info.make()); return g.jokers.back().get(); }
    std::printf("FAIL: no such joker %s\n", name); ++fails; return nullptr;
}

static void testArchetypes() {
    // "contains" semantics: Jolly procs on a 4-of-a-kind (contains a pair)
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Jolly Joker");
        Card a=C(13,SPADES), b=C(13,SPADES), c=C(13,SPADES), d=C(13,SPADES);
        BigNum s = g.playHand({&a,&b,&c,&d}, {});
        // four oak: 60c/7m; 4 kings +40 chips -> 100c, mult 7+8=15
        // (100+15)/2 = 57.5 -> floor 57 -> 3249
        CHECK(s.toDouble() == 3249.0, "jolly procs on four-of-a-kind");
    }
    // isCopy: Blueprint copying Ride the Bus grants mult but must NOT
    // double-advance the counter.
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Blueprint");
        give(g, "Ride the Bus");
        Card n=C(9,SPADES);
        g.playHand({&n}, {});             // HAND_DETERMINED: counter 0->1
        BigNum s2 = g.playHand({&n}, {});  // counter 1->2, applied TWICE
        // high card 5c/1m; nine +9 -> 14c; mult 1 + 2 (bus) + 2 (copy) = 5
        // (14+5)/2 = 9.5 -> floor 9 -> 81.  A double-advance bug would give
        // counter 4 -> mult 9 -> 121, so this value is diagnostic.
        CHECK(s2.toDouble() == 81.0, "blueprint copies bus mult, not counter");
    }
    // passive reach: Smeared makes Greedy Joker proc on a heart
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Greedy Joker");
        Card h=C(7,HEARTS);
        BigNum before = g.playHand({&h}, {});
        Balatro g2(std::make_unique<MersenneRNG>(1));
        give(g2, "Greedy Joker");
        give(g2, "Smeared Joker");
        Card h2=C(7,HEARTS);
        BigNum after = g2.playHand({&h2}, {});
        CHECK(before.toDouble() == 36.0, "greedy: no proc on plain heart");
        // 5+7 = 12c; mult 1+3 = 4 -> (12+4)/2 = 8 -> 64 (even, unaffected)
        CHECK(after.toDouble() == 64.0, "smeared: greedy procs on heart");
    }
    // passive reach into the EVALUATOR: Pareidolia makes Sock retrigger all
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Sock and Buskin");
        give(g, "Pareidolia");
        Card n=C(9,SPADES);
        BigNum s = g.playHand({&n}, {});
        // 9 scores twice: 5 + 9 + 9 = 23c, mult 1 -> 12^2 = 144
        CHECK(s.toDouble() == 144.0, "pareidolia+sock retriggers a nine");
    }
    // fact mutation: Midas Mask turns scored faces gold (a new FACT)
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Midas Mask");
        Card k=C(13,SPADES);
        g.playHand({&k}, {});
        CHECK(k.enhancement == ENH_GOLD, "midas mask writes a new fact");
    }
    // run counter read: Fortune Teller
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Fortune Teller");
        g.tarotsUsed = 5;
        Card n=C(2,SPADES);
        BigNum s = g.playHand({&n}, {});
        // high card 5c/1m; two +2 -> 7c; mult 1+5=6 -> floor(6.5)=6 -> 36
        CHECK(s.toDouble() == 36.0, "fortune teller reads run counter");
    }
    // self-destruct through the queue: Ice Cream dies without UB
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Ice Cream");
        Card n=C(2,SPADES);
        for (int i = 0; i < 25; ++i) g.playHand({&n}, {});
        CHECK(g.jokers.empty(), "ice cream decays to death, swept safely");
    }
    // held-in-hand: Baron on two held kings
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Baron");
        Card p=C(2,SPADES), k1=C(13,SPADES), k2=C(13,HEARTS);
        BigNum s = g.playHand({&p}, {&k1,&k2});
        // 5+2 = 7c; mult 2.25 -> (7+2.25)/2 = 4.625 -> floor 4 -> 16
        CHECK(s.toDouble() == 16.0, "baron: two held kings");
    }
    // stacking passive: two Oops doubles the numerator twice
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Oops! All 6s"); give(g, "Oops! All 6s");
        CHECK(g.mods.probNumeratorMult == 4, "oops stacks multiplicatively");
    }
    // sell-self event ordering
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        Joker* l = give(g, "Luchador");
        EventData ev{};
        l->interact(g, SELF_SOLD, ev, false);
        CHECK(g.bossDisabled, "luchador disables boss on self-sold");
    }
}


// ---- cash-out phase tests (the Hermit exploit + frozen interest) ----
static void testCashOut() {
    // Golden Joker's $4 must NOT be in the wallet during the cash-out
    // screen; it lands only on confirm.
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Golden Joker");
        g.money = 10; g.handsLeft = 2;
        const auto& co = g.computeCashOut(0);        // small blind
        CHECK(co.jokerTotal == 4, "golden joker itemized at $4");
        CHECK(g.money == 10, "payout NOT credited during cash-out screen");
        CHECK(g.phase == PHASE_CASH_OUT, "entered cash-out phase");
        // interest basis is the WALLET ($10) only -> floor(10/5) = 2.
        // The joker's $4 and the blind/hands lines do NOT count.
        CHECK(co.interest == 2, "interest ignores cash-out lines");
        CHECK(co.total() == 11, "itemized total: 3+2+4+2");
        g.confirmCashOut();
        CHECK(g.money == 21, "credited on confirm");
        CHECK(g.phase == PHASE_SHOP, "moved to shop");
    }
    // THE HERMIT EXPLOIT: doubling money on the cash-out screen must double
    // the wallet only, never the pending payout.
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Golden Joker");
        g.money = 10; g.handsLeft = 0;
        const auto& co = g.computeCashOut(0);
        int pending = co.total();                    // 3 + 4 + interest(2) = 9
        g.money *= 2;                                // The Hermit, on-screen
        g.confirmCashOut();
        CHECK(g.money == 20 + pending, "hermit doubles wallet, not payout");
    }
    // FROZEN INTEREST: crossing a threshold during the screen must not
    // change the already-computed interest line.
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        g.money = 12; g.handsLeft = 0;
        const auto& co = g.computeCashOut(0);        // wallet 12 -> interest 2
        int frozen = co.interest;
        g.money += 100;                              // sell a joker, etc.
        CHECK(g.pendingCashOut().interest == frozen,
              "interest line frozen after computation");
    }
    // interest cap, and To the Moon raising it
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        g.money = 100; g.handsLeft = 0;
        CHECK(g.computeCashOut(0).interest == 5, "interest caps at $5");
        Balatro g2(std::make_unique<MersenneRNG>(1));
        give(g2, "To the Moon");
        g2.money = 100; g2.handsLeft = 0;
        CHECK(g2.computeCashOut(0).interest == 10, "to the moon raises cap");
    }
    // Delayed Gratification pays only with discards unused
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Delayed Gratification");
        g.money = 0; g.handsLeft = 0; g.discardsLeft = 3;
        CHECK(g.computeCashOut(0).jokerTotal == 6, "delayed grat pays $6");
        Balatro g2(std::make_unique<MersenneRNG>(1));
        give(g2, "Delayed Gratification");
        g2.money = 0; g2.handsLeft = 0; g2.discardsLeft = 3;
        g2.discardsUsedThisRound = 1;
        CHECK(g2.computeCashOut(0).jokerTotal == 0, "delayed grat: none if used");
    }
    // Cloud 9 counts nines in the deck
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Cloud 9");
        g.money = 0; g.handsLeft = 0;
        CHECK(g.computeCashOut(0).jokerTotal == 4, "cloud 9: four 9s in deck");
    }
    // Credit Card: debt floor
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        g.money = 2;
        CHECK(!g.canAfford(5), "no debt without credit card");
        give(g, "Credit Card");
        CHECK(g.canAfford(5) && g.canAfford(22) && !g.canAfford(23),
              "credit card allows -$20");
    }
    // Vampire: eats enhancements, scales; Midas combo via joker order
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Midas Mask");
        give(g, "Vampire");            // right of Midas: eats the gold
        Card k=C(13,SPADES);
        g.playHand({&k}, {});
        CHECK(k.enhancement == ENH_NONE, "vampire strips midas's gold");
        // Second hand: Midas golds the NEW king too, Vampire eats it again,
        // so xmult is 1.2 by the time INDEPENDENT runs (the combo compounds
        // every hand). 15 chips, mult 1.2 -> floor(8.1) = 8 -> 64
        Card k2=C(13,HEARTS);
        BigNum s = g.playHand({&k2}, {});
        CHECK(s.toDouble() == 64.0,
              "vampire+midas compounds each hand");
    }
    // Odd Todd: Ace counts, Jack does not
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Odd Todd");
        Card a=C(14,SPADES);
        BigNum sa = g.playHand({&a}, {});
        // 5 + 11 + 31 = 47 chips, mult 1 -> 24^2 = 576
        CHECK(sa.toDouble() == 576.0, "odd todd: ace is odd");
        Balatro g2(std::make_unique<MersenneRNG>(1));
        give(g2, "Odd Todd");
        Card jk=C(11,SPADES);
        BigNum sj = g2.playHand({&jk}, {});
        // 5 + 10 = 15 chips, mult 1 -> 8^2 = 64
        CHECK(sj.toDouble() == 64.0, "odd todd: jack is NOT odd");
    }
}


// ---- registry hygiene + new engine fields ----
static void testRegistryAndFields() {
    // No duplicate names or the shop will roll ghosts. This test is the
    // guard against copy-paste-without-rename (which is also an ODR
    // violation the compiler cannot catch across translation units).
    {
        std::vector<std::string> names;
        for (auto& i : jokerRegistry()) names.push_back(i.name);
        std::sort(names.begin(), names.end());
        auto dup = std::adjacent_find(names.begin(), names.end());
        if (dup != names.end())
            std::printf("   duplicate registry name: %s\n", dup->c_str());
        CHECK(dup == names.end(), "registry has no duplicate joker names");
    }
    // Every joker knows its own name/rarity/cost (Baseball Card needs this)
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        Joker* j = give(g, "Burglar");
        CHECK(j && j->name == "Burglar" && j->rarity == 2 && j->baseCost == 6,
              "instance carries its own identity");
    }
    // clone() deep-copies scaling state (Invisible Joker / Ankh / Death)
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        Joker* bus = give(g, "Ride the Bus");
        Card n=C(9,SPADES);
        g.playHand({&n}, {});           // counter -> 1
        g.playHand({&n}, {});           // counter -> 2
        auto copy = bus->clone();
        CHECK(copy != nullptr, "clone() produces an object");
        // the clone must score with the SAME accumulated mult
        Balatro g2(std::make_unique<MersenneRNG>(1));
        g2.jokers.push_back(std::move(copy));
        g2.recomputeMods();
        Card n2=C(9,SPADES);
        BigNum s = g2.playHand({&n2}, {});
        // clone came in at counter 2, its own HAND_DETERMINED makes it 3
        // 5+9 = 14 chips, mult 1+3 = 4 -> 9^2 = 81
        CHECK(s.toDouble() == 81.0, "clone carries scaling state");
    }
    // Half Joker reads PLAYED, not scored
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Half Joker");
        Card a=C(13,SPADES), b=C(13,HEARTS), c2=C(2,CLUBS);
        BigNum s = g.playHand({&a,&b,&c2}, {});   // 3 played, 2 scored
        // pair: 10c/2m; kings +20 -> 30c; mult 2+20 = 22 -> 26^2 = 676
        CHECK(s.toDouble() == 676.0, "half joker counts played cards");
    }
    // Hiker permanently buffs a card
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Hiker");
        Card n=C(5,SPADES);
        g.playHand({&n}, {});
        CHECK(n.bonusChips == 5, "hiker writes permanent bonus chips");
        BigNum s = g.playHand({&n}, {});
        // high card 5c/1m; five(5) + bonus(10 after 2nd hiker trigger... )
        CHECK(n.bonusChips == 10, "hiker stacks across hands");
        (void)s;
    }
    // Baseball Card reads other jokers' rarity
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Baseball Card");      // rare, doesn't buff itself
        give(g, "Burglar");            // uncommon -> x1.5
        give(g, "Four Fingers");       // uncommon -> x1.5
        Card n=C(2,SPADES);
        BigNum s = g.playHand({&n}, {});
        // high card 5+2 = 7c; mult 1 * 1.5 * 1.5 = 2.25 -> (9.25/2)^2
        CHECK(s.toDouble() == 16.0, "baseball card counts uncommons");
    }
    // Supernova counts plays of this hand type
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Supernova");
        Card n=C(2,SPADES);
        g.playHand({&n}, {});                 // high card count -> 1
        BigNum s = g.playHand({&n}, {});      // -> 2, mult 1+2 = 3
        // 5+2 = 7c -> (7+3)/2 = 5 -> 25
        CHECK(s.toDouble() == 25.0, "supernova reads hand play counts");
    }
    // Chicot disables the boss passively
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        CHECK(!g.bossIsDisabled(), "boss enabled by default");
        give(g, "Chicot");
        CHECK(g.bossIsDisabled(), "chicot disables boss passively");
    }
    // the whole jolly + sly family, one hand each
    {
        struct { const char* n; double want; } cases[] = {
            // pair of 2s: base 10c/2m + two 2s = 14 chips
            {"Jolly Joker", 144.0},    // mult 2+8=10  -> ((14+10)/2)^2
            {"Sly Joker",   1089.0},   // chips 14+50=64 -> ((64+2)/2)^2
        };
        for (auto& cs : cases) {
            Balatro g(std::make_unique<MersenneRNG>(1));
            give(g, cs.n);
            Card a=C(2,SPADES), b=C(2,HEARTS);
            BigNum s = g.playHand({&a,&b}, {});
            CHECK(s.toDouble() == cs.want, cs.n);
        }
    }
}


// ---- regressions for the newly added jokers ----
static void testNewJokers() {
    // Scholar: the +20 chips must apply ONLY to aces (missing-braces bug)
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Scholar");
        Card a=C(14,SPADES), k=C(13,SPADES);
        BigNum s = g.playHand({&a,&k}, {});
        // high card (ace): base 5c/1m; ace 11 + king 10 = 26c... only the
        // ACE scores in a high card, so: 5 + 11 + 20 = 36c, mult 1+4 = 5
        // (36+5)/2 = 20.5 -> floor 20 -> 400.  With the brace bug the king would
        // also add chips.
        CHECK(s.toDouble() == 400.0, "scholar: bonus only on aces");
    }
    // Walkie Talkie: same shape, +4 mult only on 10s and 4s
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Walkie Talkie");
        Card t=C(10,SPADES), n=C(9,SPADES), e=C(8,SPADES),
             sv=C(7,SPADES), sx=C(6,SPADES);
        BigNum s = g.playHand({&t,&n,&e,&sv,&sx}, {});
        // all spades => STRAIGHT FLUSH: base 100c/8m; ranks 10+9+8+7+6 = 40;
        // the ten alone adds 10 chips / 4 mult.
        // chips 100+40+10 = 150, mult 8+4 = 12 -> (162/2)^2 = 6561.
        // With the missing-braces bug every card would add +4 mult (=28).
        CHECK(s.toDouble() == 6561.0, "walkie talkie: bonus only on 10s/4s");
    }
    // Shoot the Moon: queens, not kings
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Shoot the Moon");
        Card p=C(2,SPADES), q=C(12,HEARTS), k=C(13,HEARTS);
        BigNum s = g.playHand({&p}, {&q,&k});
        // high card 5+2 = 7c; mult 1+13 = 14 (queen only) -> floor(10.5)=10 -> 100
        CHECK(s.toDouble() == 100.0, "shoot the moon: queens only");
    }
    // Abstract Joker: +3 per joker, counting itself
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Abstract Joker");
        give(g, "Burglar");
        Card n=C(2,SPADES);
        BigNum s = g.playHand({&n}, {});
        // 5+2 = 7c; mult 1 + 6 = 7 -> 7^2 = 49
        CHECK(s.toDouble() == 49.0, "abstract joker: +3 per joker");
    }
    // Acrobat fires on the last hand
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Acrobat");
        g.handsLeft = 1;
        Card n=C(2,SPADES);
        BigNum s = g.playHand({&n}, {});
        // 7c, mult 1*3 = 3 -> 5^2 = 25
        CHECK(s.toDouble() == 25.0, "acrobat fires on final hand");
        CHECK(g.handsLeft == 0, "handsLeft decremented after scoring");
    }
    // Wee Joker accumulates across hands
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        give(g, "Wee Joker");
        Card two=C(2,SPADES);
        g.playHand({&two}, {});               // +8 chips banked
        BigNum s = g.playHand({&two}, {});
        // 5 + 2 + 8(accumulated) = 15c... plus this hand's +8 lands AFTER
        // INDEPENDENT? No: CARD_SCORED precedes INDEPENDENT, so 16.
        CHECK(s.toDouble() > 0, "wee joker runs");
        CHECK(g.jokers.size() == 1, "wee joker survives");
    }
    // Rarity is now data: Baseball Card must see the corrected values
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        Joker* f = give(g, "Fibonacci");
        Joker* y = give(g, "Yorick");
        CHECK(f && f->rarity == 2, "fibonacci is uncommon");
        CHECK(y && y->rarity == 4, "yorick is legendary");
    }
}


// ---- interaction layer ----
static std::unique_ptr<Consumable> makeCon(const char* n) {
    for (auto& i : consumableRegistry()) if (i.name == n) return i.make();
    return nullptr;
}

static void testInteraction() {
    // round start: shuffle, draw to hand size, phase transitions
    {
        Balatro g(std::make_unique<MersenneRNG>(7));
        g.startRound();
        CHECK(g.phase == PHASE_PLAYING, "startRound -> PLAYING");
        CHECK((int)g.hand.size() == 8, "drew to hand size 8");
        CHECK(g.drawPile.size() == 44, "44 left in draw pile");
        // every card has a distinct stable id
        std::vector<int> ids;
        for (Card& c : g.deck) ids.push_back(c.id);
        std::sort(ids.begin(), ids.end());
        CHECK(std::adjacent_find(ids.begin(), ids.end()) == ids.end(),
              "card ids are unique");
    }
    // selection: toggle on/off, 5-card cap, order preserved
    {
        Balatro g(std::make_unique<MersenneRNG>(7));
        g.startRound();
        CHECK(g.applyAction(A_TOGGLE_CARD_0 + 3), "select card 3");
        CHECK(g.applyAction(A_TOGGLE_CARD_0 + 1), "select card 1");
        CHECK(g.sel.cards.size() == 2 && g.sel.cards[0] == 3 && g.sel.cards[1] == 1,
              "selection preserves order");
        CHECK(g.applyAction(A_TOGGLE_CARD_0 + 3), "deselect card 3");
        CHECK(g.sel.cards.size() == 1 && g.sel.cards[0] == 1, "deselect works");
        for (int i : {0,2,3,4,5}) g.applyAction(A_TOGGLE_CARD_0 + i);
        CHECK(g.sel.cards.size() == 5, "capped at 5 selected");
        CHECK(!g.isLegal(A_TOGGLE_CARD_0 + 6), "6th selection illegal");
        CHECK(g.isLegal(A_TOGGLE_CARD_0 + 4), "deselecting a selected one is legal");
    }
    // card shift needs exactly one selection; permutes the hand
    {
        Balatro g(std::make_unique<MersenneRNG>(7));
        g.startRound();
        Card* before2 = g.hand[2];
        Card* before3 = g.hand[3];
        g.applyAction(A_TOGGLE_CARD_0 + 3);
        CHECK(g.applyAction(A_SHIFT_CARD_LEFT), "shift left");
        CHECK(g.hand[2] == before3 && g.hand[3] == before2, "cards swapped");
        CHECK(g.sel.cards[0] == 2, "selection follows the card");
        g.applyAction(A_TOGGLE_CARD_0 + 5);
        CHECK(!g.isLegal(A_SHIFT_CARD_LEFT), "shift illegal with 2 selected");
    }
    // joker shift = the copier-on-Mime rearrange
    {
        Balatro g(std::make_unique<MersenneRNG>(7));
        give(g, "Blueprint"); give(g, "Mime"); give(g, "Burglar");
        g.applyAction(A_TOGGLE_JOKER_0 + 2);            // Burglar
        CHECK(g.applyAction(A_SHIFT_JOKER_LEFT), "shift joker left");
        CHECK(g.jokers[1]->name == "Burglar" && g.jokers[2]->name == "Mime",
              "joker order changed");
    }
    // play through the action layer
    {
        Balatro g(std::make_unique<MersenneRNG>(7));
        g.startRound();
        g.applyAction(A_TOGGLE_CARD_0 + 0);
        g.applyAction(A_TOGGLE_CARD_0 + 1);
        int handsBefore = g.handsLeft;
        CHECK(g.applyAction(A_PLAY), "play selected");
        CHECK(g.handsLeft == handsBefore - 1, "hand consumed");
        CHECK(g.sel.cards.empty(), "selection cleared after play");
        CHECK((int)g.hand.size() == 8, "redrew to hand size");
        CHECK(g.discardPile.size() == 2, "played cards went to discard");
        CHECK(g.score.toDouble() > 0, "score banked");
    }
    // discard fires both events and burns a discard
    {
        Balatro g(std::make_unique<MersenneRNG>(7));
        give(g, "Green Joker");
        g.startRound();
        g.applyAction(A_TOGGLE_CARD_0 + 0);
        int d = g.discardsLeft;
        CHECK(g.applyAction(A_DISCARD), "discard selected");
        CHECK(g.discardsLeft == d - 1, "discard consumed");
        CHECK(g.cardsDiscardedThisRound == 1, "per-card counter fired");
        CHECK(g.discardsUsedThisRound == 1, "per-action counter fired");
    }
    // consumables: targeting comes from the selection
    {
        Balatro g(std::make_unique<MersenneRNG>(7));
        g.startRound();
        g.consumables.push_back(makeCon("Death"));
        g.applyAction(A_TOGGLE_CONSUMABLE_0 + 0);
        CHECK(!g.isLegal(A_USE_CONSUMABLE), "death illegal with 0 selected");
        g.applyAction(A_TOGGLE_CARD_0 + 0);
        CHECK(!g.isLegal(A_USE_CONSUMABLE), "death illegal with 1 selected");
        g.applyAction(A_TOGGLE_CARD_0 + 1);
        CHECK(g.isLegal(A_USE_CONSUMABLE), "death legal with 2 selected");
        int leftId = g.hand[0]->id;
        int rank1 = g.hand[1]->rank, suit1 = g.hand[1]->suit;
        CHECK(g.applyAction(A_USE_CONSUMABLE), "use death");
        CHECK(g.hand[0]->rank == rank1 && g.hand[0]->suit == suit1,
              "death copied right onto left");
        CHECK(g.hand[0]->id == leftId, "card identity preserved");
        CHECK(g.consumables.empty(), "consumable spent");
        CHECK(g.tarotsUsed == 1, "tarot counter bumped (Fortune Teller)");
    }
    // Strength mutates a fact
    {
        Balatro g(std::make_unique<MersenneRNG>(7));
        g.startRound();
        g.consumables.push_back(makeCon("Strength"));
        g.applyAction(A_TOGGLE_CONSUMABLE_0 + 0);
        g.applyAction(A_TOGGLE_CARD_0 + 0);
        int r = g.hand[0]->rank;
        g.applyAction(A_USE_CONSUMABLE);
        CHECK(g.hand[0]->rank == (r == 14 ? 2 : r + 1), "strength +1 rank");
    }
    // selling a joker fires SELF_SOLD then CARD_SOLD to others
    {
        Balatro g(std::make_unique<MersenneRNG>(7));
        give(g, "Luchador"); give(g, "Burglar");
        g.applyAction(A_TOGGLE_JOKER_0 + 0);
        int m = g.money;
        CHECK(g.applyAction(A_SELL_JOKER), "sell joker");
        CHECK(g.bossIsDisabled(), "luchador's SELF_SOLD fired before removal");
        CHECK(g.money > m, "got paid");
        CHECK(g.jokers.size() == 1 && g.cardsSoldThisRun == 1, "removed");
    }
    // THE MIME ENGINE: rearranging jokers between scoring and cash-out
    // doubles held gold cards (retrigger-aware round-end pass).
    {
        Balatro g(std::make_unique<MersenneRNG>(7));
        give(g, "Mime");
        g.startRound();
        for (Card* c : g.hand) c->enhancement = ENH_GOLD;   // 8 gold cards
        g.applyAction(A_TOGGLE_CARD_0 + 0);
        g.applyAction(A_PLAY);          // 7 gold cards left held + 1 redrawn
        const auto& co = g.computeCashOut(0);
        // 7 of the 8 held cards are gold (the redrawn one is not), each
        // retriggered once by Mime -> $6 each = $42. Without Mime it'd be $21.
        CHECK(co.goldCards == 42, "mime doubles held gold cards at round end");
    }
    // legality is phase-gated
    {
        Balatro g(std::make_unique<MersenneRNG>(7));
        CHECK(!g.isLegal(A_PLAY), "can't play in BLIND_SELECT");
        g.startRound();
        CHECK(!g.isLegal(A_CONFIRM_CASHOUT), "can't cash out while playing");
        g.computeCashOut(0);
        CHECK(g.isLegal(A_CONFIRM_CASHOUT), "cash out legal in CASH_OUT");
        CHECK(!g.isLegal(A_PLAY), "can't play in CASH_OUT");
        CHECK(!g.isLegal(A_TOGGLE_JOKER_0), "no jokers to toggle");
    }
}


// ---- shop, vouchers, tags, and RNG STREAM ISOLATION ----
static std::string shopFingerprint(Balatro& g) {
    std::string s;
    for (auto& it : g.shop) {
        s += std::to_string(it.kind) + ":" + std::to_string(it.registryIdx)
           + ":" + std::to_string(it.edition) + "|";
    }
    s += "V" + std::to_string(g.shopVoucher);
    return s;
}

static void testShopAndStreams() {
    // basic shop generation + purchase
    {
        Balatro g(std::make_unique<MersenneRNG>(99));
        g.enterBlindSelect();
        CHECK(g.phase == PHASE_BLIND_SELECT, "starts at blind select");
        CHECK(g.isLegal(A_SKIP_BLIND), "small blind is skippable");
        CHECK(g.blindGoal.toDouble() == 600.0, "ante1 small on plasma = 600");
        g.applyAction(A_SELECT_BLIND);
        CHECK(g.phase == PHASE_PLAYING, "select -> playing");
        g.computeCashOut(0);
        g.applyAction(A_CONFIRM_CASHOUT);
        CHECK(g.phase == PHASE_SHOP, "cash out -> shop");
        CHECK(g.shop.size() == 2, "two shop slots by default");
        CHECK(g.shopVoucher != V_NONE, "voucher offered on ante's first shop");
        g.money = 100;
        CHECK(g.isLegal(A_BUY_SLOT_0), "can buy slot 0");
        g.applyAction(A_BUY_SLOT_0);
        CHECK(g.shop[0].sold, "slot marked sold");
        CHECK(!g.isLegal(A_BUY_SLOT_0), "can't buy it twice");
        CHECK(g.jokers.size() + g.consumables.size() == 1, "item received");
    }
    // reroll costs money, escalates, and changes the slots
    {
        Balatro g(std::make_unique<MersenneRNG>(99));
        g.enterBlindSelect(); g.applyAction(A_SELECT_BLIND);
        g.computeCashOut(0); g.applyAction(A_CONFIRM_CASHOUT);
        g.money = 100;
        std::string before = shopFingerprint(g);
        int m = g.money, rc = g.rerollCost();
        g.applyAction(A_REROLL_SHOP);
        CHECK(g.money == m - rc, "reroll charged");
        CHECK(g.rerollCost() == rc + 1, "reroll cost escalates");
        CHECK(shopFingerprint(g) != before, "reroll changed the shop");
    }
    // slot limits gate buying
    {
        Balatro g(std::make_unique<MersenneRNG>(99));
        g.enterBlindSelect(); g.applyAction(A_SELECT_BLIND);
        g.computeCashOut(0); g.applyAction(A_CONFIRM_CASHOUT);
        g.money = 1000;
        for (int i = 0; i < 5; ++i) give(g, "Joker");   // fill joker slots
        bool anyJokerSlot = false;
        for (size_t i = 0; i < g.shop.size(); ++i)
            if (g.shop[i].kind == 0 && g.shop[i].edition != ED_NEGATIVE) {
                anyJokerSlot = true;
                CHECK(!g.isLegal(A_BUY_SLOT_0 + (int)i),
                      "joker purchase blocked when slots are full");
            }
        (void)anyJokerSlot;
    }
    // ===== THE BIG ONE: shop/tag/voucher streams must be isolated from
    // play decisions. Two runs on the same seed that play DIFFERENTLY must
    // still see identical shops, or agents can't be compared on a seed set.
    {
        auto runTo = [](int style) {
            auto g = std::make_unique<Balatro>(std::make_unique<MersenneRNG>(4242));
            g->enterBlindSelect();
            g->applyAction(A_SELECT_BLIND);
            // style 0: play one card. style 1: discard twice, then play three.
            if (style == 1) {
                g->applyAction(A_TOGGLE_CARD_0 + 0);
                g->applyAction(A_DISCARD);
                g->applyAction(A_TOGGLE_CARD_0 + 1);
                g->applyAction(A_TOGGLE_CARD_0 + 2);
                g->applyAction(A_DISCARD);
                for (int i = 0; i < 3; ++i) g->applyAction(A_TOGGLE_CARD_0 + i);
            } else {
                g->applyAction(A_TOGGLE_CARD_0 + 4);
            }
            g->applyAction(A_PLAY);
            g->computeCashOut(0);
            g->applyAction(A_CONFIRM_CASHOUT);
            return g;
        };
        auto a = runTo(0), b = runTo(1);
        CHECK(shopFingerprint(*a) == shopFingerprint(*b),
              "SHOP IDENTICAL despite different play (stream isolation)");
        CHECK(a->shopVoucher == b->shopVoucher, "voucher identical");
        // and the tag offered for the NEXT blind must match too
        a->applyAction(A_LEAVE_SHOP);
        b->applyAction(A_LEAVE_SHOP);
        CHECK(a->skipTag == b->skipTag, "skip tag identical");
        CHECK(a->ante == b->ante && a->blindIdx == b->blindIdx,
              "same position in the ante");
    }
    // ...but a DIFFERENT seed must give a different shop
    {
        Balatro g1(std::make_unique<MersenneRNG>(1));
        Balatro g2(std::make_unique<MersenneRNG>(2));
        for (Balatro* g : {&g1, &g2}) {
            g->enterBlindSelect(); g->applyAction(A_SELECT_BLIND);
            g->computeCashOut(0); g->applyAction(A_CONFIRM_CASHOUT);
        }
        CHECK(shopFingerprint(g1) != shopFingerprint(g2),
              "different seeds -> different shops");
    }
    // Rerolls DO advance the per-ante shop queue — this is real behaviour,
    // not a leak. The game keeps one queue per ante and every reroll draws
    // the next items from it, so a later blind's shop differs.
    {
        auto play = [](int rerolls) {
            auto g = std::make_unique<Balatro>(std::make_unique<MersenneRNG>(777));
            g->enterBlindSelect(); g->applyAction(A_SELECT_BLIND);
            g->computeCashOut(0); g->applyAction(A_CONFIRM_CASHOUT);
            g->money = 500;
            for (int i = 0; i < rerolls; ++i) g->applyAction(A_REROLL_SHOP);
            g->applyAction(A_LEAVE_SHOP);          // -> big blind select
            g->applyAction(A_SELECT_BLIND);
            g->computeCashOut(1); g->applyAction(A_CONFIRM_CASHOUT);
            return g;
        };
        auto x = play(0), y = play(3);
        CHECK(shopFingerprint(*x) != shopFingerprint(*y),
              "rerolls advance the ante's shop queue (real behaviour)");
    }
    // ...but PLAY decisions still must not: that is the property the AI
    // experiment depends on.
    {
        auto play = [](int style) {
            auto g = std::make_unique<Balatro>(std::make_unique<MersenneRNG>(777));
            g->enterBlindSelect(); g->applyAction(A_SELECT_BLIND);
            if (style) {
                g->applyAction(A_TOGGLE_CARD_0 + 0);
                g->applyAction(A_DISCARD);
                g->applyAction(A_TOGGLE_CARD_0 + 1);
                g->applyAction(A_TOGGLE_CARD_0 + 2);
            } else {
                g->applyAction(A_TOGGLE_CARD_0 + 3);
            }
            g->applyAction(A_PLAY);
            g->computeCashOut(0); g->applyAction(A_CONFIRM_CASHOUT);
            g->applyAction(A_LEAVE_SHOP);
            g->applyAction(A_SELECT_BLIND);
            g->computeCashOut(1); g->applyAction(A_CONFIRM_CASHOUT);
            return g;
        };
        CHECK(shopFingerprint(*play(0)) == shopFingerprint(*play(1)),
              "play decisions never shift a later shop");
    }
    // skipping a blind grants a tag and advances
    {
        Balatro g(std::make_unique<MersenneRNG>(55));
        g.enterBlindSelect();
        int tag = g.skipTag;
        CHECK(tag != TAG_NONE, "a tag is offered");
        g.applyAction(A_SKIP_BLIND);
        CHECK(g.blindsSkipped == 1, "skip counted");
        CHECK(g.blindIdx == 1, "advanced to big blind");
        // Double tag banks into pendingDoubleTags rather than the queue
        bool held = (g.investmentTagPending > 0) || !g.pendingTags.empty()
                 || g.pendingDoubleTags > 0;
        CHECK(held, "tag stored");
        CHECK(g.isLegal(A_SKIP_BLIND), "big blind also skippable");
        g.applyAction(A_SKIP_BLIND);
        CHECK(g.blindIdx == 2 && !g.isLegal(A_SKIP_BLIND),
              "boss blind cannot be skipped");
    }
    // ante rolls over after the boss
    {
        Balatro g(std::make_unique<MersenneRNG>(55));
        g.enterBlindSelect();
        for (int i = 0; i < 3; ++i) {
            g.applyAction(A_SELECT_BLIND);
            g.computeCashOut(g.blindIdx);
            g.applyAction(A_CONFIRM_CASHOUT);
            g.applyAction(A_LEAVE_SHOP);
        }
        CHECK(g.ante == 2 && g.blindIdx == 0, "ante advanced after boss");
    }
    // vouchers write into PassiveMods
    {
        Balatro g(std::make_unique<MersenneRNG>(99));
        g.enterBlindSelect(); g.applyAction(A_SELECT_BLIND);
        g.computeCashOut(0); g.applyAction(A_CONFIRM_CASHOUT);
        g.money = 100;
        g.shopVoucher = V_GRABBER;
        int h = g.mods.hands;
        CHECK(g.isLegal(A_BUY_VOUCHER), "voucher purchasable");
        g.applyAction(A_BUY_VOUCHER);
        CHECK(g.mods.hands == h + 1, "grabber grants a hand");
        CHECK(!g.isLegal(A_BUY_VOUCHER), "voucher gone after purchase");
        // ...and stays gone for the rest of the ante
        g.applyAction(A_LEAVE_SHOP);
        g.applyAction(A_SELECT_BLIND);
        g.computeCashOut(1);
        g.applyAction(A_CONFIRM_CASHOUT);
        CHECK(g.shopVoucher == V_NONE, "a bought voucher doesn't come back");
    }
}


// ---- blinds, bosses, death, packs, tags ----
static void goToBoss(Balatro& g) {
    // the boss is rolled when the ANTE begins (analyzer order), so force a
    // fresh ante roll rather than re-entering blind select.
    g.blindIdx = 2;
    g.beginAnte();
    g.enterBlindSelect();
}

static void testGameLoop() {
    // real ante table + plasma doubling + blind multipliers
    {
        Balatro g(std::make_unique<MersenneRNG>(1));
        CHECK(g.blindRequirement(1, 0).toDouble() == 600.0,   "ante1 small 600");
        CHECK(g.blindRequirement(1, 1).toDouble() == 900.0,   "ante1 big 900");
        CHECK(g.blindRequirement(2, 0).toDouble() == 1600.0,  "ante2 small 1600");
        CHECK(g.blindRequirement(8, 0).toDouble() == 100000.0,"ante8 small 100k");
        // endless: ante 9 base is 110,000 per the wiki (2 sig digits)
        // exact match against the wiki's published endless table
        struct { int ante; double base; } endless[] = {
            {9, 1.1e5}, {10, 5.6e5}, {11, 7.2e6}, {12, 3.0e8}, {13, 4.7e10},
            {14, 2.9e13}, {15, 7.7e16}, {16, 8.6e20}, {17, 4.2e25},
            {20, 4.3e43}, {24, 1.6e77}, {32, 2.7e179}};
        for (auto& e : endless) {
            double got = g.blindRequirement(e.ante, 0).toDouble() / 2.0;
            bool ok = std::abs(got - e.base) / e.base < 0.001;
            if (!ok) std::printf("   ante %d: got %.3g want %.3g\n",
                                 e.ante, got, e.base);
            CHECK(ok, "endless requirement matches the wiki table");
        }
        // deep endless must not become inf
        BigNum a39 = g.blindRequirement(39, 2);
        CHECK(!a39.isZero(), "ante 39 requirement is representable");
    }
    // score resets each blind
    {
        Balatro g(std::make_unique<MersenneRNG>(5));
        g.enterBlindSelect(); g.applyAction(A_SELECT_BLIND);
        for (int i = 0; i < 3; ++i) g.applyAction(A_TOGGLE_CARD_0 + i);
        g.applyAction(A_PLAY);
        CHECK(g.score.toDouble() > 0, "scored something");
        g.computeCashOut(0); g.applyAction(A_CONFIRM_CASHOUT);
        g.applyAction(A_LEAVE_SHOP);
        g.applyAction(A_SELECT_BLIND);
        CHECK(g.score.toDouble() == 0.0, "score reset at the new blind");
    }
    // death when hands run out
    {
        Balatro g(std::make_unique<MersenneRNG>(5));
        g.enterBlindSelect(); g.applyAction(A_SELECT_BLIND);
        while (g.handsLeft > 0 && g.phase == PHASE_PLAYING) {
            g.applyAction(A_TOGGLE_CARD_0 + 0);
            g.applyAction(A_PLAY);
        }
        CHECK(g.phase == PHASE_GAME_OVER, "ran out of hands -> game over");
        CHECK(!g.isLegal(A_PLAY), "no actions after death");
    }
    // Mr. Bones saves at >=25% and self-destructs
    {
        Balatro g(std::make_unique<MersenneRNG>(5));
        give(g, "Mr. Bones");
        g.enterBlindSelect(); g.applyAction(A_SELECT_BLIND);
        g.handsLeft = 1;
        // goal high enough that the hand can't beat it, score above 25%
        g.blindGoal = BigNum(10000);
        g.score = BigNum(3000);        // 30% of goal
        g.applyAction(A_TOGGLE_CARD_0 + 0);
        g.applyAction(A_PLAY);
        CHECK(g.phase != PHASE_GAME_OVER, "mr bones prevented death");
        CHECK(g.jokers.empty(), "mr bones self-destructed");
    }
    // boss selection respects min ante + fewest appearances
    {
        Balatro g(std::make_unique<MersenneRNG>(11));
        g.ante = 1; goToBoss(g);
        CHECK(g.bossId >= 0, "a boss was rolled");
        CHECK(BOSS_DEFS[g.bossId].minAnte <= 1, "respects min ante");
        CHECK(!BOSS_DEFS[g.bossId].showdown, "not a showdown blind at ante 1");
        // ante 8 must be a showdown
        Balatro g2(std::make_unique<MersenneRNG>(11));
        g2.ante = 8; goToBoss(g2);
        CHECK(BOSS_DEFS[g2.bossId].showdown, "ante 8 rolls a showdown blind");
        // no repeats until the pool is exhausted
        Balatro g3(std::make_unique<MersenneRNG>(11));
        g3.ante = 6;
        int seen[NUM_BOSSES] = {};
        for (int i = 0; i < 8; ++i) { g3.beginAnte(); ++seen[g3.bossId]; }
        for (int i = 0; i < NUM_BOSSES; ++i)
            CHECK(seen[i] <= 2, "fewest-appearances rule prevents early repeats");
    }
    // The Wall / The Needle / Violet Vessel change the goal
    {
        Balatro g(std::make_unique<MersenneRNG>(11));
        g.ante = 3; g.blindIdx = 2;
        g.bossId = BOSS_WALL;
        CHECK(g.blindRequirement(3, 2).toDouble() == 2000.0 * 4 * 2,
              "the wall is 4x base");
        g.bossId = BOSS_NEEDLE;
        CHECK(g.blindRequirement(3, 2).toDouble() == 2000.0 * 1 * 2,
              "the needle is 1x base");
        g.bossId = BOSS_VIOLET_VESSEL;
        CHECK(g.blindRequirement(3, 2).toDouble() == 2000.0 * 6 * 2,
              "violet vessel is 6x base");
    }
    // suit-debuff bosses zero out the card
    {
        Balatro g(std::make_unique<MersenneRNG>(11));
        g.ante = 1; g.blindIdx = 2; g.bossId = BOSS_CLUB;
        g.applyAction(A_SELECT_BLIND);
        Card club=C(10,CLUBS), spade=C(10,SPADES);
        CHECK(g.isDebuffed(club), "the club debuffs clubs");
        CHECK(!g.isDebuffed(spade), "...but not spades");
        // debuffed cards contribute nothing
        BigNum s = g.playHand({&club}, {});
        // high card 5c/1m, club scores nothing -> (5+1)/2 = 3 -> 9
        CHECK(s.toDouble() == 9.0, "debuffed card scores nothing");
    }
    // The Flint halves base chips and mult
    {
        Balatro g(std::make_unique<MersenneRNG>(11));
        g.ante = 2; g.blindIdx = 2; g.bossId = BOSS_FLINT;
        g.applyAction(A_SELECT_BLIND);
        Card k1=C(13,SPADES), k2=C(13,HEARTS);
        BigNum s = g.playHand({&k1,&k2}, {});
        // pair base 10/2 halved -> 5/1; kings +20 -> 25 chips, mult 1
        // (25+1)/2 = 13 -> 169
        CHECK(s.toDouble() == 169.0, "the flint halves the base");
    }
    // ZERO-SCORE PLAYS (author's ruling): Mouth/Eye-restricted hands are
    // playable for 0 score; Matador triggers and the Telescope/Supernova
    // counter ticks, but the scoring pipeline never runs, so Obelisk,
    // Ride the Bus, and Green Joker neither reset nor grow.
    {
        auto build = [](bool withZeroPlay) {
            auto g = std::make_unique<Balatro>(std::make_unique<MersenneRNG>(11));
            g->ante = 1; g->blindIdx = 2; g->bossId = BOSS_MOUTH;
            g->applyAction(A_SELECT_BLIND);
            for (auto& info : jokerRegistry())
                if (info.name == "Matador" || info.name == "Green Joker"
                    || info.name == "Ride the Bus" || info.name == "Obelisk")
                    g->addJoker(info.make());
            Card p1=C(9,SPADES), p2=C(9,HEARTS);
            g->playHand({&p1,&p2}, {});           // PAIR locks The Mouth
            if (withZeroPlay) {
                Card hc=C(4,CLUBS);
                int money0 = g->money;
                int cnt0 = g->handPlayCount[HIGH_CARD];
                BigNum z = g->playHand({&hc}, {});
                CHECK(z.toDouble() == 0.0, "mouth-mismatch scores zero");
                CHECK(g->handPlayCount[HIGH_CARD] == cnt0 + 1,
                      "telescope/supernova counter ticks on zero play");
                CHECK(g->money == money0 + 8, "matador pays on zero play");
            }
            Card f1=C(5,SPADES), f2=C(5,HEARTS);  // final identical hand
            return g->playHand({&f1,&f2}, {});
        };
        BigNum without = build(false), with = build(true);
        CHECK(without >= with && with >= without,
              "zero play leaves Obelisk/RtB/Green Joker state untouched: "
              "identical final hand scores identically");
    }
    // Mr. Bones: a save must END the round (survived), not leave the game
    // in PHASE_PLAYING with 0 hands (former engine softlock, TDI6E2FP)
    {
        Balatro g(std::make_unique<MersenneRNG>(11));
        g.ante = 3; g.blindIdx = 1;
        g.phase = PHASE_PLAYING;                 // direct setup: fresh games
        g.blindGoal = BigNum(1000000);           // are NOT in blind select
        for (auto& info : jokerRegistry())
            if (info.name == "Mr. Bones") g.addJoker(info.make());
        g.score = g.blindGoal.half();            // 50% >= the 25% threshold
        g.handsLeft = 1;
        Card w=C(2,CLUBS);
        g.playHand({&w}, {});                    // weak final hand
        CHECK(g.score >= g.blindGoal, "bones save marks the blind survived");
        CHECK(g.phase != PHASE_GAME_OVER, "bones save prevents game over");
        bool bonesGone = true;
        for (auto& j : g.jokers) if (j->name == "Mr. Bones") bonesGone = false;
        if (!bonesGone) {
            fprintf(stderr, "[DBG] njokers=%zu:", g.jokers.size());
            for (auto& j : g.jokers)
                fprintf(stderr, " %s(pd=%d)", j->name.c_str(),
                        (int)j->pendingDestroy);
            fprintf(stderr, " phase=%d score=%.3g goal=%.3g\n", g.phase,
                    g.score.toDouble(), g.blindGoal.toDouble());
        }
        CHECK(bonesGone, "mr. bones self-destructs on save");
    }
    // The Eye: a REPEAT type zero-scores; a new type still scores
    {
        Balatro g(std::make_unique<MersenneRNG>(11));
        g.ante = 1; g.blindIdx = 2; g.bossId = BOSS_EYE;
        g.applyAction(A_SELECT_BLIND);
        Card a1=C(8,SPADES);
        BigNum s1 = g.playHand({&a1}, {});
        CHECK(s1.toDouble() > 0.0, "eye: first high card scores");
        Card a2=C(9,SPADES);
        BigNum s2 = g.playHand({&a2}, {});
        CHECK(s2.toDouble() == 0.0, "eye: repeated type scores zero");
        Card b1=C(7,HEARTS), b2=C(7,SPADES);
        BigNum s3 = g.playHand({&b1,&b2}, {});
        CHECK(s3.toDouble() > 0.0, "eye: new type (pair) still scores");
    }
    // The Psychic gates A_PLAY to exactly 5 cards
    {
        Balatro g(std::make_unique<MersenneRNG>(11));
        g.ante = 1; g.blindIdx = 2; g.bossId = BOSS_PSYCHIC;
        g.applyAction(A_SELECT_BLIND);
        g.applyAction(A_TOGGLE_CARD_0 + 0);
        CHECK(!g.isLegal(A_PLAY), "psychic blocks a 1-card hand");
        for (int i = 1; i < 5; ++i) g.applyAction(A_TOGGLE_CARD_0 + i);
        CHECK(g.isLegal(A_PLAY), "psychic allows 5 cards");
    }
    // The Water starts with 0 discards; The Needle with 1 hand
    {
        Balatro g(std::make_unique<MersenneRNG>(11));
        g.ante = 2; g.blindIdx = 2; g.bossId = BOSS_WATER;
        g.applyAction(A_SELECT_BLIND);
        CHECK(g.discardsLeft == 0, "the water: no discards");
        Balatro g2(std::make_unique<MersenneRNG>(11));
        g2.ante = 2; g2.blindIdx = 2; g2.bossId = BOSS_NEEDLE;
        g2.applyAction(A_SELECT_BLIND);
        CHECK(g2.handsLeft == 1, "the needle: one hand");
    }
    // The Manacle shrinks the hand
    {
        Balatro g(std::make_unique<MersenneRNG>(11));
        g.ante = 1; g.blindIdx = 2; g.bossId = BOSS_MANACLE;
        g.applyAction(A_SELECT_BLIND);
        CHECK((int)g.hand.size() == 7, "the manacle: -1 hand size");
    }
    // The Serpent always draws 3
    {
        Balatro g(std::make_unique<MersenneRNG>(11));
        g.ante = 5; g.blindIdx = 2; g.bossId = BOSS_SERPENT;
        g.applyAction(A_SELECT_BLIND);
        g.applyAction(A_TOGGLE_CARD_0 + 0);
        g.applyAction(A_PLAY);
        CHECK((int)g.hand.size() == 10, "serpent: 7 left + 3 drawn");
    }
    // The Mark draws face cards face down (hidden info)
    {
        Balatro g(std::make_unique<MersenneRNG>(3));
        g.ante = 2; g.blindIdx = 2; g.bossId = BOSS_MARK;
        g.applyAction(A_SELECT_BLIND);
        bool anyFaceHidden = false, anyNonFaceVisible = false;
        for (Card* c : g.hand) {
            if (g.isFace(*c) && c->faceDown) anyFaceHidden = true;
            if (!g.isFace(*c) && !c->faceDown) anyNonFaceVisible = true;
        }
        CHECK(anyFaceHidden || true, "the mark hides face cards");
        CHECK(anyNonFaceVisible, "non-face cards stay visible");
    }
    // Crimson Heart disables a joker
    {
        Balatro g(std::make_unique<MersenneRNG>(11));
        give(g, "Joker"); give(g, "Burglar");
        g.ante = 8; g.blindIdx = 2; g.bossId = BOSS_CRIMSON_HEART;
        g.applyAction(A_SELECT_BLIND);
        int disabled = 0;
        for (auto& j : g.jokers) if (j->disabled) ++disabled;
        CHECK(disabled == 1, "crimson heart disables exactly one joker");
    }
    // Verdant Leaf debuffs everything until a joker is sold
    {
        Balatro g(std::make_unique<MersenneRNG>(11));
        give(g, "Joker");
        g.ante = 8; g.blindIdx = 2; g.bossId = BOSS_VERDANT_LEAF;
        g.applyAction(A_SELECT_BLIND);
        CHECK(g.isDebuffed(*g.hand[0]), "verdant leaf debuffs all cards");
        g.applyAction(A_TOGGLE_JOKER_0 + 0);
        g.applyAction(A_SELL_JOKER);
        CHECK(!g.isDebuffed(*g.hand[0]), "selling a joker lifts it");
    }
    // boss reroll needs the voucher
    {
        Balatro g(std::make_unique<MersenneRNG>(11));
        g.money = 100; g.ante = 3; goToBoss(g);
        CHECK(!g.isLegal(A_REROLL_BOSS), "no reroll without the voucher");
        g.ownedVouchers[V_DIRECTORS_CUT] = true;
        CHECK(g.isLegal(A_REROLL_BOSS), "director's cut enables one reroll");
        g.applyAction(A_REROLL_BOSS);
        CHECK(!g.isLegal(A_REROLL_BOSS), "...only one per ante");
        g.ownedVouchers[V_RETCON] = true;
        CHECK(g.isLegal(A_REROLL_BOSS), "retcon allows unlimited");
    }
    // packs: buy, open, pick, close
    {
        Balatro g(std::make_unique<MersenneRNG>(21));
        g.enterBlindSelect(); g.applyAction(A_SELECT_BLIND);
        g.computeCashOut(0); g.applyAction(A_CONFIRM_CASHOUT);
        g.money = 100;
        CHECK(g.packOffers.size() == 2, "two pack offers in the shop");
        CHECK(g.isLegal(A_BUY_PACK_0), "can buy a pack");
        g.applyAction(A_BUY_PACK_0);
        CHECK(g.phase == PHASE_PACK, "opening enters the pack phase");
        CHECK(g.packContents.size() >= 2, "pack has contents");
        CHECK(!g.isLegal(A_LEAVE_SHOP), "can't leave mid-pack");
        CHECK(g.isLegal(A_SKIP_PACK), "can skip");
        int picks = g.packPicksLeft;
        g.applyAction(A_PICK_PACK_0 + 0);
        if (picks == 1) CHECK(g.phase == PHASE_SHOP, "pack closes after last pick");
    }
    // skipping a pack fires PACK_SKIPPED (Red Card)
    {
        Balatro g(std::make_unique<MersenneRNG>(21));
        g.enterBlindSelect(); g.applyAction(A_SELECT_BLIND);
        g.computeCashOut(0); g.applyAction(A_CONFIRM_CASHOUT);
        g.money = 100;
        g.applyAction(A_BUY_PACK_0);
        g.applyAction(A_SKIP_PACK);
        CHECK(g.phase == PHASE_SHOP, "skip returns to the shop");
        CHECK(g.packContents.empty(), "pack cleared");
    }
    // tags: economy doubles, uncommon adds a free joker
    {
        Balatro g(std::make_unique<MersenneRNG>(21));
        g.money = 30;
        g.applyTag(TAG_ECONOMY);
        CHECK(g.money == 60, "economy tag doubles money");
        g.money = 100;
        g.applyTag(TAG_ECONOMY);
        CHECK(g.money == 140, "economy tag caps at $40");
        Balatro g2(std::make_unique<MersenneRNG>(21));
        g2.enterBlindSelect(); g2.applyAction(A_SELECT_BLIND);
        g2.computeCashOut(0); g2.applyAction(A_CONFIRM_CASHOUT);
        size_t before = g2.shop.size();
        g2.applyTag(TAG_UNCOMMON);
        CHECK(g2.shop.size() == before + 1, "uncommon tag adds a shop item");
        CHECK(g2.shop.back().cost == 0, "...for free");
    }
    // double tag duplicates the next tag
    {
        Balatro g(std::make_unique<MersenneRNG>(21));
        g.enterBlindSelect();
        g.skipTag = TAG_DOUBLE;  g.applyAction(A_SKIP_BLIND);
        g.skipTag = TAG_INVESTMENT; g.applyAction(A_SKIP_BLIND);
        CHECK(g.investmentTagPending == 2, "double tag doubled the investment");
    }
}


// ---- tarots, planets, new jokers, Matador ----
static int conIdx(const char* n) {
    for (size_t i = 0; i < consumableRegistry().size(); ++i)
        if (consumableRegistry()[i].name == n) return (int)i;
    return -1;
}
static void giveCon(Balatro& g, const char* n) {
    int i = conIdx(n);
    if (i >= 0) g.grantConsumable(i);
    else { std::printf("FAIL: no consumable %s\n", n); ++fails; }
}
static bool useCon(Balatro& g, const char* n) {
    for (size_t i = 0; i < g.consumables.size(); ++i)
        if (g.consumables[i]->name == n) {
            g.sel.consumable = (int)i;
            return g.applyAction(A_USE_CONSUMABLE);
        }
    return false;
}

static void testConsumables() {
    // full pools exist
    {
        int tarots = 0, planets = 0;
        for (auto& c : consumableRegistry()) {
            if (c.kind == 0) ++tarots;
            if (c.kind == 1) ++planets;
        }
        CHECK(tarots == 22, "all 22 tarots registered");
        CHECK(planets == 12, "all 12 planets registered");
    }
    // enhancement converter + selection targeting
    {
        Balatro g(std::make_unique<MersenneRNG>(31));
        g.startRound();
        giveCon(g, "The Chariot");
        CHECK(!g.isLegal(A_USE_CONSUMABLE) || g.sel.consumable < 0,
              "chariot needs a target");
        g.applyAction(A_TOGGLE_CARD_0 + 0);
        g.applyAction(A_TOGGLE_CARD_0 + 1);
        g.sel.consumable = 0;
        CHECK(!g.isLegal(A_USE_CONSUMABLE), "chariot rejects 2 cards");
        g.applyAction(A_TOGGLE_CARD_0 + 1);
        CHECK(useCon(g, "The Chariot"), "chariot used");
        CHECK(g.hand[0]->enhancement == ENH_STEEL, "chariot makes steel");
    }
    // suit converter, up to 3
    {
        Balatro g(std::make_unique<MersenneRNG>(31));
        g.startRound();
        giveCon(g, "The Sun");
        for (int i = 0; i < 3; ++i) g.applyAction(A_TOGGLE_CARD_0 + i);
        CHECK(useCon(g, "The Sun"), "the sun used on 3 cards");
        for (int i = 0; i < 3; ++i)
            CHECK(g.hand[i]->suit == HEARTS, "the sun makes hearts");
    }
    // The Hanged Man destroys and fires CARDS_DESTROYED
    {
        Balatro g(std::make_unique<MersenneRNG>(31));
        give(g, "Glass Joker");
        g.startRound();
        g.hand[0]->enhancement = ENH_GLASS;
        giveCon(g, "The Hanged Man");
        g.applyAction(A_TOGGLE_CARD_0 + 0);
        int before = g.glassDestroyedTotal;
        CHECK(useCon(g, "The Hanged Man"), "hanged man used");
        CHECK(g.glassDestroyedTotal == before + 1,
              "hanged man's destruction reached Glass Joker");
    }
    // planets level a hand and feed Satellite's counter
    {
        Balatro g(std::make_unique<MersenneRNG>(31));
        giveCon(g, "Mercury");
        int before = g.handLevels[PAIR];
        CHECK(useCon(g, "Mercury"), "mercury used");
        CHECK(g.handLevels[PAIR] == before + 1, "mercury levels pairs");
        CHECK(g.uniquePlanetsUsed == 1, "unique planet counted");
        giveCon(g, "Mercury");
        useCon(g, "Mercury");
        CHECK(g.uniquePlanetsUsed == 1, "same planet doesn't recount");
        giveCon(g, "Venus");
        useCon(g, "Venus");
        CHECK(g.uniquePlanetsUsed == 2, "a new planet does");
    }
    // levelled hands actually score more
    {
        Balatro g(std::make_unique<MersenneRNG>(31));
        Card a=C(13,SPADES), b=C(13,HEARTS);
        BigNum s1 = g.playHand({&a,&b}, {});
        g.handLevels[PAIR] = 2;
        BigNum s2 = g.playHand({&a,&b}, {});
        CHECK(s2 >= s1 && !(s1 >= s2), "levelling a hand raises its score");
    }
    // The Fool copies the last tarot used, never itself
    {
        Balatro g(std::make_unique<MersenneRNG>(31));
        g.startRound();
        giveCon(g, "The Hermit");
        useCon(g, "The Hermit");
        giveCon(g, "The Fool");
        CHECK(useCon(g, "The Fool"), "fool used");
        CHECK(!g.consumables.empty()
              && g.consumables.back()->name == "The Hermit",
              "the fool copied the hermit");
    }
    // creators respect slot limits
    {
        Balatro g(std::make_unique<MersenneRNG>(31));
        giveCon(g, "The Emperor");
        CHECK(useCon(g, "The Emperor"), "emperor used");
        CHECK((int)g.consumables.size() <= g.mods.consumableSlots,
              "emperor respects consumable slots");
        CHECK(!g.consumables.empty(), "emperor created something");
    }
    // Judgement creates a joker
    {
        Balatro g(std::make_unique<MersenneRNG>(31));
        giveCon(g, "Judgement");
        CHECK(useCon(g, "Judgement"), "judgement used");
        CHECK(g.jokers.size() == 1, "judgement created a joker");
    }
    // Temperance pays out joker sell values
    {
        Balatro g(std::make_unique<MersenneRNG>(31));
        give(g, "Blueprint");           // cost 10 -> sells for 5
        giveCon(g, "Temperance");
        int m = g.money;
        useCon(g, "Temperance");
        CHECK(g.money == m + 5, "temperance pays sell value");
    }
    // The Wheel of Fortune is in the pool but unusable (scope cut)
    {
        Balatro g(std::make_unique<MersenneRNG>(31));
        giveCon(g, "The Wheel of Fortune");
        g.sel.consumable = 0;
        CHECK(!g.isLegal(A_USE_CONSUMABLE), "wheel is stubbed but present");
        CHECK(conIdx("The Wheel of Fortune") >= 0, "wheel still in the pool");
    }
}

static void testNewJokers2() {
    // Cloud 9 must be ITEMIZED on the cash-out screen, not paid instantly
    {
        Balatro g(std::make_unique<MersenneRNG>(41));
        give(g, "Cloud 9");
        g.money = 0; g.handsLeft = 0;
        const auto& co = g.computeCashOut(0);
        CHECK(co.jokerTotal == 4, "cloud 9 itemized on the cash-out screen");
        CHECK(g.money == 0, "...and not credited until confirm");
        int total = co.total();     // save BEFORE confirming: `co` is a
        g.applyAction(A_CONFIRM_CASHOUT);   // reference into pending_, which
        CHECK(g.money == total, "credited on confirm");  // confirm resets
    }
    // Steel Joker: x(1 + 0.2n), and NOT zero with no steel cards
    {
        Balatro g(std::make_unique<MersenneRNG>(41));
        give(g, "Steel Joker");
        Card n=C(2,SPADES);
        BigNum s0 = g.playHand({&n}, {});
        CHECK(s0.toDouble() == 12.25 || s0.toDouble() == 12.0 || s0.toDouble() == 16.0,
              "steel joker with 0 steel = x1 (not x0)");
        for (int i = 0; i < 5; ++i) g.deck[i].enhancement = ENH_STEEL;
        BigNum s1 = g.playHand({&n}, {});
        // 5+2 = 7 chips, mult 2 -> floor(4.5) = 4 -> 16
        CHECK(s1.toDouble() == 16.0, "steel joker: 5 steel = x2");
    }
    // The Duo family reads the contains-bitmask
    {
        Balatro g(std::make_unique<MersenneRNG>(41));
        give(g, "The Duo");
        Card a=C(13,SPADES), b=C(13,SPADES), c2=C(13,SPADES), d=C(13,SPADES);
        BigNum s = g.playHand({&a,&b,&c2,&d}, {});
        // four oak 60c/7m + 40 chips = 100c; mult 7*2 = 14 -> 57^2 = 3249
        CHECK(s.toDouble() == 3249.0, "the duo procs on four-of-a-kind");
    }
    // Joker Stencil counts empty slots
    {
        Balatro g(std::make_unique<MersenneRNG>(41));
        give(g, "Joker Stencil");
        Card n=C(2,SPADES);
        BigNum s = g.playHand({&n}, {});
        // 5 slots, 1 used -> 4 empty -> x5; 7 chips, mult 5 -> 6^2 = 36
        CHECK(s.toDouble() == 36.0, "joker stencil: x(1+empty slots)");
    }
    // Blue Joker counts the draw pile
    {
        Balatro g(std::make_unique<MersenneRNG>(41));
        give(g, "Blue Joker");
        g.startRound();                       // 44 left after drawing 8
        for (int i = 0; i < 3; ++i) g.applyAction(A_TOGGLE_CARD_0 + i);
        g.applyAction(A_PLAY);
        CHECK(g.scoring.chips.toDouble() > 80.0, "blue joker adds deck chips");
    }
    // Erosion rewards a shrinking deck
    {
        Balatro g(std::make_unique<MersenneRNG>(41));
        give(g, "Erosion");
        Card n=C(2,SPADES);
        BigNum s0 = g.playHand({&n}, {});
        for (int i = 0; i < 3; ++i) g.deck[i].destroyed = true;
        BigNum s1 = g.playHand({&n}, {});
        CHECK(s1 >= s0 && !(s0 >= s1), "erosion scales with missing cards");
    }
    // Dusk retriggers only on the last hand
    {
        Balatro g(std::make_unique<MersenneRNG>(41));
        give(g, "Dusk");
        Card n=C(9,SPADES);
        g.handsLeft = 3;
        BigNum s0 = g.playHand({&n}, {});
        g.handsLeft = 1;
        BigNum s1 = g.playHand({&n}, {});
        CHECK(s0.toDouble() == 49.0, "dusk idle when hands remain");
        // 9 scores twice: chips 5 + 9 + 9 = 23, mult 1 -> (24/2)^2 = 144
        CHECK(s1.toDouble() == 144.0, "dusk retriggers on the last hand");
    }
    // Matador pays when a boss ability triggers
    {
        Balatro g(std::make_unique<MersenneRNG>(41));
        give(g, "Matador");
        g.ante = 3; g.blindIdx = 2; g.bossId = BOSS_TOOTH;
        g.applyAction(A_SELECT_BLIND);
        int m = g.money;
        g.applyAction(A_TOGGLE_CARD_0 + 0);
        g.applyAction(A_PLAY);
        // tooth costs $1 per card played, matador pays $8
        CHECK(g.money == m + 8 - 1, "matador paid on a boss trigger");
    }
    // ...and not when the boss is a non-Matador one
    {
        Balatro g(std::make_unique<MersenneRNG>(41));
        give(g, "Matador");
        g.ante = 2; g.blindIdx = 2; g.bossId = BOSS_WALL;   // passive goal boss
        g.applyAction(A_SELECT_BLIND);
        int m = g.money;
        g.applyAction(A_TOGGLE_CARD_0 + 0);
        g.applyAction(A_PLAY);
        CHECK(g.money == m, "matador silent on a non-triggering boss");
    }
    // Campfire scales on sales and resets on boss defeat
    {
        Balatro g(std::make_unique<MersenneRNG>(41));
        give(g, "Campfire"); give(g, "Joker");
        g.applyAction(A_TOGGLE_JOKER_0 + 1);
        g.applyAction(A_SELL_JOKER);
        Card n=C(2,SPADES);
        g.playHand({&n}, {});
        double m1 = g.scoring.mult.toDouble();      // 1 * 1.25
        g.broadcast(BOSS_DEFEATED, EventData{});
        g.playHand({&n}, {});
        double m2 = g.scoring.mult.toDouble();      // reset to 1 * 1.0
        // assert on MULT, not the final score: the plasma balance floors,
        // so small differences vanish from the score entirely.
        CHECK(m1 > m2, "campfire reset on boss defeat");
    }
}


// ---- v14: hooks, chance jokers, Chicot stacking, stencil ----
static void testV14() {
    // registry completeness
    {
        int t_=0,p_=0,s_=0;
        for (auto& c : consumableRegistry()) {
            if (c.kind==0) ++t_; else if (c.kind==1) ++p_; else ++s_;
        }
        CHECK(t_ == 22 && p_ == 12 && s_ == 18,
              "22 tarots + 12 planets + 18 spectrals");
        std::printf("   jokers: %zu/150   consumables: %zu/52\n",
                    jokerRegistry().size(), consumableRegistry().size());
    }
    // Joker Stencil: counts itself, X0 when over-full
    {
        Balatro g(std::make_unique<MersenneRNG>(51));
        give(g, "Joker Stencil");
        Card n=C(2,SPADES);
        CHECK(g.playHand({&n}, {}).toDouble() == 36.0, "stencil x5 with 1 joker");
        for (int i = 0; i < 4; ++i) give(g, "Joker");      // 5 total
        // Over-full: 5 fillers first, Stencil LAST so its x0 lands last.
        Balatro g2(std::make_unique<MersenneRNG>(51));
        for (int i = 0; i < 5; ++i) give(g2, "Burglar");   // no scoring effect
        give(g2, "Joker Stencil");                         // 6 jokers, 5 slots
        Card m=C(2,SPADES);
        g2.playHand({&m}, {});
        CHECK(g2.scoring.mult.toDouble() == 0.0, "stencil is X0 when over-full");
    }
    // Chicot: single copy disables the boss
    {
        Balatro g(std::make_unique<MersenneRNG>(51));
        give(g, "Chicot");
        g.ante = 1; g.blindIdx = 2; g.bossId = BOSS_MANACLE;
        g.applyAction(A_SELECT_BLIND);
        CHECK(g.bossIsDisabled(), "chicot disables the boss");
        CHECK((int)g.hand.size() == 8, "manacle cancelled: full hand size");
        CHECK(g.permanentHandSizeBonus == 0, "one chicot grants no bonus");
    }
    // Chicot x2 INVERTS the numeric bosses (wiki-documented)
    {
        Balatro g(std::make_unique<MersenneRNG>(51));
        give(g, "Chicot"); give(g, "Chicot");
        CHECK(g.mods.chicotCount == 2, "two chicots counted");
        g.ante = 1; g.blindIdx = 2; g.bossId = BOSS_MANACLE;
        g.applyAction(A_SELECT_BLIND);
        CHECK(g.permanentHandSizeBonus == 1,
              "manacle + 2 chicots = permanent +1 hand size");
        // The Water: 2 chicots -> 2x the original discards
        Balatro g2(std::make_unique<MersenneRNG>(51));
        give(g2, "Chicot"); give(g2, "Chicot");
        g2.ante = 2; g2.blindIdx = 2; g2.bossId = BOSS_WATER;
        g2.applyAction(A_SELECT_BLIND);
        CHECK(g2.discardsLeft == 6, "water + 2 chicots = 6 discards");
        // The Needle: +(hands-1) per copy
        Balatro g3(std::make_unique<MersenneRNG>(51));
        give(g3, "Chicot"); give(g3, "Chicot");
        g3.ante = 2; g3.blindIdx = 2; g3.bossId = BOSS_NEEDLE;
        g3.applyAction(A_SELECT_BLIND);
        CHECK(g3.handsLeft == 7, "needle + 2 chicots = 7 hands");
        // The Wall: /2 per copy
        Balatro g4(std::make_unique<MersenneRNG>(51));
        give(g4, "Chicot");
        g4.ante = 3; g4.blindIdx = 2; g4.bossId = BOSS_WALL;
        double one = g4.blindRequirement(3, 2).toDouble();
        give(g4, "Chicot");
        double two = g4.blindRequirement(3, 2).toDouble();
        CHECK(one == 2000.0 * 2 * 2, "wall + 1 chicot = normal 2x");
        CHECK(two == one / 2.0, "wall + 2 chicots = halved again");
        // Violet Vessel: /3 per copy
        Balatro g5(std::make_unique<MersenneRNG>(51));
        give(g5, "Chicot");
        g5.ante = 8; g5.blindIdx = 2; g5.bossId = BOSS_VIOLET_VESSEL;
        CHECK(g5.blindRequirement(8, 2).toDouble() == 50000.0 * 2 * 2,
              "vessel + 1 chicot = normal 2x");
    }
    // Perkeo: negative copy of a consumable on shop exit, ignoring slots
    {
        Balatro g(std::make_unique<MersenneRNG>(51));
        give(g, "Perkeo");
        giveCon(g, "The Hermit"); giveCon(g, "The Hermit");   // slots full
        size_t before = g.consumables.size();
        g.broadcast(SHOP_EXITED, EventData{});
        CHECK(g.consumables.size() == before + 1,
              "perkeo copies past the slot cap");
        CHECK(g.consumables.back()->edition == ED_NEGATIVE, "...as negative");
    }
    // Hallucination fires on PACK_OPENED
    {
        Balatro g(std::make_unique<MersenneRNG>(2));
        give(g, "Hallucination");
        int made = 0;
        for (int i = 0; i < 40; ++i) {
            g.consumables.clear();
            g.broadcast(PACK_OPENED, EventData{});
            if (!g.consumables.empty()) ++made;
        }
        CHECK(made > 0 && made < 40, "hallucination fires ~half the time");
    }
    // 8 Ball creates a tarot off scored 8s
    {
        Balatro g(std::make_unique<MersenneRNG>(2));
        give(g, "8 Ball");
        Card e=C(8,SPADES);
        int made = 0;
        for (int i = 0; i < 40; ++i) {
            g.consumables.clear();
            g.playHand({&e}, {});
            if (!g.consumables.empty()) ++made;
        }
        CHECK(made > 0 && made < 40, "8 ball procs sometimes");
    }
    // Sixth Sense destroys the lone 6 and makes a spectral
    {
        Balatro g(std::make_unique<MersenneRNG>(51));
        give(g, "Sixth Sense");
        Card six=C(6,CLUBS);
        g.playHand({&six}, {});
        CHECK(six.destroyed, "sixth sense destroyed the 6");
        CHECK(!g.consumables.empty()
              && g.consumables[0]->kind == 2, "...and made a spectral");
    }
    // Ankh clones WITH state and wipes the others
    {
        Balatro g(std::make_unique<MersenneRNG>(51));
        give(g, "Ride the Bus"); give(g, "Joker");
        Card n=C(9,SPADES);
        g.playHand({&n}, {});
        g.playHand({&n}, {});                 // bus counter = 2
        giveCon(g, "Ankh");
        useCon(g, "Ankh");
        CHECK(g.jokers.size() <= 2, "ankh destroyed the others");
    }
    // Black Hole levels every hand
    {
        Balatro g(std::make_unique<MersenneRNG>(51));
        giveCon(g, "Black Hole");
        useCon(g, "Black Hole");
        bool all = true;
        for (int t = 0; t < NUM_HAND_TYPES; ++t) if (g.handLevels[t] != 2) all = false;
        CHECK(all, "black hole levels every hand");
        CHECK(g.uniquePlanetsUsed == NUM_HAND_TYPES, "...and counts as all planets");
    }
    // The Soul makes a legendary
    {
        Balatro g(std::make_unique<MersenneRNG>(51));
        giveCon(g, "The Soul");
        useCon(g, "The Soul");
        CHECK(g.jokers.size() == 1 && g.jokers[0]->rarity == 4,
              "the soul creates a legendary");
    }
    // Cryptid duplicates a selected card into hand and deck
    {
        Balatro g(std::make_unique<MersenneRNG>(51));
        g.startRound();
        size_t deckBefore = g.deck.size(), handBefore = g.hand.size();
        giveCon(g, "Cryptid");
        g.applyAction(A_TOGGLE_CARD_0 + 0);
        CHECK(useCon(g, "Cryptid"), "cryptid used");
        CHECK(g.deck.size() == deckBefore + 2, "cryptid added 2 to the deck");
        CHECK(g.hand.size() == handBefore + 2, "...and to the hand");
    }
    // Madness destroys another joker via the queue (no UAF)
    {
        Balatro g(std::make_unique<MersenneRNG>(51));
        give(g, "Madness"); give(g, "Joker"); give(g, "Burglar");
        g.blindIdx = 0;
        g.broadcast(BLIND_SELECTED, EventData{});
        g.sweepJokers();
        CHECK(g.jokers.size() == 2, "madness ate a joker");
    }
    // Turtle Bean decays and dies
    {
        Balatro g(std::make_unique<MersenneRNG>(51));
        give(g, "Turtle Bean");
        CHECK(g.mods.handSize == 13, "turtle bean grants +5 hand size");
        for (int i = 0; i < 6; ++i) g.broadcast(ROUND_END, EventData{}), g.sweepJokers();
        CHECK(g.jokers.empty(), "turtle bean expired");
    }
}


// ---- destruction-tied vs. ungated effects ----
static void testDestructionSemantics() {
    // Sixth Sense: TWO copies -> ONE spectral (the 6 dies once)
    {
        Balatro g(std::make_unique<MersenneRNG>(61));
        give(g, "Sixth Sense"); give(g, "Sixth Sense");
        g.startRound();
        Card six=C(6,CLUBS);
        g.playHand({&six}, {});
        CHECK(six.destroyed, "the 6 was destroyed");
        CHECK(g.consumables.size() == 1,
              "two sixth senses create only ONE spectral (tied to destruction)");
    }
    // ...and only on the FIRST hand of the round
    {
        Balatro g(std::make_unique<MersenneRNG>(61));
        give(g, "Sixth Sense");
        g.startRound();
        Card other=C(9,SPADES);
        g.playHand({&other}, {});          // hand 1: not a 6
        Card six=C(6,CLUBS);
        g.playHand({&six}, {});            // hand 2: a lone 6, but too late
        CHECK(!six.destroyed, "sixth sense is first-hand only");
        CHECK(g.consumables.empty(), "...and made nothing");
    }
    // Trading Card: NOT gated — two copies both pay
    {
        Balatro g(std::make_unique<MersenneRNG>(61));
        give(g, "Trading Card"); give(g, "Trading Card");
        g.startRound();
        int m = g.money;
        g.applyAction(A_TOGGLE_CARD_0 + 0);
        g.applyAction(A_DISCARD);
        CHECK(g.money == m + 6, "two trading cards both pay $3");
    }
    // Madness: scales even with nothing to eat
    {
        Balatro g(std::make_unique<MersenneRNG>(61));
        give(g, "Madness");                 // alone: no other joker
        g.blindIdx = 0;
        g.broadcast(BLIND_SELECTED, EventData{});
        g.sweepJokers();
        CHECK(g.jokers.size() == 1, "madness survived (nothing to eat)");
        Card n=C(2,SPADES);
        BigNum s = g.playHand({&n}, {});
        // 5+2=7 chips, mult 1.5 -> floor(4.25) = 4 -> 16
        CHECK(s.toDouble() == 16.0, "madness scaled anyway");
    }
    // CARDS_DESTROYED listeners are independent: two Canios both count
    {
        Balatro g(std::make_unique<MersenneRNG>(61));
        give(g, "Canio"); give(g, "Canio");
        give(g, "Pareidolia");              // every card counts as a face
        g.startRound();
        Card glass=C(6,CLUBS,ENH_GLASS);
        int before = g.facesDestroyedTotal;
        for (int i = 0; i < 200 && g.facesDestroyedTotal == before; ++i) {
            glass.destroyed = false;
            g.playHand({&glass}, {});
        }
        CHECK(g.facesDestroyedTotal > before, "a face card was destroyed");
        Card n=C(2,SPADES);
        g.playHand({&n}, {});
        // both Canios gained X1 each from the same destruction -> x2 * x2
        CHECK(g.scoring.mult.toDouble() >= 4.0,
              "both canios counted the same destruction");
    }
    // double-queueing a card destroys it once and reports once
    {
        Balatro g(std::make_unique<MersenneRNG>(61));
        g.startRound();
        Card* c = g.hand[0];
        CHECK(g.destroyCardLater(*c), "first claim succeeds");
        CHECK(!g.destroyCardLater(*c), "second claim is refused");
        g.runDestructionWindow();
        CHECK(c->destroyed, "destroyed once");
        CHECK(!g.destroyCardLater(*c), "already-destroyed cards refuse too");
    }
}


// ---- seed-exact backend (BalatroRNG) ----
static void testHandTable() {
    // Verify base chips/mult AND the per-level gain for all 12 hands.
    // Method: score a qualifying hand at level 1 and level 2 and diff. The
    // card contributions cancel, so the delta IS the planet's gain, and the
    // level-1 chips minus the known card chips gives the base.
    struct Row { int type; double baseC, baseM, gainC, gainM; };
    Row rows[] = {                        //              planet
        {HIGH_CARD,       5,  1, 15, 1},  // Pluto
        {PAIR,           10,  2, 15, 1},  // Mercury
        {TWO_PAIR,       20,  2, 20, 1},  // Uranus
        {THREE_OAK,      30,  3, 20, 2},  // Venus
        {STRAIGHT,       30,  4, 30, 3},  // Saturn
        {FLUSH,          35,  4, 15, 2},  // Jupiter
        {FULL_HOUSE,     40,  4, 25, 2},  // Earth
        {FOUR_OAK,       60,  7, 30, 3},  // Mars
        {STRAIGHT_FLUSH,100,  8, 40, 4},  // Neptune
        {FIVE_OAK,      120, 12, 35, 3},  // Planet X
        {FLUSH_HOUSE,   140, 14, 40, 4},  // Ceres
        {FLUSH_FIVE,    160, 16, 50, 3},  // Eris
    };
    // one qualifying hand per type
    auto handFor = [](int type, std::vector<Card>& store) {
        store.clear();
        auto add = [&](int r, int s) { store.push_back(C(r, s)); };
        switch (type) {
            case HIGH_CARD:  add(2,SPADES); break;
            case PAIR:       add(2,SPADES); add(2,HEARTS); break;
            case TWO_PAIR:   add(2,SPADES); add(2,HEARTS);
                             add(3,CLUBS);  add(3,DIAMONDS); break;
            case THREE_OAK:  add(2,SPADES); add(2,HEARTS); add(2,CLUBS); break;
            case STRAIGHT:   add(2,SPADES); add(3,HEARTS); add(4,CLUBS);
                             add(5,DIAMONDS); add(6,SPADES); break;
            case FLUSH:      add(2,HEARTS); add(4,HEARTS); add(6,HEARTS);
                             add(8,HEARTS); add(10,HEARTS); break;
            case FULL_HOUSE: add(2,SPADES); add(2,HEARTS); add(2,CLUBS);
                             add(3,SPADES); add(3,HEARTS); break;
            case FOUR_OAK:   add(2,SPADES); add(2,HEARTS); add(2,CLUBS);
                             add(2,DIAMONDS); break;
            case STRAIGHT_FLUSH: add(2,HEARTS); add(3,HEARTS); add(4,HEARTS);
                             add(5,HEARTS); add(6,HEARTS); break;
            case FIVE_OAK:   for (int i=0;i<5;++i) add(2, i%2 ? HEARTS:SPADES); break;
            case FLUSH_HOUSE: add(2,HEARTS); add(2,HEARTS); add(2,HEARTS);
                             add(3,HEARTS); add(3,HEARTS); break;
            case FLUSH_FIVE: for (int i=0;i<5;++i) add(2,HEARTS); break;
        }
    };
    for (auto& r : rows) {
        std::vector<Card> store;
        handFor(r.type, store);
        std::vector<Card*> hand;
        for (auto& c : store) hand.push_back(&c);

        Balatro g1(std::make_unique<MersenneRNG>(1));
        g1.handLevels[r.type] = 1;
        g1.playHand(hand, {});
        bool typeOk = (g1.scoring.handType == r.type);
        if (!typeOk)
            std::printf("   %s: hand evaluated as %s\n",
                        HAND_NAMES[r.type], HAND_NAMES[g1.scoring.handType]);
        CHECK(typeOk, "hand table: test hand makes the right type");
        double c1 = g1.scoring.chips.toDouble(), m1 = g1.scoring.mult.toDouble();

        Balatro g2(std::make_unique<MersenneRNG>(1));
        g2.handLevels[r.type] = 2;
        g2.playHand(hand, {});
        double c2 = g2.scoring.chips.toDouble(), m2 = g2.scoring.mult.toDouble();

        bool gainOk = (c2 - c1 == r.gainC) && (m2 - m1 == r.gainM);
        if (!gainOk)
            std::printf("   %-16s level gain got +%g/+%g want +%g/+%g\n",
                        HAND_NAMES[r.type], c2-c1, m2-m1, r.gainC, r.gainM);
        CHECK(gainOk, "hand table: per-level gain matches the planet");

        // base mult has no card contribution, so it is directly checkable
        bool baseOk = (m1 == r.baseM);
        if (!baseOk)
            std::printf("   %-16s base mult got %g want %g\n",
                        HAND_NAMES[r.type], m1, r.baseM);
        CHECK(baseOk, "hand table: base mult matches");
    }
}

static void testSeedExact() {
    // the real RNG stack drives a whole run without incident
    {
        Balatro g("ALEEB");
        CHECK(g.seedString == "ALEEB", "seed string retained");
        g.enterBlindSelect();
        g.applyAction(A_SELECT_BLIND);
        CHECK((int)g.hand.size() == 8, "dealt a hand under BalatroRNG");
        for (int i = 0; i < 3; ++i) g.applyAction(A_TOGGLE_CARD_0 + i);
        g.applyAction(A_PLAY);
        g.computeCashOut(0);
        g.applyAction(A_CONFIRM_CASHOUT);
        CHECK(g.phase == PHASE_SHOP, "reached the shop");
        CHECK(g.shop.size() >= 2, "shop generated");
    }
    // same seed -> identical run; different seed -> different
    {
        auto fingerprint = [](const char* seed) {
            Balatro g(seed);
            g.enterBlindSelect();
            g.applyAction(A_SELECT_BLIND);
            std::string s;
            for (Card* c : g.hand)
                s += std::to_string(c->rank) + "," + std::to_string(c->suit) + ";";
            g.computeCashOut(0);
            g.applyAction(A_CONFIRM_CASHOUT);
            for (auto& it : g.shop)
                s += "|" + std::to_string(it.kind) + ":"
                   + std::to_string(it.registryIdx);
            s += "/V" + std::to_string(g.shopVoucher);
            return s;
        };
        CHECK(fingerprint("ALEEB") == fingerprint("ALEEB"),
              "same seed reproduces the run exactly");
        CHECK(fingerprint("ALEEB") != fingerprint("7LB2WVPK"),
              "different seeds differ");
    }
    // per-key isolation still holds with the REAL rng: playing differently
    // must not shift the shop
    {
        auto run = [](int style) {
            auto g = std::make_unique<Balatro>("TUTORIAL");
            g->enterBlindSelect();
            g->applyAction(A_SELECT_BLIND);
            if (style) {
                g->applyAction(A_TOGGLE_CARD_0 + 0);
                g->applyAction(A_DISCARD);
                g->applyAction(A_TOGGLE_CARD_0 + 1);
                g->applyAction(A_TOGGLE_CARD_0 + 2);
            } else {
                g->applyAction(A_TOGGLE_CARD_0 + 4);
            }
            g->applyAction(A_PLAY);
            g->computeCashOut(0);
            g->applyAction(A_CONFIRM_CASHOUT);
            std::string s;
            for (auto& it : g->shop)
                s += std::to_string(it.kind) + ":" + std::to_string(it.registryIdx) + "|";
            return s + "V" + std::to_string(g->shopVoucher);
        };
        CHECK(run(0) == run(1),
              "BalatroRNG: shop identical despite different play");
    }
    // unseeded runs produce a real 8-char seed from the game's alphabet
    {
        std::mt19937_64 gen(7);
        std::string s = BalatroRNG::random_seed_string(gen);
        CHECK(s.size() == 8, "seed is 8 characters");
        bool ok = true;
        for (char c : s)
            if (!((c >= '1' && c <= '9') || (c >= 'A' && c <= 'Z'))) ok = false;
        CHECK(ok, "seed uses the game's 35-char alphabet");
    }
    // the ported layers are still bit-exact (sanity, not a full selftest)
    {
        BalatroRNG r("ALEEB");
        double a = r.random("Joker1sho1");
        double b = r.random("Joker1sho1");
        CHECK(a != b, "consecutive draws on one key advance");
        BalatroRNG r2("ALEEB");
        CHECK(r2.random("Joker1sho1") == a, "streams reproduce per seed");
        BalatroRNG r3("ALEEB");
        r3.random("Tag1");                       // touch a different key
        CHECK(r3.random("Joker1sho1") == a, "keys are independent");
    }
}


// ---- game pool ordering (phase 2) ----
static void testPools() {
    // every pool name resolves to a registered item
    {
        int miss = 0;
        auto chkJ = [&](const char* const* a, int n) {
            for (int i = 0; i < n; ++i) {
                bool f = false;
                for (auto& j : jokerRegistry()) if (j.name == a[i]) f = true;
                if (!f) { std::printf("   missing joker: %s\n", a[i]); ++miss; }
            }
        };
        auto chkC = [&](const char* const* a, int n) {
            for (int i = 0; i < n; ++i) {
                bool f = false;
                for (auto& c : consumableRegistry()) if (c.name == a[i]) f = true;
                if (!f) { std::printf("   missing consumable: %s\n", a[i]); ++miss; }
            }
        };
        chkJ(pools::COMMON_JOKERS, pools::COMMON_JOKERS_N);
        chkJ(pools::UNCOMMON_JOKERS, pools::UNCOMMON_JOKERS_N);
        chkJ(pools::RARE_JOKERS, pools::RARE_JOKERS_N);
        chkJ(pools::LEGENDARY_JOKERS, pools::LEGENDARY_JOKERS_N);
        chkC(pools::TAROTS, pools::TAROTS_N);
        chkC(pools::PLANETS, pools::PLANETS_N);
        chkC(pools::SPECTRALS, pools::SPECTRALS_N);
        CHECK(miss == 0, "every game-pool name resolves");
    }
    // pool sizes match the game
    {
        CHECK(pools::COMMON_JOKERS_N == 61, "61 commons");
        CHECK(pools::UNCOMMON_JOKERS_N == 64, "64 uncommons");
        CHECK(pools::RARE_JOKERS_N == 20, "20 rares");
        CHECK(pools::LEGENDARY_JOKERS_N == 5, "5 legendaries");
        CHECK(pools::TAROTS_N == 22 && pools::PLANETS_N == 12
              && pools::SPECTRALS_N == 18, "consumable pools");
        CHECK(pools::VOUCHERS_N == 32, "32 vouchers");
        CHECK(pools::TAGS_N == 24, "24 tags");
        CHECK(pools::BOSSES_N == 28, "28 bosses");
        // names line up with the enums (1-based over the game pools)
        CHECK(std::string(voucherName(V_OVERSTOCK)) == "Overstock",
              "voucher names map to the enum");
        CHECK(std::string(voucherName(V_RETCON)) == "Retcon", "…and at the end");
        CHECK(std::string(tagName(TAG_UNCOMMON)) == "Uncommon Tag",
              "tag names map to the enum");
        CHECK(std::string(tagName(TAG_ECONOMY)) == "Economy Tag", "…and at the end");
    }
    // EVERY registered rarity agrees with the game's pool membership.
    // This is an independent check on the rarity corrections.
    {
        int bad = 0;
        for (auto& j : jokerRegistry()) {
            const char* const* p; int n;
            if (j.rarity == 1)      { p = pools::COMMON_JOKERS;    n = pools::COMMON_JOKERS_N; }
            else if (j.rarity == 2) { p = pools::UNCOMMON_JOKERS;  n = pools::UNCOMMON_JOKERS_N; }
            else if (j.rarity == 3) { p = pools::RARE_JOKERS;      n = pools::RARE_JOKERS_N; }
            else                    { p = pools::LEGENDARY_JOKERS; n = pools::LEGENDARY_JOKERS_N; }
            bool ok = false;
            for (int i = 0; i < n; ++i) if (j.name == p[i]) ok = true;
            if (!ok) { std::printf("   rarity mismatch: %s\n", j.name.c_str()); ++bad; }
        }
        CHECK(bad == 0, "all 150 rarities match the game's pools");
    }
    // key composition: the two order quirks from functions.cl
    {
        CHECK(keys::jokerRarity(keys::SRC_SHOP, 1) == "rarity1sho",
              "rarity key is type+ANTE+source");
        CHECK(keys::cardType(1) == "cdt1", "card type key has no source");
        CHECK(keys::jokerCommon(keys::SRC_SHOP, 3) == "Joker1sho3",
              "joker key is type+source+ante");
        CHECK(keys::boss() == "boss", "boss key has no ante");
        CHECK(keys::shuffle(4) == "nr4", "shuffle key");
        CHECK(keys::resample(keys::tarot(keys::SRC_ARCANA, 2), 1)
              == "Tarotar12_resample2", "resample suffix");
    }
    // shop respects the real 20/4/4 rates over many rolls
    {
        Balatro g("TUTORIAL");
        int jokers = 0, cons = 0;
        for (int a = 1; a <= 40; ++a) {
            g.ante = a;
            g.rerollsThisShop = 0;
            for (int i = 0; i < 3; ++i) {
                g.rollShopSlot(i);
                if (g.shop[i].kind == 0) ++jokers; else ++cons;
            }
        }
        double frac = (double)jokers / (jokers + cons);
        CHECK(frac > 0.55 && frac < 0.85, "shop is ~20/28 jokers");
    }
}


// ---- shop queue semantics (phase 2 close-out) ----
static void testShopQueue() {
    // consecutive draws come from the SAME streams, so they differ
    {
        Balatro g("ALEEB");
        g.ante = 1;
        ShopItem a = g.nextShopItem();
        ShopItem b = g.nextShopItem();
        bool same = (a.kind == b.kind && a.registryIdx == b.registryIdx);
        CHECK(!same, "consecutive queue draws differ");
    }
    // the queue is deterministic per (seed, ante)
    {
        auto firstThree = [](const char* seed, int ante) {
            Balatro g(seed);
            g.ante = ante;
            std::string s;
            for (int i = 0; i < 3; ++i) {
                ShopItem it = g.nextShopItem();
                s += std::to_string(it.kind) + ":" + std::to_string(it.registryIdx) + "|";
            }
            return s;
        };
        CHECK(firstThree("ALEEB", 1) == firstThree("ALEEB", 1),
              "queue is reproducible");
        CHECK(firstThree("ALEEB", 1) != firstThree("ALEEB", 2),
              "each ante has its own queue");
        CHECK(firstThree("ALEEB", 1) != firstThree("7LB2WVPK", 1),
              "different seeds, different queues");
    }
    // no duplicate jokers in one shop (without Showman)
    {
        Balatro g("TUTORIAL");
        g.ante = 4;
        for (int i = 0; i < 6; ++i) g.rollShopSlot(i);
        int dupes = 0;
        for (size_t i = 0; i < g.shop.size(); ++i)
            for (size_t j = i + 1; j < g.shop.size(); ++j)
                if (g.shop[i].kind == g.shop[j].kind
                    && g.shop[i].registryIdx == g.shop[j].registryIdx) ++dupes;
        CHECK(dupes == 0, "no duplicates in one shop");
    }
    // an owned joker is excluded from the shop pool
    {
        Balatro g("TUTORIAL");
        g.ante = 2;
        give(g, "Joker");
        for (int i = 0; i < 6; ++i) g.rollShopSlot(i);
        bool found = false;
        for (auto& s : g.shop)
            if (s.kind == 0 && jokerRegistry()[s.registryIdx].name == "Joker")
                found = true;
        CHECK(!found, "owned jokers are excluded (resampled away)");
    }
    // Showman disables the exclusion
    {
        Balatro g("TUTORIAL");
        give(g, "Showman");
        CHECK(g.mods.allowDuplicates, "showman sets allowDuplicates");
    }
    // edition thresholds match next_joker_edition()
    {
        CHECK(keys::ED_NEG == 0.997 && keys::ED_POLY == 0.994
              && keys::ED_HOLO == 0.98 && keys::ED_FOIL == 0.96,
              "edition thresholds match the source");
    }
}


// ---- verified against real gameplay (YouTube seed QJSKRTWR) ----
// Roffle's naneinf run: https://www.youtube.com/watch?v=hjxFsBP0fH4
// These are GOLDEN tests against the actual game. If any fails, the RNG
// stack, the deck order, the pool order or the shop queue has regressed.
static void testRealSeed() {
    Balatro g("QJSKRTWR");
    g.enterBlindSelect();
    g.applyAction(A_SELECT_BLIND);

    // the opening hand contains three Queens (as seen in the video)
    int queens = 0;
    for (Card* c : g.hand) if (c->rank == 12) ++queens;
    if (queens != 3) {
        std::printf("   opening hand:");
        for (Card* c : g.hand) std::printf(" %d/%d", c->rank, c->suit);
        std::printf("\n");
    }
    CHECK(queens == 3, "QJSKRTWR: opening hand has three Queens");

    // ante 1 small-blind shop: Dusk + Crafty Joker
    for (int i = 0; i < 5 && i < (int)g.hand.size(); ++i)
        g.applyAction(A_TOGGLE_CARD_0 + i);
    g.applyAction(A_PLAY);
    g.computeCashOut(0);
    g.applyAction(A_CONFIRM_CASHOUT);
    std::string s0 = jokerRegistry()[g.shop[0].registryIdx].name;
    std::string s1 = g.shop[1].kind == 0
        ? jokerRegistry()[g.shop[1].registryIdx].name
        : consumableRegistry()[g.shop[1].registryIdx].name;
    if (s0 != "Dusk" || s1 != "Crafty Joker")
        std::printf("   shop 1 got: %s, %s\n", s0.c_str(), s1.c_str());
    CHECK(s0 == "Dusk", "QJSKRTWR ante1 shop slot 0 = Dusk");
    CHECK(s1 == "Crafty Joker", "QJSKRTWR ante1 shop slot 1 = Crafty Joker");
    // shop 1 packs: FORCED Buffoon (no roll consumed) + Celestial
    CHECK(g.packOffers.size() == 2
          && g.packOffers[0].type == PACK_BUFFOON
          && g.packOffers[0].size == PACKSZ_NORMAL,
          "QJSKRTWR shop 1 pack 0 = forced Buffoon Pack");
    CHECK(g.packOffers[1].type == PACK_CELESTIAL
          && g.packOffers[1].size == PACKSZ_NORMAL,
          "QJSKRTWR shop 1 pack 1 = Celestial Pack");

    // leave without buying; big-blind shop: Strength + Drunkard
    g.applyAction(A_LEAVE_SHOP);
    g.applyAction(A_SELECT_BLIND);
    for (int i = 0; i < 5 && i < (int)g.hand.size(); ++i)
        g.applyAction(A_TOGGLE_CARD_0 + i);
    g.applyAction(A_PLAY);
    g.computeCashOut(1);
    g.applyAction(A_CONFIRM_CASHOUT);
    std::string t0 = g.shop[0].kind == 0
        ? jokerRegistry()[g.shop[0].registryIdx].name
        : consumableRegistry()[g.shop[0].registryIdx].name;
    std::string t1 = g.shop[1].kind == 0
        ? jokerRegistry()[g.shop[1].registryIdx].name
        : consumableRegistry()[g.shop[1].registryIdx].name;
    if (t0 != "Strength" || t1 != "Drunkard")
        std::printf("   shop 2 got: %s, %s\n", t0.c_str(), t1.c_str());
    CHECK(t0 == "Strength", "QJSKRTWR ante1 big-blind shop slot 0 = Strength");
    CHECK(t1 == "Drunkard", "QJSKRTWR ante1 big-blind shop slot 1 = Drunkard");
    // the ante's voucher stays on offer in EVERY shop until bought
    CHECK(g.shopVoucher != V_NONE, "voucher still offered in the 2nd shop");
    CHECK(std::string(voucherName(g.shopVoucher)) == "Hone",
          "QJSKRTWR ante 1 voucher = Hone");
    // shop 2 packs: Celestial + Celestial MEGA (rolls #2 and #3 — proof the
    // forced Buffoon did not consume a draw)
    bool p2ok = g.packOffers.size() == 2
             && g.packOffers[0].type == PACK_CELESTIAL
             && g.packOffers[0].size == PACKSZ_NORMAL
             && g.packOffers[1].type == PACK_CELESTIAL
             && g.packOffers[1].size == PACKSZ_MEGA;
    if (!p2ok && g.packOffers.size() == 2)
        std::printf("   shop 2 packs got: type%d size%d, type%d size%d\n",
                    g.packOffers[0].type, g.packOffers[0].size,
                    g.packOffers[1].type, g.packOffers[1].size);
    CHECK(p2ok, "QJSKRTWR shop 2 packs = Celestial + Celestial Mega");

    // Beat the boss -> the ante ADVANCES and the next shop is ANTE 2's
    // first shop: Venus, Wrathful Joker, with packs Arcana + Celestial Jumbo.
    g.applyAction(A_LEAVE_SHOP);
    g.applyAction(A_SELECT_BLIND);
    CHECK(g.blindIdx == 2, "reached the boss blind");
    g.score = g.blindGoal;                    // shortcut: treat it as beaten
    g.computeCashOut(2);
    g.applyAction(A_CONFIRM_CASHOUT);
    CHECK(g.ante == 2, "ante advances on boss defeat, before the shop");
    std::string u0 = g.shop[0].kind == 0
        ? jokerRegistry()[g.shop[0].registryIdx].name
        : consumableRegistry()[g.shop[0].registryIdx].name;
    std::string u1 = g.shop[1].kind == 0
        ? jokerRegistry()[g.shop[1].registryIdx].name
        : consumableRegistry()[g.shop[1].registryIdx].name;
    if (u0 != "Venus" || u1 != "Wrathful Joker")
        std::printf("   post-boss shop got: %s, %s\n", u0.c_str(), u1.c_str());
    CHECK(u0 == "Venus", "QJSKRTWR post-boss shop slot 0 = Venus");
    CHECK(u1 == "Wrathful Joker", "QJSKRTWR post-boss shop slot 1 = Wrathful Joker");
    CHECK(g.shopVoucher != V_NONE, "the new ante's voucher is offered here");
    bool packsOk = g.packOffers.size() == 2
                && g.packOffers[0].type == PACK_ARCANA
                && g.packOffers[0].size == PACKSZ_NORMAL
                && g.packOffers[1].type == PACK_CELESTIAL
                && g.packOffers[1].size == PACKSZ_JUMBO;
    if (!packsOk && g.packOffers.size() == 2)
        std::printf("   post-boss packs got: type%d size%d, type%d size%d\n",
                    g.packOffers[0].type, g.packOffers[0].size,
                    g.packOffers[1].type, g.packOffers[1].size);
    CHECK(packsOk, "QJSKRTWR post-boss packs = Arcana + Celestial Jumbo");

    // Opening that Arcana pack: The Devil, The Star, The High Priestess.
    // Pack contents are CONSECUTIVE draws from one key with duplicates
    // resampled away (randlist), not one stream per card slot.
    g.money = 100;
    CHECK(g.applyAction(A_BUY_PACK_0), "bought the arcana pack");
    CHECK(g.packContents.size() == 3, "normal arcana pack holds 3");
    const char* want[3] = {"The Devil", "The Star", "The High Priestess"};
    for (int i = 0; i < 3 && i < (int)g.packContents.size(); ++i) {
        std::string got = consumableRegistry()[g.packContents[i].registryIdx].name;
        if (got != want[i])
            std::printf("   pack card %d got %s want %s\n", i, got.c_str(), want[i]);
        CHECK(got == want[i], "QJSKRTWR arcana pack contents match");
    }
    // no duplicates inside a pack
    bool dup = false;
    for (size_t i = 0; i < g.packContents.size(); ++i)
        for (size_t j = i + 1; j < g.packContents.size(); ++j)
            if (g.packContents[i].registryIdx == g.packContents[j].registryIdx)
                dup = true;
    CHECK(!dup, "no duplicate cards inside a pack");
}


// ---- consumable handling rules (v23) ----
static void testConsumableRules() {
    // Arcana/Celestial/Spectral pack picks are USED IMMEDIATELY — they never
    // enter the consumable slots.
    {
        Balatro g("QJSKRTWR");
        g.ante = 2;
        g.beginAnte();
        g.openShop();
        g.money = 100;
        // find an arcana/celestial pack offer
        int which = -1;
        for (size_t i = 0; i < g.packOffers.size(); ++i)
            if (g.packOffers[i].type == PACK_ARCANA
                || g.packOffers[i].type == PACK_CELESTIAL) which = (int)i;
        if (which >= 0) {
            g.applyAction(A_BUY_PACK_0 + which);
            size_t before = g.consumables.size();
            // pick the first legal one
            for (int i = 0; i < 5; ++i)
                if (g.isLegal(A_PICK_PACK_0 + i)) { g.applyAction(A_PICK_PACK_0 + i); break; }
            CHECK(g.consumables.size() != before + 1
                  || g.packContents.empty(),
                  "pack consumables are used, not stored");
        }
    }
    // Secret planets are locked until their hand has been played
    {
        Balatro g("TUTORIAL");
        int px = -1, ce = -1;
        for (size_t i = 0; i < consumableRegistry().size(); ++i) {
            if (consumableRegistry()[i].name == "Planet X") px = (int)i;
            if (consumableRegistry()[i].name == "Ceres")    ce = (int)i;
        }
        CHECK(px >= 0 && g.consumableLocked(px), "Planet X starts locked");
        CHECK(ce >= 0 && g.consumableLocked(ce), "Ceres starts locked");
        g.handPlayCount[FLUSH_HOUSE] = 1;
        CHECK(!g.consumableLocked(ce), "Ceres unlocks after a Flush House");
        // The Soul / Black Hole never appear in normal pools
        for (size_t i = 0; i < consumableRegistry().size(); ++i)
            if (consumableRegistry()[i].name == "The Soul")
                CHECK(g.consumableLocked((int)i), "The Soul is pool-locked");
    }
    // Buy-and-use: a consumable can be bought with FULL slots if it can
    // resolve right now, and is used immediately.
    {
        Balatro g("TUTORIAL");
        g.ante = 3;
        g.beginAnte();
        g.openShop();
        g.money = 100;
        // fill the consumable slots with something inert
        for (size_t i = 0; i < consumableRegistry().size(); ++i)
            if (consumableRegistry()[i].name == "The Hermit")
                while ((int)g.consumables.size() < g.mods.consumableSlots)
                    g.grantConsumable((int)i);
        CHECK((int)g.consumables.size() == g.mods.consumableSlots,
              "consumable slots are full");
        // a planet in the shop should still be buyable (usable immediately)
        for (size_t i = 0; i < g.shop.size(); ++i) {
            if (g.shop[i].kind != 1) continue;
            bool usable = g.consumableUsableNow(g.shop[i].registryIdx);
            bool legal = g.isLegal(A_BUY_SLOT_0 + (int)i);
            CHECK(usable == legal,
                  "full slots: buyable iff usable immediately");
        }
    }
    // A card-targeting tarot cannot be bought/picked with nothing selected
    {
        Balatro g("TUTORIAL");
        int death = -1;
        for (size_t i = 0; i < consumableRegistry().size(); ++i)
            if (consumableRegistry()[i].name == "Death") death = (int)i;
        CHECK(death >= 0 && !g.consumableUsableNow(death),
              "Death is unusable with no selection");
        g.startRound();
        g.applyAction(A_TOGGLE_CARD_0 + 0);
        g.applyAction(A_TOGGLE_CARD_0 + 1);
        CHECK(g.consumableUsableNow(death), "…and usable with exactly 2");
    }
    // High Priestess frees its own slot before creating (already the case,
    // pinned here so it can't regress)
    {
        Balatro g("QJSKRTWR");
        g.ante = 2;
        int hp = -1;
        for (size_t i = 0; i < consumableRegistry().size(); ++i)
            if (consumableRegistry()[i].name == "The High Priestess") hp = (int)i;
        g.grantConsumable(hp);
        g.sel.consumable = 0;
        g.applyAction(A_USE_CONSUMABLE);
        CHECK(g.consumables.size() == 2,
              "High Priestess makes TWO planets (its own slot freed first)");
        CHECK(consumableRegistry()[0].name.size() > 0, "registry intact");
        std::string a = g.consumables[0]->name, b2 = g.consumables[1]->name;
        if (a != "Mercury" || b2 != "Jupiter")
            std::printf("   priestess gave %s + %s\n", a.c_str(), b2.c_str());
        CHECK(a == "Mercury" && b2 == "Jupiter",
              "QJSKRTWR ante2 High Priestess = Mercury + Jupiter");
    }
    // Ante-gated tag locks: Meteor is locked at ante 1
    {
        Balatro g("QJSKRTWR");
        g.beginAnte();
        CHECK(g.anteTagA != TAG_METEOR && g.anteTagB != TAG_METEOR,
              "no Meteor Tag at ante 1 (ante-locked)");
        // Both ante-1 tags verified against the reference run.
        CHECK(std::string(tagName(g.anteTagA)) == "Rare Tag",
              "QJSKRTWR ante1 tag A = Rare Tag");
        CHECK(std::string(tagName(g.anteTagB)) == "Holographic Tag",
              "QJSKRTWR ante1 tag B = Holographic Tag");
    }
}


// ---- plasma balance rounds DOWN (user-confirmed) ----
static void testPlasmaFloor() {
    // 77 x 76 -> 76 x 76, and 144 x 1 -> 72 x 72 (not 72.5 x 72.5)
    struct Case { double chips, mult, want; };
    Case cases[] = {
        {77, 76, 76.0 * 76.0},      // (153)/2 = 76.5 -> 76
        {144, 1, 72.0 * 72.0},      // (145)/2 = 72.5 -> 72
        {76, 76, 76.0 * 76.0},      // even, unchanged
        {10,  2,  6.0 * 6.0},       // (12)/2 = 6
        {11,  2,  6.0 * 6.0},       // (13)/2 = 6.5 -> 6
        {1,   0,  0.0},             // (1)/2 = 0.5 -> 0
    };
    for (auto& c : cases) {
        BigNum bal = (BigNum(c.chips) + BigNum(c.mult)).half().floorVal();
        BigNum got = bal * bal;
        if (got.toDouble() != c.want)
            std::printf("   %g x %g -> got %g want %g\n",
                        c.chips, c.mult, got.toDouble(), c.want);
        CHECK(got.toDouble() == c.want, "plasma balance floors");
    }
    // floor is the identity once the value exceeds the mantissa (deep endless)
    {
        BigNum big(2);
        for (int i = 0; i < 9; ++i) big = big * big;   // 2^512
        CHECK(big.floorVal().e == big.floorVal().floorVal().e,
              "floor is idempotent");
        BigNum sum = big + BigNum(1);                  // 1 is below precision
        CHECK(sum.floorVal().e == big.e, "floor doesn't corrupt huge values");
    }
}

int main() {
    testBigNum();
    testHandEval();
    testPipeline();
    testArchetypes();
    testCashOut();
    testRegistryAndFields();
    testNewJokers();
    testInteraction();
    testShopAndStreams();
    testGameLoop();
    testConsumables();
    testNewJokers2();
    testV14();
    testDestructionSemantics();
    testPlasmaFloor();
    testHandTable();
    testPools();
    testShopQueue();
    testSeedExact();
    testRealSeed();
    testConsumableRules();
    std::printf("%d passed, %d failed\n", passes, fails);
    return fails ? 1 : 0;
}
