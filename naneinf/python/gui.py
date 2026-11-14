"""
gui — pygame front end for the naneinf Balatro engine.

    python3 gui.py                     # random real seed
    python3 gui.py --seed QJSKRTWR
    python3 gui.py --log-obs           # embed 222-float obs in each log line
    python3 gui.py --runs-dir my_runs

Every run is logged to runs/*.jsonl automatically (see runlog.py). The log
stores (seed + accepted actions); `runlog.py export` turns a log into
(obs, legal_mask, action) numpy arrays for training.

Controls (buttons work too):
  click card/joker/consumable   toggle select
  1..9,0,-,=                    toggle hand cards 1..12
  Enter / P   play              D           discard
  C           clear selection   arrows      shift selected card
  S           select blind      K           skip blind (take tag)
  Space       confirm cash out / leave shop / skip pack
  R           reroll shop       Esc         quit (log kept)
"""
from __future__ import annotations

import argparse
import math
import os
import sys
from collections import deque

import pygame

from naneinf import (Balatro, Action, action_name, boss_name, voucher_name,
                     tag_name, HAND_NAMES, JOKER_RARITY,
                     PHASE_BLIND_SELECT, PHASE_PLAYING, PHASE_CASH_OUT,
                     PHASE_SHOP, PHASE_PACK, PHASE_GAME_OVER)
from runlog import RunLogger

# --------------------------------------------------------------------------
# look & feel — drawn from the engine's own vocabulary (felt table, chips,
# mult, money, "naneinf" = 2^1024). No game assets, everything programmatic.
# --------------------------------------------------------------------------
W, H = 1280, 800
SIDEBAR_W, LOGPANEL_W = 252, 260
CENTER_X0, CENTER_X1 = SIDEBAR_W + 8, W - LOGPANEL_W - 8

FELT      = (28, 44, 54)      # table
FELT_DARK = (20, 32, 40)      # panels
LINE      = (52, 74, 88)
INK       = (232, 228, 218)   # main text
INK_DIM   = (150, 162, 170)
CARD_FACE = (243, 238, 228)
CARD_BACK = (52, 78, 108)
CHIPS     = (74, 156, 255)    # blue — also the Play colour
MULT      = (226, 78, 78)     # red — also the Discard colour
MONEY     = (240, 196, 84)
OK_GREEN  = (94, 190 ,120)
SEL_GLOW  = (255, 214, 120)

SUIT_COL  = {0: (46, 48, 60), 1: (198, 64, 64),      # spade, heart
             2: (52, 96, 66), 3: (206, 126, 50)}     # club, diamond (4-colour)
SUIT_GLYPH = {0: "\u2660", 1: "\u2665", 2: "\u2663", 3: "\u2666"}
SUIT_LETTER = {0: "S", 1: "H", 2: "C", 3: "D"}
RARITY_COL = {1: (86, 152, 210), 2: (86, 186, 120),
              3: (218, 92, 92), 4: (170, 110, 220)}
ENH_TXT  = ["", "BONUS", "MULT", "WILD", "GLASS", "STEEL", "STONE", "GOLD", "LUCKY"]
ED_TXT   = ["", "FOIL", "HOLO", "POLY", "NEG"]
SEAL_COL = {1: (210, 70, 70), 2: (70, 120, 220), 3: (220, 180, 70), 4: (150, 90, 190)}
RANK_TXT = {11: "J", 12: "Q", 13: "K", 14: "A"}


def fmt_score(log2v: float) -> str:
    """The engine speaks in log2; show a number while it fits, then 10^x."""
    if log2v <= 0:
        return "0"
    if log2v < 40:
        return f"{int(round(2 ** log2v)):,}"
    return f"10^{log2v * 0.301029995664:.1f}"


class Btn:
    def __init__(self, rect, label, act, color=CHIPS, enabled=True, small=False):
        self.rect = pygame.Rect(rect)
        self.label, self.act = label, act        # act: action id or callable
        self.color, self.enabled, self.small = color, enabled, small


