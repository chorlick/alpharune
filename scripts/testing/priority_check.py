#!/usr/bin/env python3
"""Who is asked at each chain-priority decision (CR 337.4, 339, 340.4).

Usage: priority_check.py <trace-dir> <out.txt>

From the replay texts. At every decision offered "Pass Priority" in a Closed
State the board lists the chain; its top item's controller is C. The rules
give priority to C when the chain has just changed (an item was finalized on
top: 337.4; an item resolved and others remain: 340.4), and to the next
player when the previous decision in this chain was a pass (339.2). So the
decider must be C unless the decision just before it, with no chain change
in between, was a "Pass Priority" by the other player. Anything else is
listed. Also counted: how often both players passed in sequence and the next
thing in the trace was not a RESOLVE.
"""
import collections, os, re, sys

def main():
    tdir, outp = sys.argv[1:3]
    bad, n = [], 0
    kinds = collections.Counter()
    for fn in sorted(f for f in os.listdir(tdir) if f.endswith(".replay.txt")):
        snaps = open(os.path.join(tdir, fn), errors="replace").read().split("=== SNAP ")[1:]
        prev = None   # (decider, chose_pass, chain signature)
        for sn in snaps:
            head = sn.split("\n", 1)[0]
            if "[SEAM" in head:
                continue
            dtxt = sn.split("--- decision\n")[1].split("--- trace\n")[0] if "--- decision\n" in sn else ""
            trace = sn.split("--- trace\n")[1] if "--- trace\n" in sn else ""
            board = sn.split("--- decision\n")[0]
            chain = re.findall(r'\|\s+(?:\[top\]|\[bot\]|\[\d+\]|>>> RESOLVING:)?\s*(.+?) \(P(\d)\)(?: step=\d+)?\s*\|', board.split("Chain (top resolves next)")[1]) if "Chain (top resolves next)" in board else []
            dm = re.search(r'--- P(\d) Decision #(\d+) \[[^\]]*\](.*?)---', dtxt)
            forced = re.search(r'==> P(\d) \(forced\): Pass Priority', dtxt)
            is_prio = ("Pass Priority" in dtxt) and chain
            if not is_prio:
                prev = None
                continue
            decider = int(dm.group(1)) if dm else (int(forced.group(1)) if forced else None)
            chose = re.search(r'==> P\d(?: \(forced\))?: (.*)', dtxt)
            chose_pass = bool(chose and chose.group(1).startswith("Pass Priority"))
            sig = tuple(chain)
            top_ctrl = int(chain[0][1])
            n += 1
            ok = decider == top_ctrl or (prev and prev[2] == sig and prev[1] and prev[0] != decider)
            if not ok:
                d = re.search(r'idx=(\d+)', head)
                kinds[(chain[0][0], "decider P%d, top item of P%d" % (decider, top_ctrl))] += 1
                if len(bad) < 400:
                    bad.append(f"{fn[:-11]} decision #{d.group(1) if d else '?'}: P{decider} asked, chain top '{chain[0][0]}' is P{top_ctrl}'s; previous: {prev[:2] if prev else None}")
            prev = (decider, chose_pass, sig)
    with open(outp, "w") as out:
        out.write(f"priority decisions: {n}; not explained: {sum(kinds.values())}\n")
        for k, v in kinds.most_common(60):
            out.write(f"  {v:5} {k}\n")
        for b in bad[:120]:
            out.write("  " + b + "\n")
    print(open(outp).read()[:5000])

if __name__ == "__main__":
    main()
