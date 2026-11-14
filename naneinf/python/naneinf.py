"""
naneinf — Python bindings for the Balatro engine (White Stake, Plasma Deck).

    from naneinf import Balatro, Action

    g = Balatro("QJSKRTWR")        # or Balatro() for a random real seed
    g.step(Action.SELECT_BLIND)
    for i in (2, 3, 4):
        g.step(Action.TOGGLE_CARD + i)
    g.step(Action.PLAY)
    print(g.score_log2, g.legal_actions())

No build step: this loads the prebuilt shared library via ctypes.
"""
from __future__ import annotations

import ctypes
import os
import platform
from typing import List, Optional

try:
    import numpy as np
except ImportError:                                     # numpy is optional
    np = None

# --------------------------------------------------------------------------
# library loading
# --------------------------------------------------------------------------
_HERE = os.path.dirname(os.path.abspath(__file__))
_NAMES = {"Windows": "naneinf.dll", "Darwin": "libnaneinf.dylib"}
_LIBNAME = _NAMES.get(platform.system(), "libnaneinf.so")
_LIBPATH = os.path.join(_HERE, _LIBNAME)
if not os.path.exists(_LIBPATH):
    raise FileNotFoundError(
        f"{_LIBNAME} not found next to naneinf.py.\n"
        "Build it with:\n"
        "  g++ -std=c++20 -O2 -shared -fPIC capi.cpp balatro.cpp blinds.cpp "
        "balatro_rng.cpp hand_eval.cpp jokers/*.cpp consumables/*.cpp "
        f"-o {_LIBNAME}")
_lib = ctypes.CDLL(_LIBPATH)

_i32p = ctypes.POINTER(ctypes.c_int32)
_f32p = ctypes.POINTER(ctypes.c_float)

# Symbols added after the first release. If the loaded library predates
# them, record the name instead of dying at import time — the properties
# that use them raise a message telling you to rebuild.
_OPTIONAL = {"nf_reroll_cost", "nf_free_rerolls", "nf_clone",
             "nf_pack_contents_width",
             "nf_reshuffle_draws", "nf_reseed_rng", "nf_deck",
             "nf_cleared8"}
_MISSING = set()

_REBUILD_HINT = (
    "The loaded {lib} is older than this naneinf.py: it is missing {sym}.\n"
    "Rebuild the library from the w4 directory:\n"
    "  Windows: g++ -std=c++20 -O2 -shared -static -static-libgcc "
    "-static-libstdc++ \\\n"
    "             capi.cpp balatro.cpp blinds.cpp balatro_rng.cpp "
    "hand_eval.cpp \\\n"
    "             jokers/*.cpp consumables/*.cpp -o python/naneinf.dll\n"
    "  Linux:   g++ -std=c++20 -O2 -shared -fPIC "
    "capi.cpp balatro.cpp blinds.cpp \\\n"
    "             balatro_rng.cpp hand_eval.cpp jokers/*.cpp "
    "consumables/*.cpp \\\n"
    "             -o python/libnaneinf.so\n"
    "Mixing a new naneinf.py with an old library also means none of the "
    "engine fixes are present.")


def _sig(name, restype, *argtypes):
    try:
        fn = getattr(_lib, name)
    except AttributeError:
        if name in _OPTIONAL:
            _MISSING.add(name)
            return None
        raise AttributeError(_REBUILD_HINT.format(lib=_LIBNAME, sym=name))
    fn.restype = restype
    fn.argtypes = list(argtypes)
    return fn


def _require(sym):
    raise RuntimeError(_REBUILD_HINT.format(lib=_LIBNAME, sym=sym))

