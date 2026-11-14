"""
evo_agent.py — set-based policy for naneinf, pure NumPy.  (v3)

Architecture:
  * one shared card encoder, applied to every card anywhere (hand, deck)
  * per-family encoders for jokers / consumables / shop / pack contents
  * mean-pooling into a global context vector c (Deep Sets)
  * a second pool s over currently-SELECTED hand cards
  * pointer heads g(embed_i, c, s) -> one score per entity
  * a singleton head c -> PLAY / DISCARD / LEAVE_SHOP / ...
  * the game loop is the recurrence: one micro-action per step, re-observe

v2: one-hot ranks + explicit rank/suit MATCH features + selection hand
    type, so "toggle toward a pair" is linearly expressible.

v3: ANTI-FIDGET. Successive runs found clear_selection spam, then shift
    spam, then toggle churn, then consumable spam -- remove one and the
    next fills the vacuum, because non-progress actions were ~free. So
    price the CATEGORY, not the action, in four layers (see FIDGET
    POLICY below). All four scale with `es`.

    es = 1.0  full aggression (default)
    es = 0.5  half strength, roughly v2.5
    es = 0.0  off entirely (v2 behaviour)

GENOME: v2-minus-CLEAR_SELECTION. 27,110 params (fresh start required).
"""
import ctypes

import numpy as np

from naneinf import Balatro, Action, NUM_ACTIONS, _lib

# ------------------------------------------------------------- deck access
_lib.nf_deck_dump.restype = ctypes.c_int
_lib.nf_deck_dump.argtypes = [ctypes.c_void_p,
                              ctypes.POINTER(ctypes.c_int), ctypes.c_int]


def deck_cards(g):
    """(n, 5) int array of live deck cards: rank, suit, enh, edition, seal."""
    n = _lib.nf_deck_dump(g._h, None, 0)
    if n == 0:
        return np.zeros((0, 5), dtype=np.int32)
    buf = (ctypes.c_int * (n * 5))()
    _lib.nf_deck_dump(g._h, buf, n * 5)
    return np.frombuffer(buf, dtype=np.int32).reshape(n, 5).copy()


def king_counts(deck):
    """Kings per suit, and the non-king remainder."""
    if len(deck) == 0:
        return np.zeros(4, dtype=int), 0
    kings = deck[deck[:, 0] == 13]
    per_suit = np.bincount(kings[:, 1], minlength=4)[:4]
    return per_suit, len(deck) - int(per_suit.max())


# ----------------------------------------------------------------- layout
N_JOKER_IDS, N_CONS_IDS = 150, 52
E_CARD, E_JOKER, E_CONS, E_SHOP, E_PACKC = 16, 12, 8, 8, 12
D_JEMB, D_CEMB = 6, 4
F_CARD = 41          # 4 scalars + rank oh(13) + suit(4) + enh(9)
                     # + edition(4) + seal(4) + 3 flags
F_JOKER, F_CONS, F_SHOP, F_PACK, F_PACKC = 15, 8, 10, 8, 11
N_MATCH = 3          # per-card: rank-match, suit-match, n_selected
TRUNK_H, C_DIM, HEAD_H = 64, 64, 16
N_HT = 12            # poker hand types

# CLEAR_SELECTION is deliberately ABSENT: clearing an empty selection is a
# no-op that leaves the observation identical, so a greedy policy that
# picks it once picks it forever (a fixed point). Toggling a card off does
# the same job with finer control. Do not re-add without reading v3 notes.
SINGLETONS = ([Action.SHIFT_CARD_LEFT, Action.SHIFT_CARD_RIGHT,
               Action.SHIFT_JOKER_LEFT, Action.SHIFT_JOKER_RIGHT,
               Action.PLAY, Action.DISCARD, Action.USE_CONSUMABLE,
               Action.SELL_CONSUMABLE, Action.SELL_JOKER,
               Action.CONFIRM_CASHOUT,
               Action.SELECT_BLIND, Action.SKIP_BLIND, Action.BUY_VOUCHER,
               Action.REROLL_SHOP, Action.LEAVE_SHOP, Action.REROLL_BOSS,
               Action.HOLD_JOKER, Action.SKIP_PACK])
