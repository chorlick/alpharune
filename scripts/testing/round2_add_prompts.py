#!/usr/bin/env python3
"""Put what each decision asks into a games19 reading file.

Usage: round2_add_prompts.py <game.read.txt> <game.prompts.log> <out.read.txt>

<game.prompts.log> is build/prompt_trace's output for the same game (checked
equal by round2_prompt_runs.py). Its "[PRM] #n ..." blocks are the
describePrompt text of the decision with decision index n, for the player
deciding, with every option (the chosen one marked "*"). Each SNAP of the
reading file whose idx is n gets the block as a "prompt:" section after its
"decision:" section. Decisions the replay did not snapshot (a single option is
not rendered) keep their prompt in the log only. Phase-seam SNAPs ("[SEAM ...]")
repeat the idx of the decision before them and get no prompt. Prints how many SNAPs got a
prompt and how many prompts had no SNAP.
"""
import re, sys


def prompts(path):
    out, cur = {}, None
    for ln in open(path, errors="replace"):
        m = re.match(r"^  \[PRM\] #(\d+) (.*)$", ln.rstrip("\n"))
        if m:
            cur = [m.group(2)]
            out.setdefault(int(m.group(1)), []).append(cur)
            continue
        m = re.match(r"^  \[PRM\]   (.*)$", ln.rstrip("\n"))
        if m and cur is not None:
            cur.append("  " + m.group(1))
    return out


def main():
    src, plog, dst = sys.argv[1:4]
    P = prompts(plog)
    used = set()
    with open(dst, "w") as f:
        idx, in_decision = None, False
        lines = open(src, errors="replace").read().split("\n")
        for i, ln in enumerate(lines):
            if ln.startswith("=== SNAP "):
                m = re.search(r"idx=(-?\d+)", ln)
                idx = int(m.group(1)) if m and "[SEAM " not in ln else None
            f.write(ln + ("\n" if i + 1 < len(lines) else ""))
            nxt = lines[i + 1] if i + 1 < len(lines) else "=== SNAP "
            if ln.startswith("decision:"):
                in_decision = True
            if in_decision and (nxt.startswith("trace:") or nxt.startswith("=== SNAP ") or i + 1 == len(lines)):
                in_decision = False
                if idx in P:
                    used.add(idx)
                    for blk in P[idx]:
                        f.write("prompt: " + blk[0] + "\n" + "\n".join(blk[1:]) + "\n")
    print(f"{dst}: {len(used)} SNAPs with a prompt; {len(set(P) - used)} prompts without a SNAP")


if __name__ == "__main__":
    main()
