#!/usr/bin/env python3
"""Turn an HTML replay (`riftbound --render-html on`) into plain text.

Usage: replay_to_text.py <replay.html> <out.txt>

One block per snapshot, in order:
  === SNAP <n> idx=<decision_index> | <turn info> [SEAM <phase>]
  --- board      the ASCII board as rendered BEFORE the decision is applied
  --- decision   the legal options and the one chosen
  --- trace      the log lines the chosen option caused (up to the next snapshot)
The replay is written with --show-hand, so the board names every hand and
facedown card: it is the omniscient record, not a seat's view.
"""
import re, sys

UNESC = re.compile(r'\\(x3c|.)')
def unesc(s):
    def rep(m):
        t = m.group(1)
        return {"n": "\n", "t": "\t", "x3c": "<", '"': '"', "\\": "\\"}.get(t, t)
    return UNESC.sub(rep, s)

STR = r'"((?:[^"\\]|\\.)*)"'
SNAP = re.compile(
    r'\{\n  idx: (-?\d+),\n  turn: ' + STR + r',\n  board: ' + STR +
    r',\n  decision: ' + STR + r',\n  trace: \[(.*?)\],\n  v: [^\n]*seam: (\d), phase: ' + STR + r'\n\}',
    re.S)

def convert(src, dst):
    html = open(src, encoding="utf-8", errors="replace").read()
    start = html.index("const SNAPSHOTS = [")
    body = html[start:]
    n = 0
    with open(dst, "w", encoding="utf-8") as out:
        for m in SNAP.finditer(body):
            idx, turn, board, decision, trace, seam, phase = m.groups()
            out.write(f"=== SNAP {n} idx={idx} | {unesc(turn)}" + (f" [SEAM {unesc(phase)}]" if seam == "1" else "") + "\n")
            if board:
                out.write("--- board\n" + unesc(board).rstrip("\n") + "\n")
            out.write("--- decision\n" + unesc(decision).rstrip("\n") + "\n")
            out.write("--- trace\n")
            for t in re.finditer(STR, trace):
                out.write(unesc(t.group(1)) + "\n")
            n += 1
    return n

if __name__ == "__main__":
    print(convert(sys.argv[1], sys.argv[2]), "snapshots")
