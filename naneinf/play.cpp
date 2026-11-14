// play.cpp — interactive command-line Balatro. Drives the engine purely
// through the action interface, so whatever you can do here, the AI can do.
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <cctype>
#include <random>
#include <string>
#include <vector>
#include <memory>

#include "balatro.hpp"
#include "blinds.hpp"

static const char* SUITS[4] = {"S", "H", "C", "D"};
static const char* RANKS[15] = {"", "", "2", "3", "4", "5", "6", "7", "8",
                                "9", "10", "J", "Q", "K", "A"};
static const char* ENH[9] = {"", " bonus", " mult", " wild", " GLASS",
                             " STEEL", " STONE", " GOLD", " lucky"};
static const char* EDN[5] = {"", " foil", " holo", " POLY", " NEG"};
static const char* SEALS[5] = {"", " [red]", " [blue]", " [gold]", " [purple]"};
static const char* PHASES[6] = {"BLIND SELECT", "PLAYING", "CASH OUT",
                                "SHOP", "PACK", "GAME OVER"};
static const char* PACKNAMES[5] = {"Arcana", "Celestial", "Standard",
                                   "Buffoon", "Spectral"};
static const char* PACKSIZES[3] = {"", " Jumbo", " Mega"};

static std::string cardStr(const Balatro& g, const Card& c) {
    if (c.faceDown) return "[?? ]";
    std::string s = std::string(RANKS[c.rank]) + SUITS[c.suit];
    s += ENH[c.enhancement];
    s += EDN[c.edition];
    s += SEALS[c.seal];
    if (g.isDebuffed(c)) s += " (X)";
    return s;
}

