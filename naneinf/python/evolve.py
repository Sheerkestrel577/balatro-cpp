"""
evolve.py — GA driver for evo_agent.

  py evolve.py                                  # fresh run, defaults
  py evolve.py --resume ckpt.npz --gens 1000
  py evolve.py --es 0.3 --sigma 0.08            # loosen anti-fidget
  py evolve.py --es 0 --temp 0.3                # scaffolding off

Design choices (from the discussion):
  * fixed seed PANEL per generation, same seeds for every genome
    (common random numbers: compare policy, not luck), rotated each gen
  * fitness = mean over the panel
  * tournament selection + elitism, uniform crossover, gaussian mutation
  * random immigrants each gen to fight clone collapse
  * everything checkpoints to .npz so runs are resumable

Reading the log:
  best/mean  fitness (3.0 = did nothing; +1 per blind cleared;
             25+ = made it through ante 8)
  smalls     how many of the population cleared at least ONE blind
             (fitness >= 4). If this is near 0, the population is dying
             on the SMALL blind and nothing downstream matters yet.
  bigs       cleared at least two blinds (fitness >= 5).
"""
import argparse
import multiprocessing as mp
import time

import numpy as np

from evo_agent import GENOME_SIZE, random_genome, rollout

CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789"


def panel_seeds(gen, n, half_life=3):
    """n seeds for this generation; overlap adjacent gens so progress
    on last gen's seeds isn't thrown away wholesale."""
    seeds = []
    for i in range(n):
        r = np.random.default_rng(max(gen - (i % half_life), 0) * 1000 + i)
        seeds.append("".join(CHARS[j] for j in r.integers(0, len(CHARS), 8)))
    return seeds


def _eval(args):
    genome, seeds, skip_credit, temp, es = args
    return float(np.mean([rollout(genome, s, skip_credit=skip_credit,
                                  temp=temp, es=es) for s in seeds]))


def evolve(pop_size=48, gens=100, elite=4, tourn=3, sigma=0.05,
           mut_prob=0.25, panel=4, workers=None, resume=None,
           ckpt="ckpt.npz", skip_credit=-0.5, immigrants=6,
           temp=0.5, es=1.0):
    rng = np.random.default_rng(42)
    start_gen = 0
    if resume:
        z = np.load(resume)
        pop = [g.copy() for g in z["pop"].astype(np.float32)]
        if pop[0].shape[0] != GENOME_SIZE:
            raise SystemExit(
                f"checkpoint genome is {pop[0].shape[0]} params but "
                f"evo_agent wants {GENOME_SIZE}. The architecture changed "
                f"— start fresh (drop --resume).")
        start_gen = int(z["gen"]) + 1
        print(f"resumed gen {start_gen}, pop {len(pop)}")
    else:
        pop = [random_genome(rng) for _ in range(pop_size)]

    print(f"es={es} temp={temp} sigma={sigma} skip_credit={skip_credit} "
          f"panel={panel} immigrants={immigrants}")

    workers = workers or max(mp.cpu_count() - 1, 1)
    with mp.Pool(workers) as pool:
        for gen in range(start_gen, start_gen + gens):
            t0 = time.time()
            seeds = panel_seeds(gen, panel)
            fits = np.array(pool.map(
                _eval, [(g, seeds, skip_credit, temp, es) for g in pop]))
            order = np.argsort(fits)[::-1]
            best, mean = fits[order[0]], fits.mean()
            smalls = int((fits >= 4.0).sum())
            bigs = int((fits >= 5.0).sum())
            cleared = int((fits >= 25).sum())
            print(f"gen {gen:4d}  best {best:6.2f}  mean {mean:5.2f}  "
                  f"smalls {smalls:2d}  bigs {bigs:2d}  "
                  f"ante8 {cleared:2d}/{len(pop)}  "
                  f"[{time.time()-t0:.0f}s]")

            np.savez(ckpt, pop=np.stack(pop), gen=gen, fits=fits)
            np.save("best.npy", pop[order[0]])

            children = [pop[i].copy() for i in order[:elite]]
            children += [random_genome(rng) for _ in range(immigrants)]
            while len(children) < len(pop):
                a = pop[order[min(rng.integers(0, len(pop), tourn))]]
                b = pop[order[min(rng.integers(0, len(pop), tourn))]]
                child = np.where(rng.random(GENOME_SIZE) < 0.5, a, b)
                m = rng.random(GENOME_SIZE) < mut_prob
                child = child + m * rng.normal(0, sigma, GENOME_SIZE)
                children.append(child.astype(np.float32))
            pop = children
    return pop


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--pop", type=int, default=48)
    ap.add_argument("--gens", type=int, default=100)
    ap.add_argument("--panel", type=int, default=4)
    ap.add_argument("--sigma", type=float, default=0.05)
    ap.add_argument("--workers", type=int, default=None)
    ap.add_argument("--resume", default=None)
    ap.add_argument("--skip-credit", type=float, default=-0.5)
    ap.add_argument("--immigrants", type=int, default=6)
    ap.add_argument("--temp", type=float, default=0.5,
                    help="action sampling temperature; 0 = greedy")
    ap.add_argument("--es", type=float, default=1.0,
                    help="anti-fidget strength 0..1 (0 = off entirely)")
    a = ap.parse_args()
    evolve(pop_size=a.pop, gens=a.gens, panel=a.panel, sigma=a.sigma,
           workers=a.workers, resume=a.resume,
           skip_credit=a.skip_credit, immigrants=a.immigrants,
           temp=a.temp, es=a.es)
