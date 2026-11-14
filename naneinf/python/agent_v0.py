"""agent_v0 — iteration-zero search agent for naneinf (White Stake / Plasma).

Architecture per plan:
  * PLAY decisions: exact, by replay-forking the engine and trying candidate
    subsets for real (real jokers, real levels, real boss effects).
  * DISCARD decisions: leak-free heuristic (no lookahead into true shuffle).
  * SHOP / PACK / CONSUMABLES: hand-written king-engine-biased value tables.
No learning. Purpose: establish the mechanical floor + generate first data.
"""
from __future__ import annotations
import itertools, zlib, random, sys, time

import naneinf
from naneinf import Balatro, Action

LIB = naneinf._lib
A = Action
PH_BLIND, PH_PLAY, PH_CASH, PH_SHOP, PH_PACK, PH_OVER = range(6)


def stable_seed(*parts):
    """Deterministic across processes -- Python's hash() on strings is
    randomized per interpreter (PYTHONHASHSEED), which made agent runs
    unreproducible: same game seed, different imagined rollouts, different
    outcome. crc32 of the repr is stable everywhere."""
    return zlib.crc32(repr(parts).encode()) & 0xFFFFFFFF

# ---------------------------------------------------------------- fork ----
class Game:
    """Balatro + action log; fork() = exact copy via deterministic replay."""
    def __init__(self, seed=None, log=None):
        self.g = Balatro(seed)
        self.seed = self.g.seed
        self.log = []
        if log:
            h, st = self.g._h, LIB.nf_step
            for a in log:
                st(h, a)
            self.log = list(log)

    def do(self, a) -> bool:
        ok = self.g.step(a)
        if ok:
            self.log.append(a)
        return ok

    def fork(self) -> "Game":
        if "nf_clone" not in naneinf._MISSING:
            f = Game.__new__(Game)
            f.g = self.g.clone()
            f.seed, f.log = self.seed, list(self.log)
            return f
        return Game(self.seed, self.log)          # old library fallback

    def imagine(self, s) -> "Game":
        f = Game.__new__(Game)
        f.g = self.g.imagine(s)
        f.seed, f.log = self.seed, list(self.log)
        return f

# ------------------------------------------------------- hand heuristics --
def subset_key(cards):
    """Cheap poker strength for shortlisting (engine does exact scoring)."""
    ranks = [c["rank"] for c in cards]
    suits = [c["suit"] for c in cards]
    n = len(cards)
    cnt = {}
    for r in ranks: cnt[r] = cnt.get(r, 0) + 1
    mult = sorted(cnt.values(), reverse=True)
    flush = n == 5 and len(set(suits)) == 1
    rs = sorted(set(ranks))
    straight = n == 5 and len(rs) == 5 and (
        rs[-1] - rs[0] == 4 or rs == [2, 3, 4, 5, 14])
    if flush and mult[0] == 5: t = 11
    elif mult[0] == 5:         t = 9
    elif flush and mult[:2] == [3, 2]: t = 10
    elif flush and straight:   t = 8
    elif mult[0] == 4:         t = 7
    elif mult[:2] == [3, 2]:   t = 6
    elif flush:                t = 5
    elif straight:             t = 4
    elif mult[0] == 3:         t = 3
    elif mult[:2] == [2, 2]:   t = 2
    elif mult[0] == 2:         t = 1
    else:                      t = 0
    return (t, sum(ranks))

def keep_core(hand):
    """Indices worth keeping (pairs+, 4-flushes, big ranks). Leak-free."""
    idx = list(range(len(hand)))
    by_rank, by_suit = {}, {}
    for i in idx:
        by_rank.setdefault(hand[i]["rank"], []).append(i)
        by_suit.setdefault(hand[i]["suit"], []).append(i)
    core = set()
    for r, ii in by_rank.items():
        if len(ii) >= 2: core.update(ii)
    for s, ii in by_suit.items():
        if len(ii) >= 4: core.update(ii)
    for i in idx:                              # steel/gold/glass are assets
        if hand[i]["enhancement"] in (4, 5, 7): core.add(i)
        if hand[i]["rank"] >= 12: core.add(i)  # Q,K,A
    return core

# ------------------------------------------------------------ valuations --
JV = {}
def _jv(v, *names):
    for n in names: JV[n] = v