static void showState(Balatro& g) {
    std::printf("\n================================================\n");
    std::printf("Ante %d | blind %s | %s | $%d\n", g.ante,
                g.blindIdx == 0 ? "Small" : g.blindIdx == 1 ? "Big" : "BOSS",
                PHASES[g.phase], g.money);
    if (g.blindIdx == 2 && g.bossId >= 0)
        std::printf("BOSS: %s\n", BOSS_DEFS[g.bossId].name);
    std::printf("goal "); g.blindGoal.print("");
    std::printf("   score "); g.score.print("");
    std::printf("   hands %d  discards %d\n", g.handsLeft, g.discardsLeft);

    if (!g.jokers.empty()) {
        std::printf("Jokers:");
        for (size_t i = 0; i < g.jokers.size(); ++i) {
            Joker* j = g.jokers[i].get();
            std::printf("  %zu:%s%s%s%s", i,
                        j->faceDown ? "???" : j->name.c_str(),
                        EDN[j->edition], j->disabled ? "(off)" : "",
                        (int)i == g.sel.joker ? "*" : "");
        }
        std::printf("\n");
    }
    {
        std::string owned;
        for (int v = 1; v < NUM_VOUCHERS; ++v)
            if (g.ownedVouchers[v]) { owned += owned.empty() ? "" : ", ";
                                      owned += voucherName(v); }
        if (!owned.empty()) std::printf("Vouchers: %s\n", owned.c_str());
        std::string tags;
        for (int t : g.pendingTags) { tags += tags.empty() ? "" : ", ";
                                      tags += tagName(t); }
        if (g.investmentTagPending) tags += (tags.empty() ? "" : ", "),
                                    tags += "Investment Tag";
        if (!tags.empty()) std::printf("Pending tags: %s\n", tags.c_str());
    }
    if (!g.consumables.empty()) {
        std::printf("Consumables:");
        for (size_t i = 0; i < g.consumables.size(); ++i)
            std::printf("  %zu:%s%s", i, g.consumables[i]->name.c_str(),
                        (int)i == g.sel.consumable ? "*" : "");
        std::printf("\n");
    }
    if (g.phase == PHASE_PLAYING || (g.phase == PHASE_PACK && !g.hand.empty())) {
        std::printf("Hand (%zu cards, %zu in draw pile):\n  ",
                    g.hand.size(), g.drawPile.size());
        for (size_t i = 0; i < g.hand.size(); ++i)
            std::printf("%zu:%s%s  ", i, cardStr(g, *g.hand[i]).c_str(),
                        g.sel.hasCard((int)i) ? "*" : "");
        std::printf("\n");
        if (!g.sel.cards.empty()) {
            std::vector<Card*> cs;
            for (int i : g.sel.cards) cs.push_back(g.hand[i]);
            auto r = evaluateHand(cs, g.mods);
            std::printf("  selected -> %s\n", HAND_NAMES[r.scoredType]);
        }
    }
    if (g.phase == PHASE_BLIND_SELECT && g.skipTag != TAG_NONE)
        std::printf("Skip reward: %s\n", tagName(g.skipTag));
    if (g.phase == PHASE_CASH_OUT) {
        const auto& c = g.pendingCashOut();
        std::printf("Cash out: blind $%d + hands $%d + gold $%d + jokers $%d"
                    " + tag $%d + interest $%d = $%d\n",
                    c.blindReward, c.handsBonus, c.goldCards, c.jokerTotal,
                    c.investmentTag, c.interest, c.total());
    }
    if (g.phase == PHASE_SHOP) {
        std::printf("Shop:\n");
        for (size_t i = 0; i < g.shop.size(); ++i) {
            const ShopItem& it = g.shop[i];
            if (it.sold) { std::printf("  %zu: (sold)\n", i); continue; }
            const char* nm = it.kind == 0
                ? jokerRegistry()[it.registryIdx].name.c_str()
                : consumableRegistry()[it.registryIdx].name.c_str();
            std::printf("  %zu: %s%s  $%d\n", i, nm, EDN[it.edition], it.cost);
        }
        for (size_t i = 0; i < g.packOffers.size(); ++i) {
            const PackOffer& p = g.packOffers[i];
            if (p.taken) continue;
            std::printf("  pack %zu: %s%s Pack  $%d\n", i,
                        PACKNAMES[p.type], PACKSIZES[p.size], p.cost);
        }
        if (g.shopVoucher != V_NONE)
            std::printf("  voucher: %s  $10\n", voucherName(g.shopVoucher));
        std::printf("  reroll $%d\n", g.rerollCost());
    }
    if (g.phase == PHASE_PACK) {
        std::printf("Pack contents (%d pick(s) left):\n", g.packPicksLeft);
        for (size_t i = 0; i < g.packContents.size(); ++i) {
            const PackCard& pc = g.packContents[i];
            if (pc.taken) { std::printf("  %zu: (taken)\n", i); continue; }
            if (pc.kind == 0)
                std::printf("  %zu: %s\n", i,
                            consumableRegistry()[pc.registryIdx].name.c_str());
            else if (pc.kind == 1)
                std::printf("  %zu: %s (joker)\n", i,
                            jokerRegistry()[pc.registryIdx].name.c_str());
            else
                std::printf("  %zu: %s\n", i, cardStr(g, pc.card).c_str());
        }
    }
}

static void showHelp() {
    std::printf(
      "\nCommands:\n"
      "  c <n> [n..]  toggle card(s) in hand      j <n>  select joker\n"
      "  u <n>        select consumable           clear  clear selection\n"
      "  play / p     play selected               disc / d   discard selected\n"
      "  use          use selected consumable      sell   sell selected joker\n"
      "  sellc        sell selected consumable\n"
      "  < / >        shift selected card left/right\n"
      "  jl / jr      shift selected joker left/right\n"
      "  select / s   select blind                skip   skip blind (tag)\n"
      "  rerollboss   reroll the boss (needs voucher)   hold  hold joker (Amber Acorn)\n"
      "  cash         cash out                    ok     confirm cash out\n"
      "  buy <n>      buy shop slot               pack <n>  buy booster pack\n"
      "  voucher      buy the voucher             reroll shop reroll\n"
      "  leave        leave the shop              pick <n>  take a pack card\n"
      "  skippack     skip the rest of the pack\n"
      "  legal        list every legal action     state  reprint the board\n"
      "  dbg ...      debug/testing commands (type 'dbg help')\n"
      "  help / quit\n");
}

