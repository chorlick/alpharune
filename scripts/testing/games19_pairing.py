#!/usr/bin/env python3
"""Pair the deck lists without replacement: every list plays exactly one game.

Usage: games19_pairing.py <decks-dir> <plan.tsv> [--seed 20261011]
                          [--agent ismcts:sims=50] [--reseed <game>=<seed> ...]

The sorted list names are shuffled with random.Random(seed) and neighbours are
paired (2n lists -> n games). A second draw from the same generator per pairing
decides which list is --deck1. Game k (1-based) gets engine seed
`seed * 100 + k`, so the plan is a function of the directory listing and the
one seed. `--reseed 7=2026101199` replaces the engine seed of game 7 (used when
a game failed and the pairing needs another seed; the failed seed is a finding).

The output is a plan for run_deck_games.py: deck1, deck2, seed, agent1, agent2.
"""
import argparse, os, random

def pairings(decks_dir, seed):
    names = sorted(f[:-4] for f in os.listdir(decks_dir) if f.endswith(".txt"))
    if len(names) % 2:
        raise SystemExit(f"{len(names)} lists: an odd pool cannot be paired off")
    rng = random.Random(seed)
    rng.shuffle(names)
    out = []
    for k in range(len(names) // 2):
        a, b = names[2 * k], names[2 * k + 1]
        if rng.random() < 0.5:
            a, b = b, a
        out.append((a, b, seed * 100 + k + 1))
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("decks_dir")
    ap.add_argument("plan")
    ap.add_argument("--seed", type=int, default=20261011)
    ap.add_argument("--agent", default="ismcts:sims=50")
    ap.add_argument("--reseed", action="append", default=[])
    args = ap.parse_args()
    reseed = dict((int(g), int(s)) for g, s in (r.split("=") for r in args.reseed))
    rows = pairings(args.decks_dir, args.seed)
    with open(args.plan, "w") as f:
        f.write(f"# games19_pairing.py --seed {args.seed}; game k is line k\n")
        for k, (a, b, s) in enumerate(rows, 1):
            s = reseed.get(k, s)
            f.write(f"{a}\t{b}\t{s}\t{args.agent}\t{args.agent}\n")
            print(f"{k:2d}  {a:14s} {b:14s} {s}")

if __name__ == "__main__":
    main()
