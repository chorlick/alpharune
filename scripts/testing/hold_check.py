#!/usr/bin/env python3
"""Holds against battlefield control at the Scoring Step (CR 315.2.b, 471.2).

Usage: hold_check.py <trace-dir> <out.txt>

For every turn of every game: the battlefields the turn player controls on
the board rendered as the Scoring Step begins (the replay's seam snapshot)
must be exactly the battlefields the log shows that player Holding in that
Scoring Step ("SCORE: Px +1 via Hold BF#n", or "SCORE_GATED" / "SCORE_BLOCKED"
for a hold that scores nothing). Reports both kinds of mismatch; a turn in
which a hold took the player to the victory score and ended the game before
the remaining holds is counted apart.
"""
import collections, os, re, sys

def main():
    tdir, outp = sys.argv[1:3]
    bad = []
    n_turns = n_holds = n_ended = 0
    for fn in sorted(f for f in os.listdir(tdir) if f.endswith(".log")):
        holds = collections.defaultdict(set)
        other = collections.defaultdict(list)
        won_in_scoring = set()
        turn, in_scoring, tp = None, False, None
        for line in open(os.path.join(tdir, fn), errors="replace"):
            m = re.search(r'════ TURN (\d+) \(P(\d)\)', line)
            if m:
                turn, tp = int(m.group(1)), int(m.group(2)); in_scoring = False
                continue
            if "── Scoring Step ──" in line:
                in_scoring = True; continue
            if "── Channel Phase ──" in line:
                in_scoring = False; continue
            if in_scoring:
                h = re.search(r'SCORE: P(\d) \+1 via Hold BF#(\d+)', line)
                if h:
                    holds[turn].add(int(h.group(2)))
                    if line.rstrip().endswith("-> 8"):
                        won_in_scoring.add(turn)
                g = re.search(r'(SCORE_GATED|SCORE_BLOCKED|SCORE_REPLACED)[^B]*(?:BF#(\d+))?', line)
                if g:
                    other[turn].append(line.strip()[-90:])
                    if g.group(2) is not None:
                        holds[turn].add(int(g.group(2)))
        rp = os.path.join(tdir, fn[:-4] + ".replay.txt")
        if not os.path.exists(rp):
            continue
        for sn in open(rp, errors="replace").read().split("=== SNAP ")[1:]:
            head = sn.split("\n", 1)[0]
            m = re.search(r'Turn (\d+) \| ▶ ScoringStep begins \| P(\d)', head)
            if not m:
                continue
            t, p = int(m.group(1)), int(m.group(2))
            ctrl = [c for c in re.findall(r'\[Ctrl:(P\d|--)\]', sn)]
            mine = {i for i, c in enumerate(ctrl) if c == f"P{p}"}
            n_turns += 1
            n_holds += len(holds.get(t, ()))
            if mine != holds.get(t, set()) and t in won_in_scoring and holds[t] < mine:
                n_ended += 1
            elif mine != holds.get(t, set()):
                bad.append(f"{fn[:-4]} turn {t} (P{p}): controls {sorted(mine)} at the Scoring Step, log holds {sorted(holds.get(t, set()))} {other.get(t, '')}")
    with open(outp, "w") as out:
        out.write(f"turns checked: {n_turns}, holds: {n_holds}, turns where a hold ended the game before the rest: {n_ended}, mismatches: {len(bad)}\n")
        for b in bad:
            out.write("  " + b + "\n")
    print(open(outp).read()[:5000])

if __name__ == "__main__":
    main()
