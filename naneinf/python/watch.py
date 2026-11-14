#!/usr/bin/env python3
"""watch.py -- watch an agent play one run, narrated.

    python3 watch.py                          # v0, random seed
    python3 watch.py --agent v2               # rollout agent, no net
    python3 watch.py --agent v2 --model models/v_gen6.npz
    python3 watch.py --seed TGKP7NP2          # replay a specific run
    python3 watch.py --delay 0.4              # slow it down to read along
    python3 watch.py --quiet                  # blind-by-blind only

Nothing here touches the training loop -- it constructs its own agent
and its own game. Safe to run on the pi while the loop is collecting
(it will just be sharing cores; use --nice if you care).

Narration is driven by watching the game state change between actions,
not by instrumenting the agents, so v0 and v2 are both readable without
either knowing it's being observed.
"""
from __future__ import annotations
import argparse, os, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import naneinf
from naneinf import Action as A, action_name, PHASE_NAMES

C = dict(dim="\033[2m", bold="\033[1m", red="\033[31m", grn="\033[32m",
         yel="\033[33m", blu="\033[36m", mag="\033[35m", off="\033[0m")


def paint(s, c):
    return f"{C[c]}{s}{C['off']}"


def money_str(m):
    return paint(f"${m}", "grn" if m >= 10 else "yel")


def score_str(g):
    """Blind progress as a log2 ratio -- naneinf scores overflow floats."""
    try:
        s, goal = g.score_log2, g.goal_log2
    except Exception:
        return ""
    if goal <= 0:
        return ""
    pct = 100.0 * (2.0 ** min(s - goal, 0.0)) if s < goal else 100.0
    bar_n = int(min(pct, 100) / 10)
    bar = "#" * bar_n + "." * (10 - bar_n)
    col = "grn" if s >= goal else ("yel" if pct > 25 else "red")
    return paint(f"[{bar}] 2^{s:.1f}/2^{goal:.1f}", col)


def card_face(c):
    """One card, colored by what matters to the plan."""
    n = c["name"]
    marks = ""
    if c["seal"] == 1:
        marks += "*"                      # red seal
    if c["enhancement"] == 5:
        marks += "S"                      # steel
    elif c["enhancement"] == 7:
        marks += "G"                      # gold
    if c["edition"]:
        marks += "+"
    s_ = n + (paint(marks, "mag") if marks else "")
    if c["seal"] == 1:
        return paint(n, "red") + (paint(marks, "mag") if marks else "")
    if c["enhancement"] in (5, 7):
        return paint(n, "yel") + (paint(marks, "mag") if marks else "")
    if c["rank"] == 13:
        return paint(n, "blu")            # kings: the goal
    return s_


def hand_str(g):
    cards = g.hand()
    if not cards:
        return ""
    out = []
    for c in cards:
        n = c["name"]
        if c["seal"] == 1:
            n = paint(n, "red")          # red seal: the whole point
        elif c["enhancement"] in (5, 7):
            n = paint(n, "yel")          # steel / gold
        if c["selected"]:
            n = paint(f"({n})", "bold")
        out.append(n)
    return " ".join(out)


def deck_line(g):
    try:
        d = g.deck()
    except Exception:
        return ""
    n = max(1, len(d))
    kings = [c for c in d if c["rank"] == 13]
    rs_kings = sum(1 for c in kings if c["seal"] == 1)
    fit = len(kings) * (100.0 / n) ** 2
    return (f"deck {n}  kings {len(kings)}  "
            f"{paint(f'red-seal kings {rs_kings}', 'red')}  "
            f"fitness {fit:.1f}")


