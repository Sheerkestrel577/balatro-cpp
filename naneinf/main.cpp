// main.cpp — a greedy scripted bot playing a whole run through the action
// interface, until it dies or clears ante 8.
#include <cstdio>
#include "balatro.hpp"
#include "blinds.hpp"

int main() {
    Balatro g(std::make_unique<MersenneRNG>(20260806));
    g.enterBlindSelect();

    int guard = 0;
    while (g.phase != PHASE_GAME_OVER && g.ante <= 8 && guard++ < 2000) {
        if (g.phase == PHASE_BLIND_SELECT) {
            std::printf("ante %d blind %d", g.ante, g.blindIdx);
            if (g.blindIdx == 2) std::printf(" [%s]", BOSS_DEFS[g.bossId].name);
            std::printf("  goal "); g.blindGoal.print(""); std::printf("\n");
            g.applyAction(A_SELECT_BLIND);
        } else if (g.phase == PHASE_PLAYING) {
            // pick the best-scoring subset of up to 5 cards
            int n = (int)g.hand.size(), bestType = -1; uint32_t bestBits = 0;
            for (uint32_t sub = 1; sub < (1u << n); ++sub) {
                if (__builtin_popcount(sub) > 5) continue;
                std::vector<Card*> cs;
                for (int i = 0; i < n; ++i) if (sub & (1u << i)) cs.push_back(g.hand[i]);
                int t = evaluateHand(cs, g.mods).scoredType;
                int score = t * 100 + __builtin_popcount(sub);
                if (score > bestType) { bestType = score; bestBits = sub; }
            }
            for (int i = 0; i < n; ++i)
                if (bestBits & (1u << i)) g.applyAction(A_TOGGLE_CARD_0 + i);
            if (!g.applyAction(A_PLAY)) {           // blocked by a boss rule?
                g.applyAction(A_CLEAR_SELECTION);
                for (int i = 0; i < 5 && i < (int)g.hand.size(); ++i)
                    g.applyAction(A_TOGGLE_CARD_0 + i);
                if (!g.applyAction(A_DISCARD)) break;
            }
            if (g.score >= g.blindGoal) {
                std::printf("   beat it: "); g.score.print(""); std::printf("\n");
                g.computeCashOut(g.blindIdx);
                g.applyAction(A_CONFIRM_CASHOUT);
            }
        } else if (g.phase == PHASE_SHOP) {
            for (int s = 0; s < 4; ++s) g.applyAction(A_BUY_SLOT_0 + s);
            g.applyAction(A_BUY_PACK_0);
            if (g.phase == PHASE_PACK) {
                for (int i = 0; i < 5; ++i) g.applyAction(A_PICK_PACK_0 + i);
                g.applyAction(A_SKIP_PACK);
            }
            g.applyAction(A_LEAVE_SHOP);
        } else if (g.phase == PHASE_PACK) {
            g.applyAction(A_SKIP_PACK);
        } else break;
    }
    std::printf("\nRESULT: %s at ante %d (jokers %zu, $%d, best blind score ",
                g.phase == PHASE_GAME_OVER ? "DIED" : "cleared ante 8",
                g.ante, g.jokers.size(), g.money);
    g.bestBlindScore.print(""); std::printf(")\n");
    std::printf("naneinf=%s\n", g.won() ? "YES" : "no");
}