N_SINGLE = len(SINGLETONS)

# obs[0:20] + 12 hand levels + 9 deck stats + selection type oh + count
N_SCALARS = 20 + 12 + 9 + N_HT + 1
TRUNK_IN = (N_SCALARS + (E_CARD + 1) * 2 + E_CARD + (E_JOKER + 1)
            + (E_CONS + 1) + (E_SHOP + 1) + 2 * F_PACK + (E_PACKC + 1))


class Spec:
    """Auto-packs named weight matrices into one flat genome."""

    def __init__(self):
        self.shapes, self.size = {}, 0

    def add(self, name, *shape):
        n = int(np.prod(shape))
        self.shapes[name] = (self.size, shape)
        self.size += n

    def unpack(self, genome):
        return {k: genome[o:o + int(np.prod(s))].reshape(s)
                for k, (o, s) in self.shapes.items()}


SPEC = Spec()
SPEC.add("jemb", N_JOKER_IDS, D_JEMB)
SPEC.add("cemb", N_CONS_IDS, D_CEMB)
for nm, fi, fo in [("card", F_CARD, E_CARD), ("jok", F_JOKER, E_JOKER),
                   ("con", F_CONS, E_CONS), ("shop", F_SHOP, E_SHOP),
                   ("pkc", F_PACKC, E_PACKC)]:
    SPEC.add(f"{nm}_W", fi, fo)
    SPEC.add(f"{nm}_b", fo)
SPEC.add("trunk_W", TRUNK_IN, TRUNK_H)
SPEC.add("trunk_b", TRUNK_H)
SPEC.add("ctx_W", TRUNK_H, C_DIM)
SPEC.add("ctx_b", C_DIM)
for nm, e in [("h_card", E_CARD + N_MATCH), ("h_jok", E_JOKER),
              ("h_con", E_CONS), ("h_buy", E_SHOP), ("h_pack", F_PACK),
              ("h_pick", E_PACKC)]:
    SPEC.add(f"{nm}_W1", e + C_DIM + E_CARD, HEAD_H)
    SPEC.add(f"{nm}_b1", HEAD_H)
    SPEC.add(f"{nm}_w2", HEAD_H)
SPEC.add("single_W", C_DIM, N_SINGLE)
SPEC.add("single_b", N_SINGLE)

GENOME_SIZE = SPEC.size


def random_genome(rng, scale=0.4):
    return rng.normal(0, scale, GENOME_SIZE).astype(np.float32)


# ------------------------------------------------------------- featurizers
def _onehot(i, n):
    v = np.zeros(n, dtype=np.float32)
    if 0 <= i < n:
        v[i] = 1.0
    return v


def card_feats(rank, suit, enh, ed, seal, selected=0, fdown=0, debuff=0):
    return np.concatenate([
        [rank / 14.0, float(rank == 13), float(rank >= 11),
         (11 if rank == 14 else min(rank, 10)) / 11.0],
        _onehot(rank - 2, 13),
        _onehot(suit, 4), _onehot(enh, 9), _onehot(ed, 4), _onehot(seal, 4),
        [float(selected), float(fdown), float(debuff)]]).astype(np.float32)


def hand_feats(hand):
    if not hand:
        return np.zeros((0, F_CARD), np.float32), np.zeros(0, bool)
    rows = [card_feats(h["rank"], h["suit"], h["enhancement"], h["edition"],
                       h["seal"], h["selected"], h["face_down"],
                       h["debuffed"]) for h in hand]
    sel = np.array([h["selected"] for h in hand], dtype=bool)
    return np.stack(rows), sel