def jokers_str(g):
    js = g.jokers()
    if not js:
        return paint("(no jokers)", "dim")
    return ", ".join(f"{j['name']}" + (paint(f"[{j['edition']}]", "mag")
                                       if j["edition"] else "")
                     for j in js)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--agent", choices=["v0", "v2", "v2raw"], default="v0")
    ap.add_argument("--model", default=None)
    ap.add_argument("--seed", default=None,
                    help="replay a specific seed (from history/batch output)")
    ap.add_argument("--delay", type=float, default=0.0,
                    help="seconds to pause after each action")
    ap.add_argument("--quiet", action="store_true",
                    help="one line per blind/shop instead of per action")
    ap.add_argument("--k", type=int, default=3)
    args = ap.parse_args()

    if args.agent == "v0":
        from agent_v0 import Agent
        a = Agent(args.seed)
    else:
        from agent_v2 import RolloutAgent
        model = None if args.agent == "v2raw" else args.model
        a = RolloutAgent(args.seed, k=args.k, model=model)

    G, g = a.G, a.G.g
    print(paint(f"\n=== {args.agent} plays seed {G.seed} ===", "bold"))
    print(paint("Plasma deck, white stake, goal: red-seal kings\n", "dim"))

    # Narrate by DIFFING state around each action, so the agents stay
    # untouched: wrap the Runner's do() and report what changed.
    orig_do = G.do
    state = dict(ante=g.ante, blind=g.blind, money=g.money,
                 phase=g.phase, jokers=len(g.jokers()),
                 deck=g.deck_size, hands=g.hands_left,
                 discards=g.discards_left)

    def narrate(action, before, after):
        nm = action_name(action)
        if not args.quiet:
            action_line(action, nm, before, after)
        # phase headers come AFTER the action that caused the change
        if after["phase"] != before["phase"]:
            p = PHASE_NAMES[after["phase"]]
            if after["phase"] == naneinf.PHASE_PLAYING:
                print(paint(f"\n-- ante {after['ante']} blind "
                            f"{after['blind']} --", "blu"),
                      score_str(g))
            elif after["phase"] == naneinf.PHASE_SHOP:
                print(paint(f"\n-- shop (ante {after['ante']}) --", "mag"),
                      money_str(after["money"]))
                for it in g.shop():
                    if not it["sold"]:
                        print(paint(f"     for sale: {it['name']} "
                                    f"(${it['cost']})", "dim"))
            elif after["phase"] == naneinf.PHASE_CASH_OUT:
                print(paint(f"   blind cleared", "grn"), " ",
                      money_str(after["money"]))
    def action_line(action, nm, before, after):
        # the interesting actions
        if action in (A.PLAY, A.DISCARD):
            hl = after["hands"]
            sel = before.get("sel") or []
            played = " ".join(card_face(c) for c in sel)
            kept = " ".join(card_face(c) for c in
                            (before.get("hand_all") or [])
                            if not c["selected"])
            head = paint("PLAY   ", "grn") if action == A.PLAY \
                   else paint("DISCARD", "yel")
            print(f"   {head} {played}")
            if kept:
                print(paint(f"           holding: {kept}", "dim"))
            print(f"           {score_str(g)}  "
                  + paint(f"{hl} hands, {after['discards']} discards left",
                          "dim"))
        elif nm.startswith("buy"):
            spent = before["money"] - after["money"]
            got = before.get("bought") or nm
            print(f"   {paint('BUY', 'yel')}       {got} "
                  f"(-${spent}, {money_str(after['money'])} left)")
        elif nm == "reroll_shop":
            print(f"   {paint('reroll', 'dim')}    "
                  f"{money_str(after['money'])}")
        elif nm == "leave_shop":
            print(f"   {paint('leaves shop', 'dim')}  "
                  f"jokers: {jokers_str(g)}")
        elif after["deck"] != before["deck"]:
            print(f"   {paint('deck changes', 'dim')}  {deck_line(g)}")

    def do_wrapped(action, *rest, **kw):
        before = dict(state)
        # resolve what a BUY refers to while the shop still shows it
        try:
            if action in (A.PLAY, A.DISCARD):
                cards = g.hand()
                before["sel"] = [c for c in cards if c["selected"]]
                before["hand_all"] = cards
            if g.phase == naneinf.PHASE_SHOP and A.BUY_SLOT <= action \
                    < A.BUY_SLOT + 6:
                it = g.shop()[action - A.BUY_SLOT]
                before["bought"] = f"{it['name']}"
            elif A.BUY_PACK <= action < A.BUY_PACK + 2:
                pk = g.packs()
                i = action - A.BUY_PACK
                before["bought"] = (pk[i]["name"] if i < len(pk)
                                    else "booster pack")
        except Exception:
            pass
        r = orig_do(action, *rest, **kw)
        after = dict(ante=g.ante, blind=g.blind, money=g.money,
                     phase=g.phase, jokers=len(g.jokers()),
                     deck=g.deck_size, hands=g.hands_left,
                     discards=g.discards_left)
        try:
            narrate(action, before, after)
        except Exception as e:                # narration must never kill a run
            print(paint(f"   (narrate error: {e})", "dim"))
        state.update(after)
        if args.delay:
            time.sleep(args.delay)
        return r

    G.do = do_wrapped

    t0 = time.time()
    r = a.run(max_secs=600)

    print(paint(f"\n=== run over ===", "bold"))
    print(f"  reached ante {r['ante']}   "
          f"{paint('WON', 'grn') if r['won'] else paint('died', 'red')}   "
          f"{money_str(r['money'])}   {time.time()-t0:.1f}s")
    print(f"  {deck_line(g)}")
    print(f"  jokers: {jokers_str(g)}")
    print(f"  seed: {G.seed}   (replay with --seed {G.seed})")


if __name__ == "__main__":
    main()