_new            = _sig("nf_new", ctypes.c_void_p, ctypes.c_char_p)
_free           = _sig("nf_free", None, ctypes.c_void_p)
_seed           = _sig("nf_seed", None, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int)
_num_actions    = _sig("nf_num_actions", ctypes.c_int)
_legal          = _sig("nf_legal", None, ctypes.c_void_p, _i32p)
_step           = _sig("nf_step", ctypes.c_int, ctypes.c_void_p, ctypes.c_int)
_obs_size       = _sig("nf_obs_size", ctypes.c_int)
_obs            = _sig("nf_obs", None, ctypes.c_void_p, _f32p)
_clone          = _sig("nf_clone", ctypes.c_void_p, ctypes.c_void_p)
_reshuffle      = _sig("nf_reshuffle_draws", None, ctypes.c_void_p,
                       ctypes.c_uint64)
_reseed         = _sig("nf_reseed_rng", None, ctypes.c_void_p,
                       ctypes.c_uint64)
_deck           = _sig("nf_deck", ctypes.c_int, ctypes.c_void_p, _i32p)
_pc_width       = _sig("nf_pack_contents_width", ctypes.c_int)
# 4 = old layout (rank only); 8 = suit/enhancement/edition/seal exposed
PACK_ROW = _pc_width() if _pc_width else 4
for _n, _r in (("nf_phase", ctypes.c_int), ("nf_ante", ctypes.c_int),
               ("nf_blind", ctypes.c_int), ("nf_boss", ctypes.c_int),
               ("nf_money", ctypes.c_int), ("nf_hands", ctypes.c_int),
               ("nf_discards", ctypes.c_int), ("nf_done", ctypes.c_int),
               ("nf_won", ctypes.c_int), ("nf_naneinf_ante", ctypes.c_int),
               ("nf_voucher", ctypes.c_int), ("nf_skip_tag", ctypes.c_int),
               ("nf_draw_pile", ctypes.c_int), ("nf_deck_size", ctypes.c_int),
               ("nf_reroll_cost", ctypes.c_int),
               ("nf_cleared8", ctypes.c_int),
               ("nf_free_rerolls", ctypes.c_int),
               ("nf_score_log2", ctypes.c_double), ("nf_goal_log2", ctypes.c_double)):
    _sig(_n, _r, ctypes.c_void_p)
for _n in ("nf_hand", "nf_jokers", "nf_consumables", "nf_shop",
           "nf_packs", "nf_pack_contents"):
    _sig(_n, ctypes.c_int, ctypes.c_void_p, _i32p)
_hand_level     = _sig("nf_hand_level", ctypes.c_int, ctypes.c_void_p, ctypes.c_int)
_num_jokers     = _sig("nf_num_jokers", ctypes.c_int)
_num_consum     = _sig("nf_num_consumables", ctypes.c_int)
_joker_name     = _sig("nf_joker_name", ctypes.c_char_p, ctypes.c_int)
_joker_rarity   = _sig("nf_joker_rarity", ctypes.c_int, ctypes.c_int)
_consum_name    = _sig("nf_consumable_name", ctypes.c_char_p, ctypes.c_int)
_voucher_name   = _sig("nf_voucher_name", ctypes.c_char_p, ctypes.c_int)
_tag_name       = _sig("nf_tag_name", ctypes.c_char_p, ctypes.c_int)
_boss_name      = _sig("nf_boss_name", ctypes.c_char_p, ctypes.c_int)
_hand_name      = _sig("nf_hand_name", ctypes.c_char_p, ctypes.c_int)

NUM_ACTIONS = _num_actions()
OBS_SIZE = _obs_size()

# --------------------------------------------------------------------------
# constants (mirror balatro.hpp)
# --------------------------------------------------------------------------
MAX_HAND, MAX_JOKERS, MAX_CONSUMABLES, MAX_SHOP = 12, 10, 4, 6

