"""batch.py — evaluate an agent over many seeds and report the
ante-8-checkpoint distribution, including DECK QUALITY, not just survival.

Deck quality is what the naneinf plan actually cares about: the run wants
red-seal gold/steel Kings concentrated in one suit (for solo Idol), then
Baron/Mime later. A run reaching ante 8 with 52 junk cards is worth far
less than one reaching ante 6 with a pruned king deck -- and plain mean
ante hides that completely.

    python3 batch.py --runs 60                       # v0 (fast, dict shop)
    python3 batch.py --runs 60 --agent v2            # rollout shop
    python3 batch.py --runs 60 --agent v1 --model models/v_gen1.npz
    python3 batch.py --runs 40 --agent v2 --compare v0   # paired A/B
"""
from __future__ import annotations
import argparse, json, os, sys, time
import multiprocessing as mp
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def deck_stats(g):
    """Composition of the live deck: the axes the endgame plan needs."""
    try:
        d = g.deck()
    except Exception:
        return {}
    n = max(1, len(d))
    kings = [c for c in d if c["rank"] == 13]
    # enhancement ids: 5 steel, 7 gold (card.hpp); seal id 1 = red
    red = [c for c in d if c["seal"] == 1]
    gold_steel = [c for c in d if c["enhancement"] in (5, 7)]
    king_suits = Counter(c["suit"] for c in kings)
    top_suit = king_suits.most_common(1)[0][1] if kings else 0
    return dict(
        deck=n,
        kings=len(kings),
        king_pct=round(100.0 * len(kings) / n, 1),
        red_seals=len(red),
        gold_steel=len(gold_steel),
        rs_kings=sum(1 for c in kings if c["seal"] == 1),
        rs_ge_kings=sum(1 for c in kings
                        if c["seal"] == 1 and c["enhancement"] in (5, 7)),
        king_suit_conc=round(top_suit / max(1, len(kings)), 2),
        fitness=round(len(kings) * (100.0 / n) ** 2, 2),
    )


def _run_one(spec):
    idx, kind, model, k, seed = spec
    if kind == "v0":
        from agent_v0 import Agent
        a = Agent(seed)
    elif kind == "v1":
        from agent_v1 import ValueAgent
        a = ValueAgent(model, seed=seed)
    elif kind == "v2raw":
        from agent_v2 import RolloutAgent
        a = RolloutAgent(seed, k=k, model=None)      # no value net
    else:
        from agent_v2 import RolloutAgent
        a = RolloutAgent(seed, k=k, model=model)
    r = a.run(max_secs=240)
    r.update(deck_stats(a.G.g))
    r["hc_level"] = a.G.g.hand_level(0)      # High Card level: Baron's floor
    return r