_jv(100, "Baron"); _jv(92, "Mime"); _jv(120, "Triboulet")
_jv(85, "Brainstorm", "Blueprint"); _jv(60, "Sock and Buskin")
_jv(55, "Shoot the Moon"); _jv(50, "Steel Joker", "DNA", "Photograph")
_jv(60, "The Duo", "The Trio", "The Family", "The Order", "The Tribe",
    "Cavendish", "Ancient Joker", "The Idol", "Card Sharp", "Hologram",
    "Madness", "Obelisk", "Vampire", "Glass Joker", "Steel Joker",
    "Baseball Card", "Campfire", "Constellation", "Throwback",
    "Flower Pot", "Blackboard", "Seeing Double", "Acrobat", "Dusk",
    "Sock and Buskin", "Photograph", "Loyalty Card", "Stuntman")
_jv(45, "Half Joker", "Erosion", "Rocket", "Trading Card")
_jv(40, "Scary Face", "Raised Fist", "Hit the Road", "Bull", "Golden Joker",
    "Constellation", "Astronomer", "Vagabond", "Glass Joker", "Obelisk",
    "Card Sharp", "Supernova", "Green Joker", "Ride the Bus", "Mr. Bones",
    "Troubadour", "Cartomancer", "Perkeo", "Yorick", "Canio", "Chicot")
_jv(35, "Smiley Face", "Banner", "Abstract Joker", "Hiker", "Scholar",
    "Fibonacci", "Hack", "Dusk", "Burglar", "Castle", "Wee Joker",
    "Madness", "Vampire", "Seltzer", "Drivers License", "Mystic Summit",
    "Cloud 9", "Bootstraps", "To the Moon", "Reserved Parking", "Acrobat")
_jv(30, "Pareidolia", "Jolly Joker", "Zany Joker", "Mad Joker", "Sly Joker",
    "Wily Joker", "Clever Joker", "Devious Joker", "Crafty Joker",
    "Walkie Talkie", "Misprint", "Gros Michel", "Even Steven", "Odd Todd",
    "Runner", "Square Joker", "Spare Trousers", "Hanging Chad",
    "Lucky Cat", "Oops! All 6s", "Delayed Gratification", "Popcorn",
    "Throwback", "Blackboard", "Seeing Double", "Flower Pot", "Stuntman",
    "Loyalty Card", "Merry Andy", "Four Fingers", "Joker Stencil")
_jv(22, "Joker", "Greedy Joker", "Lusty Joker", "Wrathful Joker",
    "Gluttonous Joker", "Rough Gem", "Onyx Agate", "Arrowhead",
    "Bloodstone", "Drunkard", "Juggler", "Chaos the Clown", "Ice Cream",
    "Red Card", "Fortune Teller", "Superposition", "Sixth Sense",
    "Ramen", "Faceless Joker", "Business Card",
    "Hallucination", "Certificate", "Shortcut", "Smeared Joker",
    "Vagabond", "Egg", "Burnt Joker", "Matador")
_jv(12, "Splash", "Credit Card", "To Do List", "8 Ball", "Mail-In Rebate",
    "Flash Card", "Golden Ticket", "Midas Mask", "Turtle Bean",
    "Diet Cola", "Luchador", "Invisible Joker", "Showman", "Swashbuckler",
    "Séance", "Gift Card", "Ceremonial Dagger", "Stone Joker")
_jv(4, "Marble Joker")          # stone cards dilute king fitness
ENGINE = {"Baron", "Mime", "Brainstorm", "Blueprint", "Triboulet"}

CV = {"Strength": 90, "Death": 95, "Cryptid": 90, "The Hanged Man": 70,
      "Immolate": 75, "The Chariot": 65, "Deja Vu": 45, "The Devil": 40,
      "Justice": 30, "The Empress": 35, "The Hierophant": 35,
      "The Magician": 25, "The Tower": 15, "The Hermit": 55,
      "The Emperor": 50, "The High Priestess": 40, "Judgement": 55,
      "Temperance": 30, "The Fool": 20, "The Wheel of Fortune": 15,
      "Familiar": 40, "Grim": 35, "Incantation": 25, "Ankh": 30,
      "Ectoplasm": 25, "Aura": 30, "Talisman": 20, "Medium": 15,
      "Trance": 15, "Sigil": 5, "Ouija": 5, "Hex": 20, "Wraith": 15,
      "The Soul": 100, "Black Hole": 80}
