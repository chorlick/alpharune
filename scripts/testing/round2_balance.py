#!/usr/bin/env python3
"""Round-robin tables of testing round 2 from deck_fuzz --rows files or the
ISMCTS round robin's TSV.

Usage: round2_balance.py <label>=<file> [<label>=<file> ...] [--matrix <out.csv>]

<file> is either a `deck_fuzz --rows` file (index, deckA, deckB, seed,
winner 0/1/2, decisions, turns, longest chain, flags) or the TSV of
round2_ismcts_rr.py (deck1, deck2, seed, rc, winner, reason, turns, seconds).
Prints Markdown: per run the totals (games, P1 / P2 / no winner, mean turns and
decisions where known) and, per list, games (mirrors left out), win rate overall,
as P1 and as P2, mean turns. With --matrix, writes for the FIRST run a CSV win
matrix (row list's win rate against column list, both seatings pooled).
"""
import collections, csv, sys


def load(path):
    games = []
    for line in open(path):
        f = line.rstrip("\n").split("\t")
        if len(f) == 9 and f[0].isdigit():  # deck_fuzz rows
            a, b = f[1].split("/")[-1][:-4], f[2].split("/")[-1][:-4]
            w = int(f[4])
            games.append((a, b, w, int(f[6]), int(f[5])))
        elif len(f) == 8:  # ismcts tsv
            w = int(f[4]) if f[4].isdigit() else 0
            if f[3] != "0":
                w = -1  # engine failure
            games.append((f[0], f[1], w, int(f[6]) if f[6].isdigit() else 0, None))
    return games


def table(label, games):
    n = len(games)
    p1 = sum(1 for g in games if g[2] == 1)
    p2 = sum(1 for g in games if g[2] == 2)
    fail = sum(1 for g in games if g[2] == -1)
    none = n - p1 - p2 - fail
    turns = sum(g[3] for g in games) / n
    decs = [g[4] for g in games if g[4] is not None]
    print(f"\n### {label}\n")
    print(f"{n} games: P1 {p1} ({100*p1/n:.1f}%), P2 {p2} ({100*p2/n:.1f}%), no winner {none}, "
          f"engine failure {fail}; mean turns {turns:.1f}"
          + (f", mean decisions {sum(decs)/len(decs):.0f}" if decs else "") + "\n")
    st = collections.defaultdict(lambda: [0, 0, 0, 0, 0, 0])  # games, wins, g as P1, w as P1, g as P2, w as P2
    tsum = collections.defaultdict(int)
    for a, b, w, t, _ in games:
        if a == b or w < 0:
            continue
        for deck, seat in ((a, 1), (b, 2)):
            s = st[deck]
            s[0] += 1
            tsum[deck] += t
            won = w == seat
            s[1] += won
            if seat == 1:
                s[2] += 1; s[3] += won
            else:
                s[4] += 1; s[5] += won
    print("| list | games | win % | as P1 | as P2 | mean turns |")
    print("|---|---|---|---|---|---|")
    for deck in sorted(st, key=lambda d: -st[d][1] / st[d][0]):
        s = st[deck]
        print(f"| {deck} | {s[0]} | {100*s[1]/s[0]:.1f} | {100*s[3]/max(1,s[2]):.1f} | "
              f"{100*s[5]/max(1,s[4]):.1f} | {tsum[deck]/s[0]:.1f} |")


def matrix(games, out):
    names = sorted({g[0] for g in games} | {g[1] for g in games})
    w = collections.defaultdict(lambda: [0, 0])
    for a, b, win, _, _ in games:
        if win < 0:
            continue
        w[(a, b)][0] += win == 1; w[(a, b)][1] += 1
        w[(b, a)][0] += win == 2; w[(b, a)][1] += 1
    with open(out, "w", newline="") as f:
        cw = csv.writer(f)
        cw.writerow(["list"] + names)
        for r in names:
            cw.writerow([r] + [f"{w[(r, c)][0] / w[(r, c)][1]:.3f}" if w[(r, c)][1] else "" for c in names])


def main():
    args = sys.argv[1:]
    mat = None
    if "--matrix" in args:
        i = args.index("--matrix")
        mat = args[i + 1]
        del args[i:i + 2]
    first = None
    for spec in args:
        label, path = spec.split("=", 1)
        g = load(path)
        first = first or g
        table(label, g)
    if mat and first:
        matrix(first, mat)


if __name__ == "__main__":
    main()
