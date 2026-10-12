#!/usr/bin/env python3
"""Write a post-sideboard variant of every list in decks/ (Tournament Rules 403.4).

Usage: sideboarded_decks.py <decks-dir> <out-dir>

The game-one configuration never plays a sideboard card, so those cards would
go unobserved. For each `<deck>.txt` this writes `<out-dir>/<deck>_sb.txt` in
which every sideboard card is in the Main Deck, exchanged 1 for 1: copies are
taken out of the Main Deck one at a time, each time from the card that has
the most copies left (first in list order on a tie), never the last copy of a
card. The cards taken out become the new sideboard. Legend, Chosen Champion,
runes and battlefields are unchanged (403.4.b). Deterministic: no randomness.
"""
import os, sys, collections

def parse(path):
    secs, order, sec = collections.OrderedDict(), [], None
    for raw in open(path):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.endswith(":"):
            sec = line[:-1]
            secs[sec] = []
            continue
        n, name = line.split(None, 1)
        secs[sec].append([int(n), name.strip()])
    return secs

def main():
    src, out = sys.argv[1:3]
    os.makedirs(out, exist_ok=True)
    for fn in sorted(os.listdir(src)):
        if not fn.endswith(".txt"):
            continue
        secs = parse(os.path.join(src, fn))
        main_rows = secs["MainDeck"]
        side = secs.get("Sideboard", [])
        need = sum(n for n, _ in side)
        removed = collections.OrderedDict()
        side_names = {name for _, name in side}
        for _ in range(need):
            best = None
            for row in main_rows:
                if row[1] in side_names or row[0] <= 1:
                    continue
                if best is None or row[0] > best[0]:
                    best = row
            if best is None:
                sys.exit(f"{fn}: cannot exchange {need} cards without removing a last copy")
            best[0] -= 1
            removed[best[1]] = removed.get(best[1], 0) + 1
        for n, name in side:
            for row in main_rows:
                if row[1] == name:
                    row[0] += n
                    break
            else:
                main_rows.append([n, name])
        secs["Sideboard"] = [[n, name] for name, n in removed.items()]
        with open(os.path.join(out, fn[:-4] + "_sb.txt"), "w") as f:
            f.write(f"# {fn} after exchanging the whole sideboard 1 for 1 (scripts/testing/sideboarded_decks.py)\n")
            for sec, rows in secs.items():
                f.write(f"{sec}:\n")
                for n, name in rows:
                    f.write(f"{n} {name}\n")
                f.write("\n")
    print("ok")

if __name__ == "__main__":
    main()
