// keys.hpp — Balatro's REAL RNG key strings.
//
// Source: Immolate (SpectralPack), lib/cache.cl — the type_str/source_str
// tables and the node composition in instance.cl. CC BY-NC-SA 4.0.
//
// Composition rule (get_node_child): a key is the concatenation of its
// components in order, e.g. randchoice_common(R_Joker_Common, S_Shop, 3)
// gives "Joker1" + "sho" + "3" = "Joker1sho3". Resamples append
// "_resample<N+1>". Some keys carry no source and/or no ante.
//
// PHASE-2 STATUS: these strings are correct. Whether this engine CONSUMES
// them in the same ORDER and COUNT as the real game is a separate question
// and is NOT yet verified — see PHASE2_STATUS in the docs.
#pragma once
#include <string>

namespace keys {

// ---- sources (source_str) ----
inline const char* SRC_SHOP        = "sho";
inline const char* SRC_EMPEROR     = "emp";
inline const char* SRC_PRIESTESS   = "pri";
inline const char* SRC_JUDGEMENT   = "jud";
inline const char* SRC_WRAITH      = "wra";
inline const char* SRC_ARCANA      = "ar1";
inline const char* SRC_CELESTIAL   = "pl1";
inline const char* SRC_SPECTRAL    = "spe";
inline const char* SRC_STANDARD    = "sta";
inline const char* SRC_BUFFOON     = "buf";
inline const char* SRC_VAGABOND    = "vag";
inline const char* SRC_SUPERPOS    = "sup";
inline const char* SRC_8BALL       = "8ba";
inline const char* SRC_SEANCE      = "sea";
inline const char* SRC_SIXTH_SENSE = "sixth";
inline const char* SRC_TOP_UP      = "top";
inline const char* SRC_RARE_TAG    = "rta";
inline const char* SRC_UNCOMMON_TAG= "uta";
inline const char* SRC_BLUE_SEAL   = "blusl";
inline const char* SRC_PURPLE_SEAL = "8ba";   // yes, same as 8 Ball (game quirk)
inline const char* SRC_SOUL        = "sou";
inline const char* SRC_RIFF_RAFF   = "rif";
inline const char* SRC_CARTOMANCER = "car";
inline const char* SRC_NONE        = "";

inline std::string A(int ante) { return std::to_string(ante); }

// ---- type + source + ante ----
inline std::string jokerCommon(const char* src, int a)    { return std::string("Joker1") + src + A(a); }
inline std::string jokerUncommon(const char* src, int a)  { return std::string("Joker2") + src + A(a); }
inline std::string jokerRare(const char* src, int a)      { return std::string("Joker3") + src + A(a); }
inline std::string jokerLegendary(const char* src, int a) { return std::string("Joker4") + src + A(a); }
inline std::string jokerByRarity(int rarity, const char* src, int a) {
    switch (rarity) {
        case 2:  return jokerUncommon(src, a);
        case 3:  return jokerRare(src, a);
        case 4:  return jokerLegendary(src, a);
        default: return jokerCommon(src, a);
    }
}
// NOTE the order: the game passes {N_Type, N_Ante, N_Source} for rarity —
// ante BEFORE source — unlike almost every other key. ("rarity1sho")
inline std::string jokerRarity(const char* src, int a)  { return std::string("rarity") + A(a) + src; }
inline std::string jokerEdition(const char* src, int a) { return std::string("edi") + src + A(a); }

inline std::string tarot(const char* src, int a)    { return std::string("Tarot") + src + A(a); }
inline std::string planet(const char* src, int a)   { return std::string("Planet") + src + A(a); }
inline std::string spectral(const char* src, int a) { return std::string("Spectral") + src + A(a); }

// standard (playing) cards
inline std::string cardHasEnhancement(int a) { return std::string("stdset") + A(a); }
inline std::string enhancement(const char* src, int a) { return std::string("Enhanced") + src + A(a); }
inline std::string cardFront(const char* src, int a)   { return std::string("front") + src + A(a); }
inline std::string cardEdition(int a)   { return std::string("standard_edition") + A(a); }
inline std::string cardHasSeal(int a)   { return std::string("stdseal") + A(a); }
inline std::string cardSealType(int a)  { return std::string("stdsealtype") + A(a); }
// Shop item type: {N_Type, N_Ante} only — NO source component. ("cdt1")
inline std::string cardType(int a) { return std::string("cdt") + A(a); }

// shop / run structure
inline std::string shopPack(int a)   { return std::string("shop_pack") + A(a); }
inline std::string tag(int a)        { return std::string("Tag") + A(a); }
inline std::string voucher(int a)    { return std::string("Voucher") + A(a); }
inline std::string voucherFromTag() { return "Voucher_fromtag"; }
inline std::string orbitalTag(int a) { return std::string("orbital") + A(a); }
inline std::string shuffle(int a)    { return std::string("nr") + A(a); }
// Pressing the Cash Out button reshuffles the deck (button_callbacks.lua
// G.FUNCS.cash_out): G.deck:shuffle('cashout'..ante). THIS — not the
// end-of-round collection order — is what Arcana/Spectral packs deal from.
inline std::string cashoutShuffle(int a) { return std::string("cashout") + A(a); }
inline std::string boss()            { return "boss"; }   // NOTE: no ante
inline std::string soul(const char* src, int a) { return std::string("soul_") + src + A(a); }
inline std::string erratic()         { return "erratic"; }

// per-card / per-joker effects
inline std::string luckyMult()   { return "lucky_mult"; }
inline std::string luckyMoney()  { return "lucky_money"; }
inline std::string misprint()    { return "misprint"; }
inline std::string grosMichel()  { return "gros_michel"; }
inline std::string cavendish()   { return "cavendish"; }
inline std::string wheelOfFortune() { return "wheel_of_fortune"; }
inline std::string sigil()       { return "sigil"; }
inline std::string ouija()       { return "ouija"; }

// resample suffix: "_resample<N+1>" for N >= 1
inline std::string resample(const std::string& base, int n) {
    return n <= 0 ? base : base + "_resample" + std::to_string(n + 1);
}

// ---- booster pack table (items.cl PACKS + PACK_INFO) ----
// ONE weighted roll per pack from "shop_pack<ante>", scaled by the total
// weight and walked in this order. Two packs per shop = two consecutive
// draws from the same key (NOT resamples).
struct PackEntry { int type; int size; double weight; int count; int picks; };
inline constexpr double PACK_TOTAL_WEIGHT = 22.42;
inline constexpr PackEntry PACK_TABLE[] = {
    // type(0 arcana,1 celestial,2 standard,3 buffoon,4 spectral), size, weight, cards, picks
    {0, 0, 4.00, 3, 1}, {0, 1, 2.00, 5, 1}, {0, 2, 0.50, 5, 2},   // Arcana
    {1, 0, 4.00, 3, 1}, {1, 1, 2.00, 5, 1}, {1, 2, 0.50, 5, 2},   // Celestial
    {2, 0, 4.00, 3, 1}, {2, 1, 2.00, 5, 1}, {2, 2, 0.50, 5, 2},   // Standard
    {3, 0, 1.20, 2, 1}, {3, 1, 0.60, 4, 1}, {3, 2, 0.15, 4, 2},   // Buffoon
    {4, 0, 0.60, 2, 1}, {4, 1, 0.30, 4, 1}, {4, 2, 0.07, 4, 2},   // Spectral
};
inline constexpr int PACK_TABLE_N = 15;

// ---- shop composition (functions.cl) ----
// base rates: joker 20, tarot 4, planet 4, playing card 0, spectral 0.
// roll = random(cardType(ante)) * totalRate, then walk in the order
// joker -> tarot -> planet -> playing card -> spectral.
inline constexpr double RATE_JOKER = 20, RATE_TAROT = 4, RATE_PLANET = 4;
// joker rarity thresholds: >0.95 rare, >0.7 uncommon, else common
inline constexpr double RARITY_RARE = 0.95, RARITY_UNCOMMON = 0.70;
// joker edition thresholds (next_joker_edition)
inline constexpr double ED_NEG = 0.997, ED_POLY = 0.994,
                        ED_HOLO = 0.98,  ED_FOIL = 0.96;

// ============================================================
//  GAMEPLAY-EFFECT KEYS
// ============================================================
// Immolate only models shop/pack GENERATION, so it does not contain the
// keys for in-round effects. The keys below were VERIFIED against Balatro's
// reverse-engineered Lua source (card.lua, blind.lua, common_events.lua,
// state_events.lua) and annotated with their exact call site. A handful were
// wrong before verification (space_joker->space, reserved_parking->parking,
// halu->halu..ante, ancient->anc, cert/certificate->mail/cert_fr,
// to_do_list..ante->to_do, marble->marb_fr) and have been corrected.
//
// NOTE on card-generating jokers (Marble, Certificate, Idol, Ancient,
// Mail-In Rebate): the real game makes ONE pseudorandom_element pick over a
// card/suit/rank/hand list per effect. This engine instead rolls rank and
// suit on separate sub-keys, so even with the correct BASE key these are not
// yet bit-exact — matching them fully needs the single-element sampling, not
// just the key string.
//
// Per-key isolation means a wrong name here corrupts only that one effect —
// it cannot desync the shop, tags, vouchers or bosses.

// [SRC] verified against Immolate cache.cl
inline std::string kGlass()          { return "glass"; }              // state_events: pseudorandom('glass') [confirmed]
inline std::string kBusinessCard()   { return "business"; }           // card.lua: pseudorandom('business') [confirmed]
inline std::string kBloodstone()     { return "bloodstone"; }         // card.lua: pseudorandom('bloodstone') [confirmed]
inline std::string kReservedParking(){ return "parking"; }            // card.lua: pseudorandom('parking')
inline std::string kSpaceJoker()     { return "space"; }               // card.lua: pseudorandom('space')
inline std::string k8Ball()          { return "8ball"; }              // card.lua: pseudorandom('8ball') [confirmed]
inline std::string kHallucination(int a){ return "halu" + A(a); }     // card.lua: pseudorandom('halu'..ante)
inline std::string kAncient(int a)   { return "anc" + A(a); }         // common_events: pseudoseed('anc'..ante)
inline std::string kIdol(int a)      { return "idol" + A(a); }        // common_events: pseudoseed('idol'..ante) [single card pick]
inline std::string kCastle(int a)    { return "cas" + A(a); }         // common_events: pseudoseed('cas'..ante) [confirmed]
inline std::string kToDoList(int)    { return "to_do"; }               // card.lua: pseudoseed('to_do') [no ante]
inline std::string kMailRebate(int a){ return "mail" + A(a); }        // common_events: pseudoseed('mail'..ante)
inline std::string kCertificate(int) { return "cert_fr"; }            // card.lua: front pseudoseed('cert_fr'); seal uses 'certsl'
inline std::string kMarble()         { return "marb_fr"; }            // card.lua: pseudoseed('marb_fr') [real: single card pick; sim splits r/s]
inline std::string kMadness()        { return "madness"; }            // card.lua: pseudorandom('madness') [confirmed]
inline std::string kAnkh()           { return "ankh_choice"; }        // card.lua: pseudoseed('ankh_choice') [confirmed]
inline std::string kHex()            { return "hex"; }                // card.lua: pseudoseed('hex') [confirmed]
inline std::string kInvisible()      { return "invisible"; }          // card.lua: pseudoseed('invisible') [confirmed]
inline std::string kPerkeo()         { return "perkeo"; }             // card.lua: pseudoseed('perkeo') [confirmed]
inline std::string kImmolate()       { return "immolate"; }           // card.lua: pseudoseed('immolate') [confirmed]
inline std::string kAura()           { return "aura"; }               // card.lua: poll_edition('aura') [confirmed]
inline std::string kEctoplasm()      { return "ectoplasm"; }          // card.lua: pseudoseed('ectoplasm') [confirmed]
inline std::string kSpectralDestroy(){ return "spe_card"; }           // card.lua: pseudoseed('spe_card') [confirmed]
// boss-blind effects
inline std::string kHook()           { return "hook"; }               // blind.lua: pseudoseed('hook') [confirmed]
inline std::string kCrimsonHeart()   { return "crimson_heart"; }      // blind.lua: pseudoseed('crimson_heart') [confirmed]
inline std::string kCeruleanBell()   { return "cerulean_bell"; }      // blind.lua: pseudoseed('cerulean_bell') [confirmed]
inline std::string kWheelFaceDown()  { return "wheel"; }              // blind.lua: pseudoseed('wheel') [confirmed]
// tag-created jokers use the real source tags (rta/uta) with the joker key
inline std::string kTagJoker(int rarity, int a) {
    return jokerByRarity(rarity, rarity == 3 ? SRC_RARE_TAG : SRC_UNCOMMON_TAG, a);
}

// ---- keys the real game has that this engine does not yet mirror ----
// glass break, joker probability procs (Bloodstone, Business Card, Reserved
// Parking, Space Joker, 8 Ball, Hallucination), The Hook's discards, Crimson
// Heart's joker pick, Cerulean Bell's card pick, per-round joker rerolls
// (Ancient/Idol/Castle/Rebate/To Do List), Madness/Invisible/Ankh/Hex picks.
// These use ad-hoc names in this engine — see PHASE2_STATUS.md.

}  // namespace keys