class Action:
    """Action ids. Indexed families are BASE + index."""
    TOGGLE_CARD       = 0
    TOGGLE_JOKER      = TOGGLE_CARD + MAX_HAND
    TOGGLE_CONSUMABLE = TOGGLE_JOKER + MAX_JOKERS
    SHIFT_CARD_LEFT   = TOGGLE_CONSUMABLE + MAX_CONSUMABLES
    SHIFT_CARD_RIGHT  = SHIFT_CARD_LEFT + 1
    SHIFT_JOKER_LEFT  = SHIFT_CARD_LEFT + 2
    SHIFT_JOKER_RIGHT = SHIFT_CARD_LEFT + 3
    PLAY              = SHIFT_CARD_LEFT + 4
    DISCARD           = SHIFT_CARD_LEFT + 5
    USE_CONSUMABLE    = SHIFT_CARD_LEFT + 6
    SELL_CONSUMABLE   = SHIFT_CARD_LEFT + 7
    SELL_JOKER        = SHIFT_CARD_LEFT + 8
    CONFIRM_CASHOUT   = SHIFT_CARD_LEFT + 9
    CLEAR_SELECTION   = SHIFT_CARD_LEFT + 10
    SELECT_BLIND      = SHIFT_CARD_LEFT + 11
    SKIP_BLIND        = SHIFT_CARD_LEFT + 12
    BUY_SLOT          = SHIFT_CARD_LEFT + 13
    BUY_VOUCHER       = BUY_SLOT + MAX_SHOP
    REROLL_SHOP       = BUY_VOUCHER + 1
    LEAVE_SHOP        = BUY_VOUCHER + 2
    REROLL_BOSS       = BUY_VOUCHER + 3
    HOLD_JOKER        = BUY_VOUCHER + 4
    BUY_PACK          = BUY_VOUCHER + 5
    PICK_PACK         = BUY_PACK + 2
    SKIP_PACK         = PICK_PACK + 5

PHASE_BLIND_SELECT, PHASE_PLAYING, PHASE_CASH_OUT, PHASE_SHOP, \
    PHASE_PACK, PHASE_GAME_OVER = range(6)
PHASE_NAMES = ["blind_select", "playing", "cash_out", "shop", "pack", "game_over"]

SUITS = "shcd"          # spades, hearts, clubs, diamonds
RANKS = {11: "J", 12: "Q", 13: "K", 14: "A"}

def card_str(rank: int, suit: int) -> str:
    if suit < 0:
        return "??"
    return f"{RANKS.get(rank, rank)}{SUITS[suit].upper()}"

# name tables, built once
JOKER_NAMES = [_joker_name(i).decode() for i in range(_num_jokers())]
JOKER_RARITY = [_joker_rarity(i) for i in range(_num_jokers())]
CONSUMABLE_NAMES = [_consum_name(i).decode() for i in range(_num_consum())]
HAND_NAMES = [_hand_name(t).decode() for t in range(12)]

def voucher_name(i: int) -> str: return _voucher_name(i).decode()
def tag_name(i: int) -> str:     return _tag_name(i).decode()
def boss_name(i: int) -> str:    return _boss_name(i).decode() if i >= 0 else ""