def match_feats(hand, sel_mask):
    """Per-card: how many OTHER selected cards share my rank / suit.
    Self-excluded, so already-selected cards don't score their own match."""
    n = len(hand)
    out = np.zeros((n, N_MATCH), np.float32)
    if n == 0 or not sel_mask.any():
        return out
    ranks = np.array([h["rank"] for h in hand])
    suits = np.array([h["suit"] for h in hand])
    selr, sels = ranks[sel_mask], suits[sel_mask]
    for i in range(n):
        out[i, 0] = ((selr == ranks[i]).sum() - sel_mask[i]) / 4.0
        out[i, 1] = ((sels == suits[i]).sum() - sel_mask[i]) / 5.0
        out[i, 2] = sel_mask.sum() / 5.0
    return out


def sel_hand_type(hand):
    """12-way one-hot of the poker hand the CURRENT selection would play,
    + count/5. Advisory (no Four Fingers / Shortcut awareness)."""
    sel = [h for h in hand if h["selected"]]
    v = np.zeros(N_HT + 1, np.float32)
    v[N_HT] = len(sel) / 5.0
    if not sel:
        return v
    ranks = sorted(h["rank"] for h in sel)
    counts = sorted(np.unique(ranks, return_counts=True)[1])[::-1]
    flush = len(sel) == 5 and len({h["suit"] for h in sel}) == 1
    uniq = sorted(set(ranks))
    straight = (len(sel) == 5 and len(uniq) == 5
                and (uniq[4] - uniq[0] == 4 or uniq == [2, 3, 4, 5, 14]))
    if counts[0] == 5:          t = 11 if flush else 9
    elif straight and flush:    t = 8
    elif counts[0] == 4:        t = 7
    elif counts[0] == 3 and len(counts) > 1 and counts[1] == 2:
        t = 10 if flush else 6
    elif flush:                 t = 5
    elif straight:              t = 4
    elif counts[0] == 3:        t = 3
    elif counts[0] == 2 and len(counts) > 1 and counts[1] == 2:
        t = 2
    elif counts[0] == 2:        t = 1
    else:                       t = 0
    v[t] = 1.0
    return v


def deck_feats(deck):
    if len(deck) == 0:
        return np.zeros((0, F_CARD), np.float32)
    return np.stack([card_feats(r, s, e, ed, sl)
                     for r, s, e, ed, sl in deck])


def joker_feats(jokers):
    if not jokers:
        return np.zeros((0, F_JOKER - D_JEMB), np.float32), []
    n = len(jokers)
    rows = [np.concatenate([
        _onehot(j["edition"], 4),
        [min(j["rarity"], 4) / 4.0, min(j["sell_value"], 10) / 10.0,
         i / max(n - 1, 1), float(j["selected"]), float(j["disabled"])]])
        for i, j in enumerate(jokers)]                       # position feat!
    return np.stack(rows).astype(np.float32), [j["index"] for j in jokers]


def cons_feats(cons):
    if not cons:
        return np.zeros((0, F_CONS - D_CEMB), np.float32), []
    rows = [np.concatenate([_onehot(c["kind"], 3), [float(c["selected"])]])
            for c in cons]
    return np.stack(rows).astype(np.float32), [c["index"] for c in cons]


def shop_feats(shop, params):
    if not shop:
        return np.zeros((0, F_SHOP), np.float32)
    rows = []
    for s in shop:
        emb = (params["jemb"][s["index"]] if s["kind"] == 0
               else np.pad(params["cemb"][s["index"]], (0, D_JEMB - D_CEMB))) \
            if s["index"] >= 0 else np.zeros(D_JEMB, np.float32)
        rows.append(np.concatenate([
            [float(s["kind"] == 0), float(s["kind"] != 0),
             min(s["cost"], 15) / 15.0, float(s["sold"])], emb]))
    return np.stack(rows).astype(np.float32)


def pack_feats(packs):
    out = np.zeros(2 * F_PACK, np.float32)
    for i, p in enumerate(packs[:2]):
        out[i * F_PACK:(i + 1) * F_PACK] = np.concatenate([
            _onehot(p["type"], 5), [p["size"] / 2.0,
                                    min(p["cost"], 10) / 10.0, 1.0]])
    return out


