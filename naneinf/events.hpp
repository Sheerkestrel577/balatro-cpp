// events.hpp — interaction types (engine spec §2) + EventData (§3).
#pragma once
#include <vector>

struct Card;
class Joker;

enum InteractionType {
    // scoring pipeline
    HAND_DETERMINED,
    RETRIG_QUERY_SCORED,
    CARD_SCORED,
    RETRIG_QUERY_HELD,
    CARD_HELD,
    INDEPENDENT,
    HAND_SCORED,
    // round structure
    BLIND_SELECTED,
    BLIND_SKIPPED,
    BOSS_TRIGGERED,
    BOSS_DEFEATED,
    ROUND_END,
    ROUND_START,
    // discards
    DISCARD,
    CARD_DISCARDED,
    // shop & economy
    SHOP_ENTERED, SHOP_EXITED,
    SHOP_REROLL,
    CARD_BOUGHT,
    CARD_SOLD,
    SELF_SOLD,
    PACK_OPENED, PACK_SKIPPED,
    // deck & consumables
    CONSUMABLE_USED,
    CARD_ADDED,
    CARDS_DESTROYED,
    LUCKY_TRIGGERED,
    JOKERS_CHANGED,
};

// One fat POD (spec §3): facts about the event, read-only for jokers.
// Mutable scoring state lives in Balatro::scoring, not here.
struct EventData {
    Card* card = nullptr;
    const std::vector<Card*>* cards = nullptr;
    int handType = 0;
    Joker* joker = nullptr;
    int consumableId = -1;
    int amount = 0;
};