class App:
    def __init__(self, seed, runs_dir, log_obs):
        pygame.init()
        pygame.display.set_caption("naneinf — Balatro engine")
        self.screen = pygame.display.set_mode((W, H))
        self.clock = pygame.time.Clock()
        self.f_big  = self._font(30, bold=True)
        self.f_med  = self._font(20, bold=True)
        self.f_txt  = self._font(16)
        self.f_sm   = self._font(13)
        self.f_tiny = self._font(11)
        # do rendered suit glyphs actually exist in this font?
        probe = self.f_med.render(SUIT_GLYPH[0], True, INK)
        self.glyphs_ok = probe.get_bounding_rect().w > 2
        self.runs_dir, self.log_obs = runs_dir, log_obs
        self.msgs = deque(maxlen=400)
        self.buttons: list[Btn] = []
        self.flash = ""                       # transient "illegal" notice
        self.flash_t = 0
        self.new_run(seed)

    def _font(self, size, bold=False):
        return pygame.font.SysFont("dejavusans,verdana,arial", size, bold=bold)

    # ---- run lifecycle ---------------------------------------------------
    def new_run(self, seed=None):
        self.g = Balatro(seed)
        self.log = RunLogger(self.g.seed, self.runs_dir, self.log_obs)
        self.prev = dict(phase=self.g.phase, money=self.g.money,
                         ante=self.g.ante, blind=self.g.blind)
        self.say(f"— new run, seed {self.g.seed} → {os.path.basename(self.log.path)}")

    def finish_run(self):
        if not getattr(self.log, "_f", None) or self.log._f.closed:
            return
        if self.g.done:
            self.log.end(self.g)
        else:
            self.log.abandon()

    def say(self, text):
        self.msgs.append(text)

    # ---- the single choke point every input goes through -----------------
    def act(self, a: int):
        g = self.g
        if g.done:
            return
        obs = g.obs() if self.log_obs else None
        self.log_pending = (a, obs)
        before = dict(phase=g.phase, money=g.money, ante=g.ante,
                      blind=g.blind, score=g.score_log2)
        # log captures the pre-action state, so write BEFORE stepping
        if not g.legal_mask()[a]:
            self.flash, self.flash_t = f"illegal: {action_name(a)}", 90
            return
        self.log.step(g, a, obs)
        g.step(a)
        self._narrate(before, a)
        if g.done:
            self.log.note("run over", won=g.won, ante=g.ante)
            self.log.end(g)

    def _narrate(self, before, a):
        g = self.g
        name = action_name(a)
        if name == "play":
            self.say(f"play → {fmt_score(g.score_log2)} / {fmt_score(g.goal_log2)}")
        elif name == "discard":
            self.say("discard")
        elif not name.startswith("toggle"):
            self.say(name)
        d = g.money - before["money"]
        if d:
            self.say(f"  {'+' if d > 0 else ''}{d}$ → ${g.money}")
        if g.phase == PHASE_CASH_OUT and before["phase"] == PHASE_PLAYING:
            bn = ["small blind", "big blind", "BOSS"][before["blind"]]
            self.say(f"BLIND BEATEN ({bn}, ante {g.ante})")
            self.log.note(f"blind beaten ante {g.ante} blind {before['blind']}")
        if g.ante != before["ante"]:
            self.say(f"=== ANTE {g.ante} ===")
            self.log.note(f"ante {g.ante}")
        if g.done:
            self.say("*** NANEINF — RUN WON ***" if g.won else "run lost")

    # ---- input -----------------------------------------------------------
    KEYMAP_CARDS = [pygame.K_1, pygame.K_2, pygame.K_3, pygame.K_4, pygame.K_5,
                    pygame.K_6, pygame.K_7, pygame.K_8, pygame.K_9, pygame.K_0,
                    pygame.K_MINUS, pygame.K_EQUALS]

    def on_key(self, k):
        g = self.g
        if g.done:
            if k == pygame.K_n: self.finish_run(); self.new_run()
            if k == pygame.K_s: self.finish_run(); self.new_run(g.seed)
            return
        if k in self.KEYMAP_CARDS:
            self.act(Action.TOGGLE_CARD + self.KEYMAP_CARDS.index(k)); return
        table = {pygame.K_RETURN: Action.PLAY, pygame.K_p: Action.PLAY,
                 pygame.K_d: Action.DISCARD, pygame.K_c: Action.CLEAR_SELECTION,
                 pygame.K_s: Action.SELECT_BLIND, pygame.K_k: Action.SKIP_BLIND,
                 pygame.K_r: Action.REROLL_SHOP,
                 pygame.K_LEFT: Action.SHIFT_CARD_LEFT,
                 pygame.K_RIGHT: Action.SHIFT_CARD_RIGHT}
        if k in table:
            self.act(table[k]); return
        if k == pygame.K_SPACE:
            for a in (Action.CONFIRM_CASHOUT, Action.LEAVE_SHOP, Action.SKIP_PACK):
                if self.g.legal_mask()[a]:
                    self.act(a); return

    def on_click(self, pos):
        for b in self.buttons:
            if b.enabled and b.rect.collidepoint(pos):
                if callable(b.act): b.act()
                else: self.act(b.act)
                return

    # ---- drawing helpers -------------------------------------------------
    def text(self, s, font, color, x, y, center=False, right=False):
        surf = font.render(s, True, color)
        r = surf.get_rect()
        if center: r.center = (x, y)
        elif right: r.topright = (x, y)
        else: r.topleft = (x, y)
        self.screen.blit(surf, r)
        return r

    def plate(self, rect, fill, border=LINE, radius=8, bw=2):
        pygame.draw.rect(self.screen, fill, rect, border_radius=radius)
        pygame.draw.rect(self.screen, border, rect, bw, border_radius=radius)

    def button(self, b: Btn):
        col = b.color if b.enabled else (60, 72, 80)
        self.plate(b.rect, col, border=(0, 0, 0), radius=10, bw=2)
        f = self.f_sm if b.small else self.f_txt
        self.text(b.label, f, (18, 22, 26) if b.enabled else (110, 120, 126),
                  b.rect.centerx, b.rect.centery, center=True)

    def suit_text(self, suit):
        return SUIT_GLYPH[suit] if self.glyphs_ok else SUIT_LETTER[suit]

    # ---- board regions ---------------------------------------------------
    def draw_sidebar(self):
        g = self.g
        r = pygame.Rect(6, 6, SIDEBAR_W - 6, H - 12)
        self.plate(r, FELT_DARK)
        x, y = 20, 18
        blname = ["SMALL BLIND", "BIG BLIND", "BOSS"][g.blind] \
            if 0 <= g.blind <= 2 else "?"
        self.text(f"ANTE {g.ante}", self.f_big, INK, x, y); y += 40
        self.text(blname, self.f_med,
                  MULT if g.blind == 2 else INK_DIM, x, y); y += 26
        if g.blind == 2 and g.boss >= 0:
            self.text(boss_name(g.boss), self.f_txt, MULT, x, y); y += 24
        y += 8
        # signature element: the naneinf meter — a log2-scaled climb to the
        # blind goal, ticked in powers of two (this engine's native unit).
        self.text("SCORE", self.f_sm, INK_DIM, x, y); y += 16
        self.text(fmt_score(g.score_log2), self.f_med, CHIPS, x, y); y += 26
        self.text(f"goal  {fmt_score(g.goal_log2)}", self.f_txt, INK_DIM, x, y)
        y += 26
        bar = pygame.Rect(x, y, SIDEBAR_W - 46, 18)
        self.plate(bar, (16, 24, 30), radius=6, bw=1)
        frac = 0.0 if g.goal_log2 <= 0 else min(1.0, g.score_log2 / g.goal_log2)
        if frac > 0:
            fill = pygame.Rect(bar.x + 2, bar.y + 2,
                               int((bar.w - 4) * frac), bar.h - 4)
            pygame.draw.rect(self.screen, CHIPS if frac < 1 else OK_GREEN,
                             fill, border_radius=5)
        self.text(f"2^{g.score_log2:.1f} / 2^{g.goal_log2:.1f}",
                  self.f_tiny, INK_DIM, x, y + 22)
        y += 46
        for label, val, col in ((f"hands", g.hands_left, CHIPS),
                                (f"discards", g.discards_left, MULT),
                                (f"money", f"${g.money}", MONEY)):
            self.text(label, self.f_txt, INK_DIM, x, y)
            self.text(str(val), self.f_med, col, SIDEBAR_W - 26, y - 2,
                      right=True)
            y += 30
        y += 6
        self.text(f"deck {g.draw_pile}/{g.deck_size}", self.f_sm, INK_DIM, x, y)
        y += 20
        if g.skip_tag:
            self.text("tag: " + tag_name(g.skip_tag), self.f_sm, MONEY, x, y)
            y += 20
        y += 8
        self.text("HAND LEVELS", self.f_sm, INK_DIM, x, y); y += 18
        for t in range(12):
            lv = g.hand_level(t)
            if lv > 1 or t < 9:
                col = OK_GREEN if lv > 1 else INK_DIM
                self.text(f"{HAND_NAMES[t]:<15} {lv}", self.f_tiny, col, x, y)
                y += 14
        self.text(f"seed {g.seed}", self.f_sm, INK_DIM, x, H - 34)

    def draw_card(self, rect, c, hover=False):
        sel = c["selected"]
        r = rect.move(0, -14) if sel else rect
        if c["face_down"]:
            self.plate(r, CARD_BACK, radius=8)
            for gy in range(r.y + 8, r.bottom - 8, 10):
                pygame.draw.line(self.screen, (72, 100, 134),
                                 (r.x + 6, gy), (r.right - 6, gy + 6))
            self.text("?", self.f_med, (150, 176, 205),
                      r.centerx, r.centery, center=True)
            return r
        self.plate(r, CARD_FACE,
                   border=SEL_GLOW if sel else (30, 30, 30),
                   radius=8, bw=3 if sel else 2)
        col = SUIT_COL.get(c["suit"], INK)
        rank = RANK_TXT.get(c["rank"], str(c["rank"]))
        self.text(rank, self.f_med, col, r.x + 7, r.y + 4)
        self.text(self.suit_text(c["suit"]), self.f_med, col,
                  r.centerx, r.centery + 6, center=True)
        if c["enhancement"]:
            self.text(ENH_TXT[c["enhancement"]], self.f_tiny, (90, 80, 40),
                      r.centerx, r.bottom - 12, center=True)
        if c["edition"]:
            self.text(ED_TXT[c["edition"]], self.f_tiny, (110, 70, 150),
                      r.x + 6, r.bottom - 24)
        if c["seal"]:
            pygame.draw.circle(self.screen, SEAL_COL[c["seal"]],
                               (r.right - 12, r.y + 12), 6)
        if c["debuffed"]:
            s = pygame.Surface((r.w, r.h), pygame.SRCALPHA)
            s.fill((40, 40, 40, 150))
            self.screen.blit(s, r.topleft)
            self.text("X", self.f_med, MULT, r.centerx, r.centery, center=True)
        return r

    def draw_hand(self):
        g = self.g
        cards = g.hand()
        if not cards:
            return
        cw, ch = 74, 102
        avail = CENTER_X1 - CENTER_X0 - 20
        step = min(cw + 8, max(30, (avail - cw) // max(1, len(cards) - 1))) \
            if len(cards) > 1 else 0
        x0 = CENTER_X0 + 10
        y = H - 210
        for i, c in enumerate(cards):
            rect = pygame.Rect(x0 + i * step, y, cw, ch)
            drawn = self.draw_card(rect, c)
            self.buttons.append(Btn(drawn, "", Action.TOGGLE_CARD + i))
            self.buttons[-1].label = None      # invisible hitbox
        nsel = sum(1 for c in cards if c["selected"])
        if nsel:
            self.text(f"{nsel} selected", self.f_sm, INK_DIM,
                      CENTER_X0 + 12, y - 20)
        # action row
        by = H - 88
        mask = g.legal_mask()
        row = [("Play hand", Action.PLAY, CHIPS),
               ("Discard", Action.DISCARD, MULT),
               ("Clear", Action.CLEAR_SELECTION, (120, 132, 140)),
               ("◀", Action.SHIFT_CARD_LEFT, (120, 132, 140)),
               ("▶", Action.SHIFT_CARD_RIGHT, (120, 132, 140))]
        bx = CENTER_X0 + 10
        for label, a, col in row:
            wdt = 60 if len(label) <= 2 else 150
            b = Btn((bx, by, wdt, 44), label, a, col, enabled=bool(mask[a]))
            self.buttons.append(b); self.button(b)
            bx += wdt + 10

    def draw_jokers_row(self):
        g = self.g
        mask = g.legal_mask()
        x, y = CENTER_X0 + 10, 14
        self.text("JOKERS", self.f_sm, INK_DIM, x, y)
        jx, jy, jw, jh = x, y + 18, 108, 66
        for i, j in enumerate(g.jokers()):
            rect = pygame.Rect(jx + i * (jw + 8), jy, jw, jh)
            r = rect.move(0, -8) if j["selected"] else rect
            self.plate(r, (36, 50, 62),
                       border=SEL_GLOW if j["selected"]
                       else RARITY_COL.get(j["rarity"], LINE),
                       radius=8, bw=3 if j["selected"] else 2)
            name = "???" if j["face_down"] else j["name"]
            words, line, lines = name.split(), "", []
            for w_ in words:
                if len(line) + len(w_) > 12 and line:
                    lines.append(line); line = w_
                else:
                    line = (line + " " + w_).strip()
            lines.append(line)
            for li, ln in enumerate(lines[:3]):
                self.text(ln, self.f_tiny, INK, r.centerx,
                          r.y + 12 + li * 13, center=True)
            self.text(f"${j['sell_value']}", self.f_tiny, MONEY,
                      r.right - 6, r.bottom - 15, right=True)
            if j["disabled"]:
                self.text("off", self.f_tiny, MULT, r.x + 5, r.bottom - 15)
            self.buttons.append(Btn(r, None, Action.TOGGLE_JOKER + i))
        bx = CENTER_X0 + 10
        for label, a in (("Sell joker", Action.SELL_JOKER),
                         ("Hold (acorn)", Action.HOLD_JOKER),
                         ("Shift ◀", Action.SHIFT_JOKER_LEFT),
                         ("Shift ▶", Action.SHIFT_JOKER_RIGHT)):
            if mask[a]:
                b = Btn((bx, jy + jh + 6, 104, 26), label, a,
                        (120, 132, 140), small=True)
                self.buttons.append(b); self.button(b)
                bx += 112
        # consumables to the right
        cs = g.consumables()
        cx = CENTER_X1 - 10 - 4 * 96
        self.text("CONSUMABLES", self.f_sm, INK_DIM, cx, y)
        for i, c in enumerate(cs):
            rect = pygame.Rect(cx + i * 96, jy, 88, jh)
            r = rect.move(0, -8) if c["selected"] else rect
            self.plate(r, (46, 42, 62),
                       border=SEL_GLOW if c["selected"] else (110, 96, 160),
                       radius=8, bw=3 if c["selected"] else 2)
            for li, ln in enumerate(c["name"].split()[:3]):
                self.text(ln, self.f_tiny, INK, r.centerx,
                          r.y + 14 + li * 13, center=True)
            self.buttons.append(Btn(r, None, Action.TOGGLE_CONSUMABLE + i))
        for k, (label, a) in enumerate((("Use", Action.USE_CONSUMABLE),
                                        ("Sell", Action.SELL_CONSUMABLE))):
            if mask[a]:
                b = Btn((cx + k * 92, jy + jh + 6, 84, 26), label, a,
                        (110, 96, 160), small=True)
                self.buttons.append(b); self.button(b)

    def draw_context(self):
        g = self.g
        area = pygame.Rect(CENTER_X0 + 6, 132, CENTER_X1 - CENTER_X0 - 12, 430)
        mask = g.legal_mask()
        ph = g.phase
        if ph == PHASE_BLIND_SELECT:
            blname = ["SMALL BLIND", "BIG BLIND",
                      "BOSS: " + (boss_name(g.boss) if g.boss >= 0 else "?")][g.blind]
            self.text(blname, self.f_big, INK, area.centerx, area.y + 90,
                      center=True)
            self.text(f"goal {fmt_score(g.goal_log2)}", self.f_med, INK_DIM,
                      area.centerx, area.y + 132, center=True)
            b = Btn((area.centerx - 170, area.y + 180, 160, 54),
                    "Select blind", Action.SELECT_BLIND, CHIPS,
                    enabled=bool(mask[Action.SELECT_BLIND]))
            self.buttons.append(b); self.button(b)
            b = Btn((area.centerx + 10, area.y + 180, 160, 54),
                    "Skip (take tag)", Action.SKIP_BLIND, (120, 132, 140),
                    enabled=bool(mask[Action.SKIP_BLIND]))
            self.buttons.append(b); self.button(b)
            if mask[Action.REROLL_BOSS]:
                b = Btn((area.centerx - 80, area.y + 250, 160, 40),
                        "Reroll boss", Action.REROLL_BOSS, (120, 132, 140))
                self.buttons.append(b); self.button(b)
        elif ph == PHASE_CASH_OUT:
            self.text("BLIND BEATEN", self.f_big, OK_GREEN,
                      area.centerx, area.y + 100, center=True)
            self.text(f"score {fmt_score(g.score_log2)}", self.f_med, CHIPS,
                      area.centerx, area.y + 145, center=True)
            b = Btn((area.centerx - 110, area.y + 190, 220, 56),
                    "Cash out  ⏎", Action.CONFIRM_CASHOUT, MONEY)
            self.buttons.append(b); self.button(b)
        elif ph == PHASE_SHOP:
            self.text("SHOP", self.f_med, INK, area.x + 6, area.y)
            sy = area.y + 28
            for i, s in enumerate(g.shop()):
                rect = pygame.Rect(area.x + 6 + (i % 3) * 240,
                                   sy + (i // 3) * 96, 230, 88)
                sold = s["sold"]
                self.plate(rect, (30, 42, 52) if sold else (38, 54, 66),
                           border=LINE if sold else
                           ((110, 96, 160) if s["kind"] else CHIPS))
                nm = "—" if sold else s["name"]
                self.text(nm, self.f_txt, INK_DIM if sold else INK,
                          rect.x + 10, rect.y + 10)
                if s["edition"]:
                    self.text(ED_TXT[s["edition"]], self.f_tiny,
                              (170, 120, 220), rect.x + 10, rect.y + 34)
                if not sold:
                    self.text(f"${s['cost']}", self.f_med, MONEY,
                              rect.right - 12, rect.y + 8, right=True)
                    a = Action.BUY_SLOT + i
                    b = Btn((rect.x + 10, rect.bottom - 34, 70, 26), "Buy",
                            a, OK_GREEN, enabled=bool(mask[a]), small=True)
                    self.buttons.append(b); self.button(b)
            py = sy + 2 * 96 + 8
            for i, p in enumerate(g.packs()):
                a = Action.BUY_PACK + i
                b = Btn((area.x + 6 + i * 250, py, 240, 40),
                        f"{p['name']}  ${p['cost']}", a, (110, 96, 160),
                        enabled=bool(mask[a]))
                self.buttons.append(b); self.button(b)
            vy = py + 50
            if g.voucher:
                b = Btn((area.x + 6, vy, 300, 40),
                        f"Voucher: {voucher_name(g.voucher)}",
                        Action.BUY_VOUCHER, MONEY,
                        enabled=bool(mask[Action.BUY_VOUCHER]))
                self.buttons.append(b); self.button(b)
            try:
                rr = (f"Reroll  FREE x{g.free_rerolls}" if g.free_rerolls
                      else f"Reroll  ${g.reroll_cost}")
                free_now = g.free_rerolls
            except RuntimeError:        # library predates nf_reroll_cost
                rr, free_now = "Reroll", 0
            for label, a, col in (((rr), Action.REROLL_SHOP,
                                   MONEY if free_now else (120, 132, 140)),
                                  ("Next round ⏎", Action.LEAVE_SHOP, CHIPS)):
                b = Btn((area.right - 370 if a == Action.REROLL_SHOP
                         else area.right - 190, vy, 170 if a == Action.REROLL_SHOP
                         else 180, 44), label, a, col, enabled=bool(mask[a]))
                self.buttons.append(b); self.button(b)
        elif ph == PHASE_PACK:
            self.text("PACK — pick one (select hand cards first if the card "
                      "needs targets)", self.f_txt, INK, area.x + 6, area.y)
            for i, p in enumerate(g.pack_contents()):
                if p["taken"]:
                    continue
                a = Action.PICK_PACK + i
                rect = pygame.Rect(area.x + 6 + (i % 3) * 240,
                                   area.y + 30 + (i // 3) * 66, 230, 58)
                self.plate(rect, (46, 42, 62), border=(110, 96, 160))
                self.text(p["name"], self.f_txt, INK,
                          rect.centerx, rect.centery - 10, center=True)
                b = Btn((rect.x + 65, rect.bottom - 26, 100, 22), "Take", a,
                        OK_GREEN, enabled=bool(mask[a]), small=True)
                self.buttons.append(b); self.button(b)
            b = Btn((area.x + 6, area.bottom - 46, 140, 36), "Skip pack",
                    Action.SKIP_PACK, (120, 132, 140),
                    enabled=bool(mask[Action.SKIP_PACK]))
            self.buttons.append(b); self.button(b)

    def draw_logpanel(self):
        r = pygame.Rect(W - LOGPANEL_W - 2, 6, LOGPANEL_W - 4, H - 12)
        self.plate(r, FELT_DARK)
        self.text("RUN LOG", self.f_sm, INK_DIM, r.x + 12, r.y + 8)
        self.text(os.path.basename(self.log.path), self.f_tiny, INK_DIM,
                  r.x + 12, r.y + 24)
        y = r.bottom - 24
        for m in reversed(self.msgs):
            col = INK
            if m.startswith(("BLIND", "***", "===")): col = OK_GREEN
            elif m.startswith("—"): col = MONEY
            elif m.strip().startswith(("+", "-")): col = MONEY
            self.text(m[:34], self.f_sm, col, r.x + 12, y)
            y -= 17
            if y < r.y + 44:
                break

    def draw_gameover(self):
        g = self.g
        s = pygame.Surface((W, H), pygame.SRCALPHA)
        s.fill((10, 14, 18, 210))
        self.screen.blit(s, (0, 0))
        msg = "NANEINF" if g.won else "RUN OVER"
        col = OK_GREEN if g.won else MULT
        self.text(msg, self.f_big, col, W // 2, H // 2 - 110, center=True)
        self.text(f"seed {g.seed} — reached ante {g.ante} — "
                  f"{self.log.steps} actions logged", self.f_txt, INK,
                  W // 2, H // 2 - 60, center=True)
        self.text(f"log: {self.log.path}", self.f_sm, INK_DIM,
                  W // 2, H // 2 - 34, center=True)
        for i, (label, fn) in enumerate((
                ("New run  [N]", lambda: (self.finish_run(), self.new_run())),
                ("Same seed  [S]", lambda: (self.finish_run(),
                                            self.new_run(g.seed))))):
            b = Btn((W // 2 - 190 + i * 200, H // 2 + 10, 180, 52), label, fn,
                    CHIPS if i == 0 else (120, 132, 140))
            self.buttons.append(b); self.button(b)

    # ---- main loop -------------------------------------------------------
    def draw(self):
        self.screen.fill(FELT)
        self.buttons = []
        self.draw_sidebar()
        self.draw_logpanel()
        self.draw_jokers_row()
        self.draw_context()
        if self.g.phase in (PHASE_PLAYING, PHASE_PACK):
            self.draw_hand()
        if self.g.phase == PHASE_GAME_OVER or self.g.done:
            self.draw_gameover()
        if self.flash_t > 0:
            self.flash_t -= 1
            self.text(self.flash, self.f_txt, MULT, (CENTER_X0 + CENTER_X1)//2,
                      H - 118, center=True)
        pygame.display.flip()

    def run(self):
        running = True
        while running:
            for ev in pygame.event.get():
                if ev.type == pygame.QUIT:
                    running = False
                elif ev.type == pygame.KEYDOWN:
                    if ev.key == pygame.K_ESCAPE:
                        running = False
                    else:
                        self.on_key(ev.key)
                elif ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
                    self.on_click(ev.pos)
            self.draw()
            self.clock.tick(60)
        self.finish_run()
        pygame.quit()


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seed", default=None, help="8-char run seed")
    ap.add_argument("--runs-dir", default="runs")
    ap.add_argument("--log-obs", action="store_true",
                    help="embed the 222-float observation in every step "
                         "record (bigger files; otherwise use runlog.py "
                         "export to reconstruct)")
    args = ap.parse_args()
    App(args.seed, args.runs_dir, args.log_obs).run()


if __name__ == "__main__":
    main()