static void showDebugHelp() {
    std::printf(
      "\nDebug commands (testing only — not part of the AI action interface):\n"
      "  dbg money <n>      add n to money (n may be negative)\n"
      "  dbg hands <n>      add n to hands left this round\n"
      "  dbg discards <n>   add n to discards left this round\n"
      "  dbg ante <n>       set the ante counter to n\n"
      "  dbg reseed <SEED>  swap in a fresh RNG seed WITHOUT restarting the\n"
      "                     program (test a new seed in place)\n"
      "  dbg reroll         free shop reroll: restock every slot, no cost,\n"
      "                     no cost escalation (spin shop RNG cheaply)\n"
      "  dbg roll <key> [lo hi]\n"
      "                     draw one value from the RNG stream for <key> and\n"
      "                     print it (random() in [0,1); randint(lo,hi) if a\n"
      "                     range is given). Consumes a draw from that stream.\n"
      "  dbg help           show this list\n");
}

int main() {
    std::printf("=== naneinf: White Stake Plasma ===\n");
    std::printf("Seed (8 chars like ALEEB, or blank for random): ");
    std::string line;
    std::getline(std::cin, line);
    for (auto& ch : line) ch = (char)toupper((unsigned char)ch);
    if (line.empty()) {
        std::mt19937_64 gen(std::random_device{}());
        line = BalatroRNG::random_seed_string(gen);
    }
    std::printf("seed: %s   (real Balatro RNG — type this into the game)\n",
                line.c_str());
    Balatro g(line);
    g.enterBlindSelect();
    showHelp();
    showState(g);

    while (true) {
        if (g.phase == PHASE_GAME_OVER) {
            std::printf("\n*** GAME OVER at ante %d ***  best blind score ",
                        g.ante);
            g.bestBlindScore.print(""); std::printf("\n");
            break;
        }
        std::printf("\n> ");
        if (!std::getline(std::cin, line)) break;
        std::istringstream ss(line);
        std::string cmd; ss >> cmd;
        if (cmd.empty()) continue;

        auto act = [&](int a) {
            if (!g.applyAction(a)) std::printf("(illegal)\n");
        };

        if (cmd == "quit" || cmd == "q") break;
        else if (cmd == "help" || cmd == "h") { showHelp(); continue; }
        else if (cmd == "state") { showState(g); continue; }
        else if (cmd == "legal") {
            bool mask[NUM_ACTIONS];
            g.legalActions(mask);
            std::printf("legal action ids:");
            for (int a = 0; a < NUM_ACTIONS; ++a) if (mask[a]) std::printf(" %d", a);
            std::printf("\n");
            continue;
        }
        else if (cmd == "dbg") {
            std::string sub; ss >> sub;
            if (sub.empty() || sub == "help") { showDebugHelp(); continue; }
            else if (sub == "money") {
                int n;
                if (!(ss >> n)) { std::printf("usage: dbg money <n>\n"); continue; }
                g.money += n;
                std::printf("[dbg] money %+d -> $%d\n", n, g.money);
                showState(g);
            }
            else if (sub == "hands") {
                int n;
                if (!(ss >> n)) { std::printf("usage: dbg hands <n>\n"); continue; }
                g.handsLeft += n;
                if (g.handsLeft < 0) g.handsLeft = 0;
                std::printf("[dbg] hands %+d -> %d\n", n, g.handsLeft);
                showState(g);
            }
            else if (sub == "discards") {
                int n;
                if (!(ss >> n)) { std::printf("usage: dbg discards <n>\n"); continue; }
                g.discardsLeft += n;
                if (g.discardsLeft < 0) g.discardsLeft = 0;
                std::printf("[dbg] discards %+d -> %d\n", n, g.discardsLeft);
                showState(g);
            }
            else if (sub == "ante") {
                int n;
                if (!(ss >> n)) { std::printf("usage: dbg ante <n>\n"); continue; }
                g.ante = n;
                std::printf("[dbg] ante set to %d "
                            "(note: does not re-roll the current blind goal)\n",
                            g.ante);
                showState(g);
            }
            else if (sub == "reseed") {
                std::string seed; ss >> seed;
                if (seed.empty()) { std::printf("usage: dbg reseed <SEED>\n"); continue; }
                for (auto& ch : seed) ch = (char)toupper((unsigned char)ch);
                g.rng = std::make_unique<BalatroRNG>(seed);
                g.seedString = seed;
                std::printf("[dbg] RNG reseeded to %s. Already-dealt cards and the\n"
                            "      current shop are unchanged; new draws use the new\n"
                            "      seed. Use 'dbg reroll' in the shop to respin it.\n",
                            seed.c_str());
            }
            else if (sub == "reroll") {
                if (g.phase != PHASE_SHOP) {
                    std::printf("[dbg] reroll only works in the shop\n");
                    continue;
                }
                for (size_t i = 0; i < g.shop.size(); ++i) g.rollShopSlot((int)i);
                std::printf("[dbg] free reroll: all %zu slots restocked "
                            "(no cost, cost stays $%d)\n",
                            g.shop.size(), g.rerollCost());
                showState(g);
            }
            else if (sub == "roll") {
                std::string key; ss >> key;
                if (key.empty()) {
                    std::printf("usage: dbg roll <key> [lo hi]\n");
                    continue;
                }
                int lo, hi;
                if (ss >> lo >> hi) {
                    int v = g.rng->randint(key, lo, hi);
                    std::printf("[dbg] randint(\"%s\", %d, %d) = %d\n",
                                key.c_str(), lo, hi, v);
                } else {
                    double v = g.rng->random(key);
                    std::printf("[dbg] random(\"%s\") = %.10f\n", key.c_str(), v);
                }
            }
            else std::printf("? unknown dbg subcommand; type 'dbg help'\n");
            continue;
        }
        else if (cmd == "c") { int n; while (ss >> n) act(A_TOGGLE_CARD_0 + n); }
        else if (cmd == "j") { int n; ss >> n; act(A_TOGGLE_JOKER_0 + n); }
        else if (cmd == "u") { int n; ss >> n; act(A_TOGGLE_CONSUMABLE_0 + n); }
        else if (cmd == "clear")  act(A_CLEAR_SELECTION);
        else if (cmd == "sort") {               // rank <-> suit, like the game
            g.handSortMode = 1 - g.handSortMode;
            g.sel = Selection{};                // indices change meaning
            g.sortHand();
            std::printf("hand sort: %s\n",
                        g.handSortMode ? "SUIT (S,H,C,D; rank desc within)"
                                       : "RANK (desc; ties S>H>C>D)");
        }
        else if (cmd == "play" || cmd == "p")  act(A_PLAY);
        else if (cmd == "disc" || cmd == "d")  act(A_DISCARD);
        else if (cmd == "use")    act(A_USE_CONSUMABLE);
        else if (cmd == "sell")   act(A_SELL_JOKER);
        else if (cmd == "sellc")  act(A_SELL_CONSUMABLE);
        else if (cmd == "<")      act(A_SHIFT_CARD_LEFT);
        else if (cmd == ">")      act(A_SHIFT_CARD_RIGHT);
        else if (cmd == "jl")     act(A_SHIFT_JOKER_LEFT);
        else if (cmd == "jr")     act(A_SHIFT_JOKER_RIGHT);
        else if (cmd == "select" || cmd == "s") act(A_SELECT_BLIND);
        else if (cmd == "skip")   act(A_SKIP_BLIND);
        else if (cmd == "rerollboss") act(A_REROLL_BOSS);
        else if (cmd == "hold")   act(A_HOLD_JOKER);
        else if (cmd == "cash") {
            if (g.phase == PHASE_PLAYING) g.computeCashOut(g.blindIdx);
            else std::printf("(not playing)\n");
        }
        else if (cmd == "ok")     act(A_CONFIRM_CASHOUT);
        else if (cmd == "buy")    { int n; ss >> n; act(A_BUY_SLOT_0 + n); }
        else if (cmd == "pack")   { int n; ss >> n; act(A_BUY_PACK_0 + n); }
        else if (cmd == "pick")   { int n; ss >> n; act(A_PICK_PACK_0 + n); }
        else if (cmd == "skippack") act(A_SKIP_PACK);
        else if (cmd == "voucher") act(A_BUY_VOUCHER);
        else if (cmd == "reroll") act(A_REROLL_SHOP);
        else if (cmd == "leave")  act(A_LEAVE_SHOP);
        else { std::printf("? type 'help'\n"); continue; }

        // auto-advance: beating the blind opens the cash-out screen
        if (g.phase == PHASE_PLAYING && g.score >= g.blindGoal) {
            std::printf("\n*** BLIND BEATEN ***\n");
            g.computeCashOut(g.blindIdx);
        }
        if (g.won())
            std::printf("\n*** NANEINF REACHED ON ANTE %d ***\n", g.naneinfAnte);
        showState(g);
    }
    std::printf("bye\n");
}