def packc_feats(pc, params):
    if not pc:
        return np.zeros((0, F_PACKC), np.float32)
    rows = []
    for x in pc:
        if x["kind"] == 1 and x["index"] >= 0:
            emb = params["jemb"][x["index"]]
        elif x["kind"] == 0 and x["index"] >= 0:
            emb = np.pad(params["cemb"][x["index"]], (0, D_JEMB - D_CEMB))
        else:
            emb = np.zeros(D_JEMB, np.float32)
        rows.append(np.concatenate([
            _onehot(x["kind"], 3), emb,
            [x.get("rank", 0) / 14.0, float(x["taken"])]]))
    return np.stack(rows).astype(np.float32)


# ----------------------------------------------------------------- forward
def _enc(X, W, b):
    return np.tanh(X @ W + b) if len(X) else X.reshape(0, W.shape[1])


def _pool(E, cap):
    if len(E) == 0:
        return np.zeros(E.shape[1] + 1, np.float32)
    return np.concatenate([E.mean(0), [len(E) / cap]]).astype(np.float32)


def _pointer(E, c, s, W1, b1, w2):
    if len(E) == 0:
        return np.zeros(0, np.float32)
    X = np.concatenate([E, np.tile(c, (len(E), 1)),
                        np.tile(s, (len(E), 1))], axis=1)
    return np.tanh(X @ W1 + b1) @ w2


def act(params, g):
    """One forward pass -> a length-58 score vector aligned with legal_mask."""
    hand, jokers, cons = g.hand(), g.jokers(), g.consumables()
    shop, packs, pc = g.shop(), g.packs(), g.pack_contents()
    deck = deck_cards(g)

    HF, sel_mask = hand_feats(hand)
    Eh = _enc(HF, params["card_W"], params["card_b"])
    Es = Eh[sel_mask] if sel_mask.any() else np.zeros((0, E_CARD), np.float32)
    s = Es.mean(0) if len(Es) else np.zeros(E_CARD, np.float32)
    Ed = _enc(deck_feats(deck), params["card_W"], params["card_b"])
    Ehm = (np.concatenate([Eh, match_feats(hand, sel_mask)], axis=1)
           if len(Eh) else np.zeros((0, E_CARD + N_MATCH), np.float32))

    JF, jids = joker_feats(jokers)
    Ej = _enc(np.concatenate([params["jemb"][jids], JF], 1)
              if jids else np.zeros((0, F_JOKER), np.float32),
              params["jok_W"], params["jok_b"])
    CF, cids = cons_feats(cons)
    Ec = _enc(np.concatenate([params["cemb"][cids], CF], 1)
              if cids else np.zeros((0, F_CONS), np.float32),
              params["con_W"], params["con_b"])
    Esh = _enc(shop_feats(shop, params), params["shop_W"], params["shop_b"])
    Epc = _enc(packc_feats(pc, params), params["pkc_W"], params["pkc_b"])
    PF = pack_feats(packs)

    ks, non = king_counts(deck)
    obs = g.obs()
    scal = np.concatenate([
        obs[0:20], obs[210:222],
        ks / 8.0, [ks.max() / 8.0, non / 52.0,
                   len(deck) / 104.0, g.draw_pile / 104.0,
                   np.log1p(max(g.money, 0)) / 5.0],
        sel_hand_type(hand)]).astype(np.float32)

    trunk_in = np.concatenate([
        scal, _pool(Eh, 16), _pool(Es, 5),
        (Ed.mean(0) if len(Ed) else np.zeros(E_CARD, np.float32)),
        _pool(Ej, 10), _pool(Ec, 4), _pool(Esh, 6), PF, _pool(Epc, 5)])
    h = np.tanh(trunk_in @ params["trunk_W"] + params["trunk_b"])
    c = np.tanh(h @ params["ctx_W"] + params["ctx_b"])

    scores = np.full(NUM_ACTIONS, -np.inf, np.float32)

    def put(base, vals, cap):
        for i, v in enumerate(vals[:cap]):
            scores[base + i] = v

    p = params
    put(Action.TOGGLE_CARD,
        _pointer(Ehm, c, s, p["h_card_W1"], p["h_card_b1"], p["h_card_w2"]),
        12)
    put(Action.TOGGLE_JOKER,
        _pointer(Ej, c, s, p["h_jok_W1"], p["h_jok_b1"], p["h_jok_w2"]), 10)
    put(Action.TOGGLE_CONSUMABLE,
        _pointer(Ec, c, s, p["h_con_W1"], p["h_con_b1"], p["h_con_w2"]), 4)
    put(Action.BUY_SLOT,
        _pointer(Esh, c, s, p["h_buy_W1"], p["h_buy_b1"], p["h_buy_w2"]), 6)
    put(Action.BUY_PACK,
        _pointer(PF.reshape(2, F_PACK), c, s,
                 p["h_pack_W1"], p["h_pack_b1"], p["h_pack_w2"]), 2)
    put(Action.PICK_PACK,
        _pointer(Epc, c, s, p["h_pick_W1"], p["h_pick_b1"], p["h_pick_w2"]),
        5)
    single = c @ p["single_W"] + p["single_b"]
    for a, v in zip(SINGLETONS, single):
        scores[a] = v
    return scores