class Balatro:
    """One self-contained White Stake / Plasma Deck run."""

    def __init__(self, seed: Optional[str] = None):
        self._h = _new((seed or "").encode())
        self._legal_buf = (ctypes.c_int32 * NUM_ACTIONS)()
        self._obs_buf = (ctypes.c_float * OBS_SIZE)()
        self._int_buf = (ctypes.c_int32 * 1024)()

    def __del__(self):
        h = getattr(self, "_h", None)
        if h:
            _free(h)
            self._h = None

    def close(self):
        self.__del__()

    @classmethod
    def _from_handle(cls, h):
        g = cls.__new__(cls)
        g._h = h
        g._legal_buf = (ctypes.c_int32 * NUM_ACTIONS)()
        g._obs_buf = (ctypes.c_float * OBS_SIZE)()
        g._int_buf = (ctypes.c_int32 * 1024)()
        return g

    def clone(self) -> "Balatro":
        """Exact deep copy (nf_clone). O(state), ~12 us. The copy shares
        the RNG STATE, so its future equals this game's true future --
        fine for evaluating deterministic outcomes of visible choices,
        cheating for anything that draws cards or rolls dice. For rollouts
        use imagine()."""
        if "nf_clone" in _MISSING: _require("nf_clone")
        return Balatro._from_handle(_clone(self._h))

    def imagine(self, seed: int) -> "Balatro":
        """Clone + reshuffle draw pile + reseed the game RNG: a copy whose
        future is a fresh SAMPLE from the distribution, not the run's
        actual future. This is the honest-rollout primitive."""
        g = self.clone()
        _reshuffle(g._h, seed & 0xFFFFFFFFFFFFFFFF)
        _reseed(g._h, (seed * 0x9E3779B97F4A7C15 + 1) & 0xFFFFFFFFFFFFFFFF)
        return g

    def deck(self) -> List[dict]:
        """All live cards in the deck (destroyed excluded)."""
        if "nf_deck" in _MISSING: _require("nf_deck")
        n = _deck(self._h, self._int_buf)
        out = []
        for i in range(n):
            r, su, enh, ed, seal = (self._int_buf[i*5+j] for j in range(5))
            out.append(dict(rank=r, suit=su, enhancement=enh, edition=ed,
                            seal=seal, name=card_str(r, su)))
        return out

    @property
    def deck_kings(self) -> int:
        return sum(1 for c in self.deck() if c["rank"] == 13)

    @property
    def cleared8(self) -> bool:
        """True once ante 8 is beaten -- the run objective."""
        if "nf_cleared8" in _MISSING: _require("nf_cleared8")
        return bool(_lib.nf_cleared8(self._h))

    # ---- identity -------------------------------------------------------
    @property
    def seed(self) -> str:
        buf = ctypes.create_string_buffer(16)
        _seed(self._h, buf, 16)
        return buf.value.decode()

    # ---- gym-ish core ---------------------------------------------------
    def legal_mask(self):
        _legal(self._h, self._legal_buf)
        if np is not None:
            return np.frombuffer(self._legal_buf, dtype=np.int32).astype(bool).copy()
        return [bool(x) for x in self._legal_buf]

    def legal_actions(self) -> List[int]:
        _legal(self._h, self._legal_buf)
        return [a for a in range(NUM_ACTIONS) if self._legal_buf[a]]

    def obs(self):
        _obs(self._h, self._obs_buf)
        if np is not None:
            return np.frombuffer(self._obs_buf, dtype=np.float32).copy()
        return list(self._obs_buf)

    def step(self, action: int) -> bool:
        """Apply an action. Returns False (and changes nothing) if illegal."""
        return bool(_step(self._h, int(action)))

    # ---- scalars --------------------------------------------------------
    @property
    def phase(self) -> int: return _lib.nf_phase(self._h)
    @property
    def phase_name(self) -> str: return PHASE_NAMES[self.phase]
    @property
    def ante(self) -> int: return _lib.nf_ante(self._h)
    @property
    def blind(self) -> int: return _lib.nf_blind(self._h)
    @property
    def boss(self) -> int: return _lib.nf_boss(self._h)
    @property
    def money(self) -> int: return _lib.nf_money(self._h)
    @property
    def hands_left(self) -> int: return _lib.nf_hands(self._h)
    @property
    def discards_left(self) -> int: return _lib.nf_discards(self._h)
    @property
    def score_log2(self) -> float:
        """log2 of this blind's score. naneinf is >= 1024."""
        return _lib.nf_score_log2(self._h)
    @property
    def goal_log2(self) -> float: return _lib.nf_goal_log2(self._h)
    @property
    def done(self) -> bool: return bool(_lib.nf_done(self._h))
    @property
    def won(self) -> bool: return bool(_lib.nf_won(self._h))
    @property
    def naneinf_ante(self) -> int: return _lib.nf_naneinf_ante(self._h)
    @property
    def voucher(self) -> int: return _lib.nf_voucher(self._h)
    @property
    def skip_tag(self) -> int: return _lib.nf_skip_tag(self._h)
    @property
    def draw_pile(self) -> int: return _lib.nf_draw_pile(self._h)
    @property
    def deck_size(self) -> int: return _lib.nf_deck_size(self._h)
    @property
    def reroll_cost(self) -> int:
        """What the next shop reroll costs right now (0 if a Chaos the
        Clown charge is unspent)."""
        if "nf_reroll_cost" in _MISSING:
            _require("nf_reroll_cost")
        return _lib.nf_reroll_cost(self._h)
    @property
    def free_rerolls(self) -> int:
        """Unspent free-reroll charges (Chaos the Clown)."""
        if "nf_free_rerolls" in _MISSING:
            _require("nf_free_rerolls")
        return _lib.nf_free_rerolls(self._h)

    def hand_level(self, hand_type: int) -> int:
        return _hand_level(self._h, hand_type)

    # ---- structured views ----------------------------------------------
    def _rows(self, fn, width):
        n = fn(self._h, self._int_buf)
        return [tuple(self._int_buf[i * width + j] for j in range(width))
                for i in range(n)]

    def hand(self) -> List[dict]:
        out = []
        for cid, rank, suit, enh, ed, seal, flags in self._rows(_lib.nf_hand, 7):
            out.append(dict(id=cid, rank=rank, suit=suit, enhancement=enh,
                            edition=ed, seal=seal,
                            selected=bool(flags & 1), face_down=bool(flags & 2),
                            debuffed=bool(flags & 4),
                            name=card_str(rank, suit)))
        return out

    def jokers(self) -> List[dict]:
        out = []
        for idx, rarity, ed, sell, flags, _ in self._rows(_lib.nf_jokers, 6):
            out.append(dict(index=idx,
                            name=JOKER_NAMES[idx] if idx >= 0 else "???",
                            rarity=rarity, edition=ed, sell_value=sell,
                            selected=bool(flags & 1), face_down=bool(flags & 2),
                            disabled=bool(flags & 4)))
        return out

    def consumables(self) -> List[dict]:
        return [dict(index=i, name=CONSUMABLE_NAMES[i] if i >= 0 else "?",
                     kind=k, selected=bool(s))
                for i, k, s in self._rows(_lib.nf_consumables, 3)]

    def shop(self) -> List[dict]:
        out = []
        for kind, idx, cost, ed, sold in self._rows(_lib.nf_shop, 5):
            name = (JOKER_NAMES[idx] if kind == 0 else CONSUMABLE_NAMES[idx]) \
                   if idx >= 0 else "?"
            out.append(dict(kind=kind, index=idx, name=name, cost=cost,
                            edition=ed, sold=bool(sold)))
        return out

    PACK_TYPES = ["Arcana", "Celestial", "Standard", "Buffoon", "Spectral"]
    PACK_SIZES = ["", "Jumbo", "Mega"]

    def packs(self) -> List[dict]:
        return [dict(type=t, size=s, cost=c,
                     name=f"{self.PACK_TYPES[t]} {self.PACK_SIZES[s]}".strip() + " Pack")
                for t, s, c in self._rows(_lib.nf_packs, 3)]

    def pack_contents(self) -> List[dict]:
        out = []
        for row in self._rows(_lib.nf_pack_contents, PACK_ROW):
            kind, idx, rank, taken = row[0], row[1], row[2], row[3]
            suit, enh, ed, seal = (row[4], row[5], row[6], row[7]) \
                if PACK_ROW >= 8 else (-1, 0, 0, 0)
            if kind == 0:   name = CONSUMABLE_NAMES[idx] if idx >= 0 else "?"
            elif kind == 1: name = JOKER_NAMES[idx] if idx >= 0 else "?"
            else:           name = card_str(rank, suit)
            out.append(dict(kind=kind, index=idx, name=name, rank=rank,
                            suit=suit, enhancement=enh, edition=ed,
                            seal=seal, taken=bool(taken)))
        return out

    # ---- convenience ----------------------------------------------------
    def render(self) -> str:
        L = [f"seed {self.seed} | ante {self.ante} blind {self.blind} "
             f"| {self.phase_name} | ${self.money}"]
        if self.boss >= 0 and self.blind == 2:
            L.append(f"BOSS: {boss_name(self.boss)}")
        L.append(f"score 2^{self.score_log2:.2f} / goal 2^{self.goal_log2:.2f} "
                 f"| hands {self.hands_left} discards {self.discards_left}")
        js = self.jokers()
        if js: L.append("jokers: " + ", ".join(j["name"] for j in js))
        cs = self.consumables()
        if cs: L.append("consumables: " + ", ".join(c["name"] for c in cs))
        if self.phase in (PHASE_PLAYING, PHASE_PACK):
            h = self.hand()
            if h:
                L.append("hand: " + " ".join(
                    ("[" + c["name"] + "]") if c["selected"] else c["name"]
                    for c in h))
        if self.phase == PHASE_SHOP:
            L.append("shop: " + ", ".join(
                f"{s['name']} ${s['cost']}" for s in self.shop() if not s["sold"]))
            L.append("packs: " + ", ".join(p["name"] for p in self.packs()))
            if "nf_reroll_cost" not in _MISSING:
                L.append(f"reroll ${self.reroll_cost}"
                         + (f"  (free x{self.free_rerolls})"
                            if self.free_rerolls else ""))
            if self.voucher: L.append("voucher: " + voucher_name(self.voucher))
        if self.phase == PHASE_PACK:
            L.append("pack: " + ", ".join(
                p["name"] for p in self.pack_contents() if not p["taken"]))
        return "\n".join(L)


