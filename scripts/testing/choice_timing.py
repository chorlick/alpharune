#!/usr/bin/env python3
"""Where each prompt of each card was asked: at finalization or at resolution.

Usage: choice_timing.py <trace-dir> <out.tsv>

Walks every log. A prompt line (MAY_PROMPT, TGT_PROMPT, TGT_PAIR_PROMPT_A/B,
MODE_PROMPT, SELECT_PROMPT, ASK_PROMPT) is attributed to the phase of the
chain item being handled when it was written: "finalize" after a
FINALIZE-CHOICES line, "resolve" after a RESOLVE line, "play" between a
SPELL/PLAY line and the item's first chain line (a play-time target). Output:
one row per (prompt kind, label) with the three counts and one example, most
resolve-time first. CR 355 / 383.3 / 403 decide targets, modes, move
destinations, a leading "you may" and a leading cost at finalization;
everything else at resolution — this table is what to read against them.
"""
import collections, os, re, sys

PROMPT = re.compile(r'\b(MAY_PROMPT|TGT_PROMPT|TGT_PAIR_PROMPT_[AB]|MODE_PROMPT|SELECT_PROMPT|ASK_PROMPT): (.*?)(?: \[| \(P=|$)')

def main():
    tdir, outp = sys.argv[1:3]
    rows = collections.defaultdict(lambda: collections.Counter())
    ex = {}
    for fn in sorted(f for f in os.listdir(tdir) if f.endswith(".log")):
        phase = "?"
        for line in open(os.path.join(tdir, fn), errors="replace"):
            if "FINALIZE-CHOICES:" in line:
                phase = "finalize"
            elif "RESOLVE:" in line:
                phase = "resolve"
            elif " SPELL: " in line or " PLAY: " in line:
                phase = "play"
            elif "CHOSE: [" in line and "MakeChoice" not in line:
                phase = "?"
            m = PROMPT.search(line)
            if m:
                key = (m.group(1), re.sub(r'\d+', 'N', m.group(2))[:70])
                rows[key][phase] += 1
                d = re.search(r'\[D:(\d+)\.', line)
                ex.setdefault((key, phase), f"{fn[:-4]} D:{d.group(1) if d else '?'}")
    with open(outp, "w") as out:
        out.write("kind\tlabel\tfinalize\tresolve\tplay\tother\texample(resolve or first)\n")
        for key, c in sorted(rows.items(), key=lambda kv: (-kv[1]["resolve"], kv[0])):
            e = ex.get((key, "resolve")) or next(v for (k, p), v in ex.items() if k == key)
            out.write(f"{key[0]}\t{key[1]}\t{c['finalize']}\t{c['resolve']}\t{c['play']}\t{c['?']}\t{e}\n")
    print(len(rows), "labels")

if __name__ == "__main__":
    main()