def summarize(res, label):
    n = len(res)
    antes = [r["ante"] for r in res]
    reached8 = sum(1 for r in res if r["ante"] >= 8)
    print(f"\n=== {label}: {n} runs ===")
    print("ante distribution:", dict(sorted(Counter(antes).items())))
    print(f"mean ante {sum(antes)/n:.2f}   reached ante 8+: "
          f"{reached8} ({100*reached8/n:.0f}%)   "
          f"wins {sum(r['won'] for r in res)}   "
          f"softlocks {sum(r.get('softlock', False) for r in res)}")
    if any("kings" in r for r in res):
        f = lambda key: sum(r.get(key, 0) for r in res) / n
        print(f"deck at end -- size {f('deck'):.1f}  kings {f('kings'):.1f} "
              f"({f('king_pct'):.0f}%)  red seals {f('red_seals'):.1f}  "
              f"gold/steel {f('gold_steel'):.1f}")
        print(f"  red-seal kings {f('rs_kings'):.2f}   "
              f"red-seal gold/steel kings {f('rs_ge_kings'):.2f}   "
              f"king suit conc {f('king_suit_conc'):.2f}   "
              f"HC level {f('hc_level'):.1f}")
        print(f"  stated fitness K*(100/N)^2: mean {f('fitness'):.1f}  "
              f"max {max(r.get('fitness', 0) for r in res):.1f}")
    # the frontier: are purity and progress compatible, or a trade-off?
    deep = [r for r in res if r["ante"] >= 6]
    if deep:
        g = lambda key: sum(r.get(key, 0) for r in deep) / len(deep)
        print(f"  among ante-6+ runs (n={len(deep)}): kings {g('kings'):.1f}, "
              f"deck {g('deck'):.1f}, money {g('money'):.0f}, "
              f"HC {g('hc_level'):.1f}")
    best = max(res, key=lambda r: (r["ante"], r.get("fitness", 0)))
    print("  deepest:", {k: best.get(k) for k in
                         ("seed", "ante", "won", "money", "kings", "deck",
                          "hc_level", "jokers")})


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", type=int, default=60)
    ap.add_argument("--workers", type=int, default=max(1, mp.cpu_count() - 1))
    ap.add_argument("--agent", choices=["v0", "v1", "v2", "v2raw"],
                    default="v0",
                    help="v2 uses --model as its rollout tail estimate; "
                         "v2raw is the same agent with NO value net")
    ap.add_argument("--compare", choices=["v0", "v1", "v2", "v2raw"],
                    default=None,
                    help="also run this agent on the SAME seeds (paired A/B). "
                         "'--agent v2 --model M --compare v2raw' answers "
                         "'does the value net actually help?'")
    ap.add_argument("--model", default=None)
    ap.add_argument("--compare_model", default=None,
                    help="value net for the COMPARE side. Enables "
                         "model-vs-model: --agent v2 --model v_gen5.npz "
                         "--compare v2 --compare_model v_gen4.npz")
    ap.add_argument("--k", type=int, default=3, help="v2 rollout samples")
    ap.add_argument("--out", default="batch_results.json")
    args = ap.parse_args()

    if args.agent == "v1" and not args.model:
        sys.exit("--agent v1 needs --model models/v_gen1.npz")

    # fixed seed list so --compare is PAIRED: both agents face identical
    # runs, removing most between-seed variance from the difference.
    from naneinf import Balatro
    seeds = [Balatro().seed for _ in range(args.runs)]

    def _model_for(kind, explicit):
        if kind == "v2raw" or kind == "v0":
            return None
        return explicit

    runsets = [(args.agent, _model_for(args.agent, args.model))]
    if args.compare:
        cm = args.compare_model if args.compare_model is not None \
             else args.model
        runsets.append((args.compare, _model_for(args.compare, cm)))

    def _label(kind, model):
        if model:
            tag = os.path.basename(model).replace(".npz", "")
            return f"{kind}[{tag}]"
        return kind

    all_res = {}
    labels = []
    for kind, model in runsets:
        label = _label(kind, model)
        if label in all_res:
            label += "#2"
        labels.append(label)
        specs = [(i, kind, model, args.k, s) for i, s in enumerate(seeds)]
        t0 = time.time()
        res = []
        if args.workers > 1:
            # drain with a deadline: mp.Pool never re-queues a dead
            # worker's task, so a bare loop can hang forever (see
            # collect.py / README section 4)
            with mp.Pool(args.workers) as p:
                it = p.imap_unordered(_run_one, specs)
                while True:
                    try:
                        r = it.next(timeout=420)
                    except StopIteration:
                        break
                    except mp.TimeoutError:
                        print(f"\n!! stalled -- lost "
                              f"{args.runs - len(res)} run(s), continuing "
                              f"with {len(res)}")
                        break
                    res.append(r)
                    print(f"\r{kind}: [{len(res)}/{args.runs}] "
                          f"{(time.time()-t0)/len(res):.1f}s/run   ",
                          end="", flush=True)
        else:
            for sp in specs:
                res.append(_run_one(sp))
                print(f"\r{kind}: [{len(res)}/{args.runs}]", end="",
                      flush=True)
        print()
        all_res[label] = res
        summarize(res, label)

    if args.compare:
        a, b = labels
        ma = sum(r["ante"] for r in all_res[a]) / args.runs
        mb = sum(r["ante"] for r in all_res[b]) / args.runs
        import statistics as st
        da = {r["seed"]: r["ante"] for r in all_res[a]}
        db = {r["seed"]: r["ante"] for r in all_res[b]}
        common = [s for s in da if s in db]
        diffs = [da[s] - db[s] for s in common]
        se = (st.pstdev(diffs) / (len(diffs) ** 0.5)) if len(diffs) > 1 else 0
        wa = {r["seed"]: r["won"] for r in all_res[a]}
        wb = {r["seed"]: r["won"] for r in all_res[b]}
        wd = [int(wa[s_]) - int(wb[s_]) for s_ in common]
        print(f"\nPAIRED: {a} {ma:.2f} vs {b} {mb:.2f} antes | "
              f"mean diff {sum(diffs)/max(1,len(diffs)):+.2f} +/- {se:.2f} "
              f"(1 s.e., n={len(diffs)})")
        print(f"  wins: {a} {sum(wa[s_] for s_ in common)} vs "
              f"{b} {sum(wb[s_] for s_ in common)} "
              f"(paired diff {sum(wd):+d} on identical seeds; "
              f"seeds won by only one side: "
              f"{sum(1 for d in wd if d)} of {len(wd)})")
        print("  a difference under ~2 s.e. is noise, not an improvement")

    with open(args.out, "w") as f:
        json.dump(all_res, f)
    print(f"\nwrote {args.out}")


if __name__ == "__main__":
    main()