def action_name(a: int) -> str:
    A = Action
    if a < A.TOGGLE_JOKER:            return f"toggle_card[{a - A.TOGGLE_CARD}]"
    if a < A.TOGGLE_CONSUMABLE:       return f"toggle_joker[{a - A.TOGGLE_JOKER}]"
    if a < A.SHIFT_CARD_LEFT:         return f"toggle_consumable[{a - A.TOGGLE_CONSUMABLE}]"
    if A.BUY_SLOT <= a < A.BUY_VOUCHER: return f"buy_slot[{a - A.BUY_SLOT}]"
    if A.BUY_PACK <= a < A.PICK_PACK: return f"buy_pack[{a - A.BUY_PACK}]"
    if A.PICK_PACK <= a < A.SKIP_PACK: return f"pick_pack[{a - A.PICK_PACK}]"
    names = {A.SHIFT_CARD_LEFT: "shift_card_left", A.SHIFT_CARD_RIGHT: "shift_card_right",
             A.SHIFT_JOKER_LEFT: "shift_joker_left", A.SHIFT_JOKER_RIGHT: "shift_joker_right",
             A.PLAY: "play", A.DISCARD: "discard", A.USE_CONSUMABLE: "use_consumable",
             A.SELL_CONSUMABLE: "sell_consumable", A.SELL_JOKER: "sell_joker",
             A.CONFIRM_CASHOUT: "confirm_cashout", A.CLEAR_SELECTION: "clear_selection",
             A.SELECT_BLIND: "select_blind", A.SKIP_BLIND: "skip_blind",
             A.BUY_VOUCHER: "buy_voucher", A.REROLL_SHOP: "reroll_shop",
             A.LEAVE_SHOP: "leave_shop", A.REROLL_BOSS: "reroll_boss",
             A.HOLD_JOKER: "hold_joker", A.SKIP_PACK: "skip_pack"}
    return names.get(a, f"action[{a}]")