# ----------------------------------------------------------------- rollout
PROGRESS = {Action.PLAY, Action.DISCARD, Action.USE_CONSUMABLE,
            Action.SELL_JOKER, Action.SELL_CONSUMABLE, Action.SELECT_BLIND,
            Action.SKIP_BLIND, Action.CONFIRM_CASHOUT, Action.LEAVE_SHOP,
            Action.REROLL_SHOP, Action.BUY_VOUCHER, Action.REROLL_BOSS,
            Action.SKIP_PACK}
PROGRESS |= {Action.BUY_SLOT + i for i in range(6)}
PROGRESS |= {Action.BUY_PACK + i for i in range(2)}
PROGRESS |= {Action.PICK_PACK + i for i in range(5)}

PROGRESS_MASK = np.zeros(NUM_ACTIONS, bool)
for _a in PROGRESS:
    PROGRESS_MASK[_a] = True

_UNDO = {Action.SHIFT_CARD_LEFT: Action.SHIFT_CARD_RIGHT,
         Action.SHIFT_CARD_RIGHT: Action.SHIFT_CARD_LEFT,
         Action.SHIFT_JOKER_LEFT: Action.SHIFT_JOKER_RIGHT,
         Action.SHIFT_JOKER_RIGHT: Action.SHIFT_JOKER_LEFT}


