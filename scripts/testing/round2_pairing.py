#!/usr/bin/env python3
"""Round 2 of the paired-game reading: the games19 pairing method on an odd pool.

Usage: round2_pairing.py <decks-dir> <plan.tsv> [--seed 20261015]
                         [--agent ismcts:sims=200] [--reseed <game>=<seed> ...]

Exactly the method of games19_pairing.py (sorted list names shuffled with
random.Random(seed), neighbours paired, a second draw from the same generator
per pairing decides which list is --deck1, game k gets engine seed
seed * 100 + k). The one difference: with an odd number of lists the last name
of the shuffled order is left over; it is printed (to stderr) and plays no game.
"""
import argparse, os, random, sys

def pairings(decks_dir, seed):
    names = sorted(f[:-4] for f in os.listdir(decks_dir) if f.endswith(".txt"))
    rng = random.Random(seed)
    rng.shuffle(names)
    left = names[-1] if len(names) % 2 else None
    out = []
    for k in range(len(names) // 2):
        a, b = names[2 * k], names[2 * k + 1]
        if rng.random() < 0.5:
            a, b = b, a
        out.append((a, b, seed * 100 + k + 1))
    return out, left

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("decks_dir")
    ap.add_argument("plan")
    ap.add_argument("--seed", type=int, default=20261015)
    ap.add_argument("--agent", default="ismcts:sims=200")
    ap.add_argument("--reseed", action="append", default=[])
    args = ap.parse_args()
    reseed = dict((int(g), int(s)) for g, s in (r.split("=") for r in args.reseed))
    games, left = pairings(args.decks_dir, args.seed)
    with open(args.plan, "w") as f:
        for k, (a, b, s) in enumerate(games, 1):
            f.write(f"{a}\t{b}\t{reseed.get(k, s)}\t{args.agent}\t{args.agent}\n")
    if left:
        print(f"left over (odd pool): {left}", file=sys.stderr)

if __name__ == "__main__":
    main()