for p in ("Pluto", "Mercury", "Uranus", "Venus", "Saturn", "Jupiter",
          "Earth", "Mars", "Neptune", "Planet X", "Ceres", "Eris"):
    CV[p] = 32

def money_floor(ante):
    return 0 if ante <= 1 else min(25, 5 * (ante - 1))

# ------------------------------------------------------------------ agent -
class Agent:
    def __init__(self, seed=None, log_fn=None):
        self.G = Game(seed)
        self.say = log_fn or (lambda *a: None)
        self.rng = random.Random(self.G.seed)
        self._init_fields()

    def _init_fields(self):
        """Runtime state shared by __init__ AND __new__-style pilot
        construction (agent_v2._pilot). Add new agent fields HERE ONLY —
        fields added ad hoc to __init__ will be missing on pilots."""
        self.kings = 4
        self.epsilon = 0.0        # exploration rate for shop purchases
        self.dead = False         # set when the round is unresolvable
        self.play_counts = [0] * 12   # my subset_key type indices
        # Ante 8 is a CHECKPOINT, not the goal: the real target is naneinf
        # (2^1024) in the ante 26-31 window. Set to 8 for fast iteration,
        # 31+ for real runs.
        self.stop_ante = 8
        self.V = None             # optional value function (agent_v1)
        # Forced skip exploration (collection only). Every run before
        # gen 7 took SELECT_BLIND unconditionally, so the value net has
        # never seen a post-skip state and skip tags were 100% declined.
        # This injects skips so the data exists to learn from.
        self.skip_explore = 0.0

    def blind_select(self):
        """Play the blind. Overridden by RolloutAgent to MEASURE the
        play-vs-skip choice; here it only ever skips for exploration."""
        if self.skip_explore and self.G.g.legal_mask()[A.SKIP_BLIND] \
                and self.rng.random() < self.skip_explore:
            if self.G.do(A.SKIP_BLIND):
                return
        self.G.do(A.SELECT_BLIND)

    # ---------- exact play evaluation via forks ----------
    def eval_play(self, sel_idx):
        f = self.G.fork()
        for i in sel_idx:
            if not f.do(A.TOGGLE_CARD + i): return None
        before_m = f.g.money
        before_s = 2.0 ** f.g.score_log2
        if not f.do(A.PLAY): return None
        cleared = f.g.phase != PH_PLAY
        gain = 2.0 ** f.g.score_log2 - before_s
        return dict(cleared=cleared, gain=gain, dm=f.g.money - before_m,
                    hands=f.g.hands_left)

    def _best_gain_on(self, game_obj, top=12):
        g2 = game_obj.g
        hand = g2.hand()
        pool = [i for i, c in enumerate(hand) if not c["face_down"]] \
               or list(range(len(hand)))
        subs = []
        for k in range(1, 6):
            subs += list(itertools.combinations(pool, k))
        subs.sort(key=lambda s: subset_key([hand[i] for i in s]),
                  reverse=True)
        best = 0.0
        for s in subs[:top]:
            f = game_obj.fork()
            ok = all(f.do(A.TOGGLE_CARD + i) for i in s)
            before = 2.0 ** f.g.score_log2
            if ok and f.do(A.PLAY):
                best = max(best, 2.0 ** f.g.score_log2 - before)
        return best

    def best_play(self, full=False):
        g = self.G.g
        hand = g.hand()
        vis = [i for i, c in enumerate(hand) if not c["face_down"]]
        pool = vis if len(vis) >= 5 else list(range(len(hand)))
        subs = []
        for k in range(1, 6):
            subs += list(itertools.combinations(pool, k))
        subs.sort(key=lambda s: subset_key([hand[i] for i in s
                                            if not hand[i]["face_down"]] or
                                           [hand[s[0]]]), reverse=True)
        cand = subs if full else subs[:40]
        best, bev = None, None
        for s in cand:
            ev = self.eval_play(s)
            if ev is None: continue
            key = (ev["cleared"], ev["gain"] + 0.01 * ev["dm"])
            if bev is None or key > (bev["cleared"],
                                     bev["gain"] + 0.01 * bev["dm"]):
                best, bev = s, ev
        return best, bev

    # ---------- consumable lines (king-maxxing) ----------
    def try_consumables(self):
        g = self.G.g
        if g.phase != PH_PLAY: return False
        hand = g.hand()
        kings = [i for i, c in enumerate(hand)
                 if c["rank"] == 13 and not c["face_down"]]
        queens = [i for i, c in enumerate(hand)
                  if c["rank"] == 12 and not c["face_down"]]
        junk = sorted((i for i, c in enumerate(hand)
                       if c["rank"] < 11 and not c["face_down"]
                       and c["enhancement"] not in (4, 5, 7)),
                      key=lambda i: hand[i]["rank"])
        for ci, c in enumerate(g.consumables()):
            n = c["name"]; sel = None
            if n == "Strength":
                # priority: walk red-seal / enhanced cards up toward King.
                # Plain queens are the fallback, not the target.
                prio = sorted(
                    (i for i, c in enumerate(hand)
                     if not c["face_down"] and c["rank"] < 13
                     and (c["seal"] == 1 or c["enhancement"] in (5, 7))),
                    key=lambda i: -hand[i]["rank"])
                if prio:
                    sel = prio[:2]
                    dk = sum(1 for i in sel if hand[i]["rank"] == 12)
                elif queens:
                    sel = queens[:2]; dk = len(sel)
                else:
                    continue
            elif n == "Death" and kings and junk:
                # Death is *left = *right: the RIGHT card is cloned onto the
                # left. Pick the best possible source -- red seal + gold/steel
                # king if one exists -- so every Death compounds the engine.
                def kq(i):
                    c = hand[i]
                    return ((c["seal"] == 1) * 2 +
                            (c["enhancement"] in (5, 7)) * 1)
                src = max(kings, key=kq)
                jj = [j for j in junk if j < src]
                if not jj: continue
                sel = [jj[0], src]; dk = 1
            elif n == "Cryptid" and kings: sel = [kings[0]]; dk = 2
            elif n == "The Hanged Man" and len(junk) >= 1:
                sel = junk[:2]; dk = 0
            elif n == "Immolate" and len(junk) >= 5: sel = junk[:5]; dk = 0
            elif n in ("The Chariot", "Deja Vu", "The Devil") and kings:
                sel = [kings[0]]; dk = 0
            elif n in ("The Empress", "The Hierophant", "Justice",
                       "The Magician") and (kings or queens):
                sel = [(kings or queens)[0]]; dk = 0
            elif n == "Familiar" and len(junk) >= 2: sel = []; dk = 0
            elif CV.get(n, 0) == 32:
                # planets: LEVEL WHAT WE PLAY. 4.2 levels/run sprayed over
                # nine hand types is a rounding error; the same levels on
                # one type is a doubling. Hold off-focus planets unless
                # consumable slots are tight.
                PLANET_T = {"Pluto": 0, "Mercury": 1, "Uranus": 2,
                            "Venus": 3, "Saturn": 4, "Jupiter": 5,
                            "Earth": 6, "Mars": 7, "Neptune": 8,
                            "Planet X": 9, "Ceres": 10, "Eris": 11}
                t_ = PLANET_T.get(n, -1)
                focus = max(range(12), key=lambda i: self.play_counts[i]) \
                    if any(self.play_counts) else 1          # default Pair
                slots_tight = len(g.consumables()) >= 2
                if t_ == focus or slots_tight:
                    sel = []; dk = 0
                else:
                    continue                     # hold it
            elif n in ("The Emperor", "The High Priestess", "Judgement",
                       "Black Hole", "The Soul"):
                sel = []; dk = 0
            else:
                continue
            f_ok = self._use(ci, sel or [])
            if f_ok:
                self.kings += dk
                return True
        return False

    def _use(self, ci, card_sel):
        G = self.G
        G.do(A.CLEAR_SELECTION)
        for i in card_sel:
            if not G.do(A.TOGGLE_CARD + i):
                G.do(A.CLEAR_SELECTION); return False
        if not G.do(A.TOGGLE_CONSUMABLE + ci):
            G.do(A.CLEAR_SELECTION); return False
        ok = G.do(A.USE_CONSUMABLE)
        G.do(A.CLEAR_SELECTION)
        return ok

    # ---------- phase handlers ----------
    def playing(self):
        while self.try_consumables():
            if self.G.g.phase != PH_PLAY: return
        g = self.G.g
        best, ev = self.best_play()
        if best is None:
            # shortlist may have cut the only LEGAL plays (boss locks like
            # The Mouth restrict by hand type): retry over ALL subsets
            best, ev = self.best_play(full=True)
        if best is None:
            # no legal play exists at all. Discard if we can; otherwise the
            # round is unwinnable and unresolvable (engine softlock -- e.g.
            # The Mouth with an unformable locked type and 0 discards).
            if g.discards_left > 0:
                hand = g.hand()
                core = keep_core(hand)
                toss = [i for i in range(len(hand)) if i not in core
                        and not hand[i]["face_down"]] or [0]
                self.G.do(A.CLEAR_SELECTION)
                for i in toss[:5]: self.G.do(A.TOGGLE_CARD + i)
                if self.G.do(A.DISCARD): return
                self.G.do(A.CLEAR_SELECTION)
            self.dead = True
            return
        need = 2.0 ** g.goal_log2 - 2.0 ** g.score_log2
        weak = (not ev["cleared"] and ev["gain"] * max(1, g.hands_left) <
                need)
        desperate = (not ev["cleared"] and g.hands_left <= 1)
        if (weak or desperate) and g.discards_left > 0:
            hand = g.hand()
            core = keep_core(hand)
            toss = [i for i in range(len(hand)) if i not in core
                    and not hand[i]["face_down"]]
            toss.sort(key=lambda i: hand[i]["rank"])
            toss = toss[:5]
            if toss and not desperate and \
                    "nf_clone" not in naneinf._MISSING:
                # honest lookahead: does discarding beat playing, in
                # expectation over fresh shuffles of the KNOWN remainder?
                gains = []
                for k in range(3):
                    im = self.G.imagine(stable_seed(self.G.seed,
                                              len(self.G.log), k))
                    im.do(A.CLEAR_SELECTION)
                    for i in toss: im.do(A.TOGGLE_CARD + i)
                    if not im.do(A.DISCARD):
                        gains = None; break
                    gains.append(self._best_gain_on(im))
                if gains is not None and gains and \
                        sum(gains) / len(gains) <= ev["gain"] * 1.1:
                    toss = []           # playing now is better in expectation
            if toss:
                self.G.do(A.CLEAR_SELECTION)
                for i in toss: self.G.do(A.TOGGLE_CARD + i)
                if self.G.do(A.DISCARD): return
                self.G.do(A.CLEAR_SELECTION)
        self.G.do(A.CLEAR_SELECTION)
        for i in best: self.G.do(A.TOGGLE_CARD + i)
        played_type = subset_key([hand2[i] for i in best])[0] \
            if (hand2 := g.hand()) and all(i < len(hand2) for i in best) \
            else None
        if self.G.do(A.PLAY):
            if played_type is not None:
                self.play_counts[played_type] += 1
        else:
            self.random_legal()

    def shop(self):
        g, G = self.G.g, self.G
        floor = money_floor(g.ante)
        acted = True
        rerolls = 0
        while acted and g.phase == PH_SHOP:
            acted = False
            items = g.shop()
            # jokers/consumables on the board
            best_i, best_v = -1, 0
            held = len(g.jokers())
            for si, it in enumerate(items):
                if it["sold"]: continue
                v = (JV.get(it["name"], 15) if it["kind"] == 0
                     else CV.get(it["name"], 20))
                v = v - it["cost"]
                afford = (g.money - it["cost"] >= floor or
                          (it["name"] in ENGINE and g.money >= it["cost"]))
                if it["kind"] == 0 and held >= 5: v -= 25
                if afford and v > best_v:
                    best_i, best_v = si, v
            # exploration: sometimes buy something the dict dislikes, so
            # its outcome statistics exist at all (see README step 2)
            if self.epsilon and self.rng.random() < self.epsilon:
                opts = [si for si, it in enumerate(items)
                        if not it["sold"] and g.money - it["cost"] >= 0]
                if opts:
                    si = self.rng.choice(opts)
                    if G.do(A.BUY_SLOT + si):
                        acted = True; continue
            if best_i >= 0 and G.do(A.BUY_SLOT + best_i):
                acted = True; continue
            # packs: at most one per shop, engine-y types first
            for pi, p in enumerate(g.packs()):
                # ALL pack types. Standard (type 2) was excluded here and it
                # is the ONLY source of sealed/enhanced playing cards -- the
                # seed card the whole red-seal-king plan depends on.
                want = True
                if want and g.money - p["cost"] >= floor:
                    if G.do(A.BUY_PACK + pi):
                        acted = True
                        self.pack()
                        break
            if acted: continue
            if (rerolls < 2 and g.money - g.reroll_cost >= floor + 8):
                if G.do(A.REROLL_SHOP):
                    rerolls += 1; acted = True; continue
        G.do(A.LEAVE_SHOP)

    def _pack_rows(self):
        """Pack rows straight from the binding. (This used to hand-parse the
        int buffer at a hardcoded stride of 4 because the old C API exposed
        only `rank`; nf_pack_contents now returns 8 fields including seal
        and enhancement, so the workaround is gone -- it would silently
        mis-stride and drop exactly the seal data the deck plan needs.)"""
        return self.G.g.pack_contents()

    def pack(self):
        g, G = self.G.g, self.G
        while g.phase == PH_PACK:
            cont = self._pack_rows()
            best_i, best_v = -1, 8
            for i, pc in enumerate(cont):
                if pc["taken"]: continue
                if pc["kind"] == 0: v = CV.get(pc["name"], 20)
                elif pc["kind"] == 1: v = JV.get(pc["name"], 15)
                else:
                    # Playing cards are valued by what they can BECOME, not
                    # what they are. A red seal is the scarce seed (~5% of
                    # offered cards) and rank is cheap to fix with Strength;
                    # Death then clones the finished card onto junk.
                    v = 2.0
                    r = pc["rank"]
                    if r == 13:   v = 55           # king
                    elif r == 12: v = 22           # queen: 1 Strength away
                    elif r >= 10: v = 10           # walkable
                    if pc["seal"] == 1:  v += 90   # RED: the seed card
                    if pc["enhancement"] == 7: v += 35   # gold (money engine)
                    if pc["enhancement"] == 5: v += 30   # steel (Baron pair)
                    if pc["enhancement"] == 6: v -= 40   # stone: no seal, no rank
                    if pc["edition"] == 4: v += 25       # negative
                    # every card taken is +1 to N and the fitness is /N^2:
                    # charge for deck growth, scaled by how bloated we are.
                    # Seeds stay worth taking; junk becomes untakeable.
                    seed_card = (pc["seal"] == 1 or pc["rank"] == 13
                                 or pc["enhancement"] in (5, 7))
                    if not seed_card:
                        v -= 1.2 * max(0, g.deck_size - 46)
                if v > best_v: best_i, best_v = i, v
            if best_i >= 0 and G.do(A.PICK_PACK + best_i):
                pc = cont[best_i]
                if pc["kind"] == 2 and pc["rank"] == 13: self.kings += 1
                continue
            if not G.do(A.SKIP_PACK):
                self.random_legal()

    def cash_out(self):
        g = self.G.g
        for ci, c in enumerate(g.consumables()):
            if c["name"] == "The Hermit" and 8 <= g.money <= 20:
                self._use(ci, []); break
        self.G.do(A.CONFIRM_CASHOUT)

    def random_legal(self):
        acts = self.G.g.legal_actions()
        if acts: self.G.do(self.rng.choice(acts))

    # ---------- main loop ----------
    def run(self, max_steps=25000, max_secs=240):
        g = self.G.g
        t0 = time.time()
        last_len = -1; stuck = 0
        while not g.done and not self.dead:
            if g.ante > self.stop_ante:
                break                      # checkpoint reached
            if len(self.G.log) >= max_steps or time.time() - t0 > max_secs:
                break
            if len(self.G.log) == last_len:
                stuck += 1
                if stuck > 4: self.random_legal()
            else:
                stuck = 0
            last_len = len(self.G.log)
            p = g.phase
            if   p == PH_BLIND: self.blind_select()
            elif p == PH_PLAY:  self.playing()
            elif p == PH_CASH:  self.cash_out()
            elif p == PH_SHOP:  self.shop()
            elif p == PH_PACK:  self.pack()
            else: break
        N = g.deck_size
        try:
            K = g.deck_kings
        except Exception:
            K = min(self.kings, N)
        win = (g.won or g.ante > 8) and not self.dead
        fit = K * (100.0 / N) ** 2 if (win and N) else 0.0
        return dict(seed=self.G.seed, won=win, naneinf=g.won,
                    softlock=self.dead, ante=g.ante,
                    money=g.money, deck=N, kings=K, fitness=round(fit, 2),
                    steps=len(self.G.log), secs=round(time.time() - t0, 1),
                    jokers=[j["name"] for j in g.jokers()])

if __name__ == "__main__":
    seed = sys.argv[1] if len(sys.argv) > 1 else None
    print(Agent(seed).run())
