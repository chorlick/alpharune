#!/usr/bin/env python3
"""Write the game plans of the deck-list testing run (see run_deck_games.py).

Usage: make_plan.py <decks-dir> <out-dir>

  plan_rr.tsv      every ordered pairing of the lists (18 x 18 = 324 games),
                   random vs random, seed 20261100 + 18*i + j.
  plan_rr_sb.tsv   the same with the post-sideboard lists (<deck>_sb), seeds
                   20262100 + 18*i + j.
  plan_ismcts.tsv  per list four games with ismcts:sims=20 in the list's seat
                   against a random opponent (two as P1, two as P2; opponents
                   are the lists 1, 5, 7 and 11 places further on), seeds
                   20263000 + 4*i + k.
  plan_rr2.tsv     a second seed for every ordered pairing of the lists,
                   20264100 + 18*i + j (run into a directory of its own).
  plan_ismcts_sb.tsv  the ISMCTS plan with the post-sideboard lists, seeds
                   20265000 + 4*i + k.
"""
import os, sys
def main():
    src, out = sys.argv[1:3]
    decks = sorted(f[:-4] for f in os.listdir(src) if f.endswith(".txt"))
    n = len(decks)
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "plan_rr.tsv"), "w") as f:
        for i, a in enumerate(decks):
            for j, b in enumerate(decks):
                f.write(f"{a}\t{b}\t{20261100 + n*i + j}\trandom\trandom\n")
    with open(os.path.join(out, "plan_rr_sb.tsv"), "w") as f:
        for i, a in enumerate(decks):
            for j, b in enumerate(decks):
                f.write(f"{a}_sb\t{b}_sb\t{20262100 + n*i + j}\trandom\trandom\n")
    with open(os.path.join(out, "plan_ismcts.tsv"), "w") as f:
        for i, a in enumerate(decks):
            for k, off in enumerate((1, 5, 7, 11)):
                b = decks[(i + off) % n]
                seed = 20263000 + 4*i + k
                if k < 2:
                    f.write(f"{a}\t{b}\t{seed}\tismcts:sims=20\trandom\n")
                else:
                    f.write(f"{b}\t{a}\t{seed}\trandom\tismcts:sims=20\n")
    with open(os.path.join(out, "plan_rr2.tsv"), "w") as f:
        for i, a in enumerate(decks):
            for j, b in enumerate(decks):
                f.write(f"{a}\t{b}\t{20264100 + n*i + j}\trandom\trandom\n")
    with open(os.path.join(out, "plan_ismcts_sb.tsv"), "w") as f:
        for i, a in enumerate(decks):
            for k, off in enumerate((1, 5, 7, 11)):
                b = decks[(i + off) % n]
                seed = 20265000 + 4*i + k
                if k < 2:
                    f.write(f"{a}_sb\t{b}_sb\t{seed}\tismcts:sims=20\trandom\n")
                else:
                    f.write(f"{b}_sb\t{a}_sb\t{seed}\trandom\tismcts:sims=20\n")

if __name__ == "__main__":
    main()