def rollout(genome, seed=None, max_ante=8, stall_limit=400,
            skip_credit=-0.5, temp=0.5, es=1.0):
    """FIDGET POLICY (four layers, all scaled by `es`; es=0 disables all):

    1. NO-OP DETECTION. obs is compared before/after every step. An action
       that leaves the observation bit-identical did nothing -- the state
       that produced it is unchanged, so a greedy policy would pick it
       again forever. Costs 5x a normal wasted step and forces progress
       immediately. This is the general fix: it catches clear-on-empty,
       shift-at-edge, use-consumable-with-none-selected, and every future
       no-op without naming any of them.

    2. UNDO PENALTY. Reversing the previous action (shift left after
       shift right; re-toggling the card just toggled) is a 2-cycle: two
       steps, no net state change. It stays LEGAL and costs 5x, because
       masking it would freeze the weights that want it -- unable to
       fire, they never cost fitness, so selection never grinds them
       down and they resurface the moment the mask is lifted. Punish,
       never forbid.

    3. FIDGET BUDGET. After `fidget_limit` consecutive non-progress
       actions, the best legal PROGRESS action is forced. At es=1 the
       limit is 6 -- enough to build a 5-card hand, not enough to wander.

    4. TIME TAX. Every non-progress step costs 0.01 (capped at 3.0) in
       fitness, from the FIRST one, with no threshold to hide under. A
       genome that builds its hand in 6 toggles beats an identical one
       that takes 40, in every state, regardless of which action it
       wastes steps on.

    skip_credit: -0.5 punishes skipping (phase 1); 1.0 is real-Balatro
    semantics (phase 2), for once the economy circuit exists.

    temp > 0 softmax-samples actions so a merely-second-best good toggle
    still expresses sometimes; temp=0 is greedy (for evaluation)."""
    params = SPEC.unpack(np.asarray(genome, np.float32))
    g = Balatro(seed)
    arng = np.random.default_rng(sum(ord(ch) for ch in g.seed) * 7919 + 1)

    es = float(np.clip(es, 0.0, 1.0))
    fidget_limit = 6 if es >= 1.0 else int(round(6 + 26 * (1 - es)))
    tax = 0.01 * es
    tax_cap = 3.0 * es

    stall = skipped = extra_cards = wasted = noops = fidget = 0
    prev_a = prev_prev = -1
    while not g.done and stall < stall_limit and g.ante <= max_ante:
        mask = g.legal_mask()
        if not mask.any():
            break

        scores = act(params, g)
        scores[~mask] = -np.inf
        if not np.isfinite(scores.max()):
            break
        if temp > 0:
            z = np.exp((scores - scores.max()) / temp)
            a = int(arng.choice(NUM_ACTIONS, p=z / z.sum()))
        else:
            a = int(scores.argmax())

        if a == Action.PLAY:
            n_sel = int(g.obs()[26:116:8].sum())   # selected flags, 12 slots
            extra_cards += max(n_sel - 1, 0)

        before = g.obs().copy() if es > 0 else None
        ok = g.step(a)
        if ok and a == Action.SKIP_BLIND:
            skipped += 1
        prev_prev, prev_a = prev_a, a

        if a in PROGRESS:
            stall = fidget = 0
            continue

        stall += 1
        wasted += 1
        fidget += 1
        force = fidget >= fidget_limit
        if es > 0 and np.array_equal(before, g.obs()):    # layer 1
            noops += 1
            force = True
        elif es > 0 and (a == _UNDO.get(prev_prev) or a == prev_prev):
            noops += 1                                   # layer 2: 2-cycle
        if force:                                        # layer 3
            pm = mask & PROGRESS_MASK
            if pm.any():
                fs = scores.copy()
                fs[~pm] = -np.inf
                g.step(int(fs.argmax()))
                prev_prev = prev_a = -1
            fidget = 0

    f = fitness(g, cleared=(g.ante > max_ante or g.won),
                skipped=skipped, skip_credit=skip_credit,
                extra_cards=extra_cards,
                waste_cost=min(tax * (wasted + 4 * noops), tax_cap))
    g.close()
    return f


def fitness(g, cleared, skipped=0, skip_credit=-0.5, extra_cards=0,
            waste_cost=0.0):
    """Shaped survival below the bar; king homogeneity + money above it.

    Position = ante*3 + blind counts where you're STANDING; skips advance
    it for free. So pay only for blinds actually CLEARED, then price each
    skip at skip_credit (see rollout docstring for the curriculum).

    extra_cards: 0.02 per card beyond the first per played hand (cap 0.8)
    -- pays the flat part of the gradient, since 2 unpaired cards score
    like 1 and evolution otherwise never explores toward pairs.

    waste_cost: the time tax, already summed and capped by rollout.
    """
    if not cleared:
        position = g.ante * 3 + g.blind
        won_blinds = max(position - skipped, 0)
        margin = float(np.clip((g.score_log2 - g.goal_log2) / 8.0, -1, 1))
        return (won_blinds + skip_credit * skipped + (margin + 1) / 2
                + min(0.02 * extra_cards, 0.8) - waste_cost)
    ks, non = king_counts(deck_cards(g))
    return 25.0 + 0.1 * (3.0 * ks.max() - non + 48.0) + 0.01 * max(g.money, 0)


if __name__ == "__main__":
    rng = np.random.default_rng(0)
    print(f"genome: {GENOME_SIZE:,} params "
          f"(trunk_in={TRUNK_IN}, scalars={N_SCALARS})")
    import time
    for es in (1.0, 0.5, 0.0):
        r = np.random.default_rng(7)
        t = time.time()
        fits = [rollout(random_genome(r), es=es) for _ in range(6)]
        print(f"es={es}: mean fitness {np.mean(fits):5.2f}  "
              f"{(time.time()-t)/6:.2f}s/rollout")
