#!/usr/bin/env python3
"""One compact reading file per game from the replay text.

Usage: games19_reading.py <game.replay.txt> <game.read.txt>

The replay text (replay_to_text.py) holds, per snapshot, the rendered board
(every hand and facedown card named), the legal options with the one chosen,
and the trace lines that followed. Its boards are padded boxes that mostly
repeat the one before. This writes the same content without the repetition:

  === SNAP <n> idx=<decision index> | <turn line>
  board:        every board line is labelled with its section ("P1", "P1 BASE",
                "BF <name>", "P2 BASE", "P2"); wrapped lines are joined.
                The first board of every turn is printed in full ("board FULL");
                otherwise only the lines that differ from the board before
                ("board CHANGES": "~" a line as it is now, "-" a line that is
                gone), or "board: (same)".
  decision:     the legal options and the option chosen, as rendered
  trace:        the log lines up to the next snapshot, "[D:012.03]" prefixes kept

Every option list and every trace line of the replay text is in the output.
The replay ends at the last decision: the few log lines after it (the last
resolution, the winning point, "Game over") are in the .log only.
"""
import re, sys

LABELS = ("Hand:", "Trash:", "Banish:", "Units:", "Runes:", "Gear:", ">>", "P1:", "P2:", "[HIDDEN")

def board_lines(lines):
    """Box removed, wrapped lines joined, every line prefixed with its section."""
    out, sec = [], ""
    for ln in lines:
        s = ln.rstrip()
        if s.startswith("+") and set(s) <= set("+=-"):
            continue
        if s.startswith("|"):
            s = s[1:]
        if s.endswith("|"):
            s = s[:-1]
        s = s.rstrip()
        if not s.strip():
            continue
        body = s.strip()
        if body.startswith("TURN "):
            out.append(body)
            continue
        m = re.match(r"^ (P[12])  \S", s)
        if m:
            sec = m.group(1)
            out.append(f"{sec} | {body}")
            continue
        if re.match(r"^ P[12] BASE", s):
            sec = body
            continue
        if "[Ctrl:" in body and not s.startswith("   "):
            sec = "BF " + body
            out.append(sec)
            continue
        if body.startswith(LABELS) or not out:
            out.append(f"{sec.split('  [Ctrl')[0]} | {body}")
        else:  # a wrapped continuation of the line above
            out[-1] += "  " + body
    return out

def key(line):
    """What identifies a board line across boards: its section and label."""
    head, _, rest = line.partition(" | ")
    lab = rest.split(":")[0] if ":" in rest[:16] else rest[:12]
    return (head, lab)

def main():
    src, dst = sys.argv[1:3]
    snaps, cur = [], None
    for raw in open(src, encoding="utf-8"):
        ln = raw.rstrip("\n")
        if ln.startswith("=== SNAP "):
            cur = {"head": ln, "board": [], "decision": [], "trace": [], "sec": None}
            snaps.append(cur)
        elif cur is None:
            continue
        elif ln in ("--- board", "--- decision", "--- trace"):
            cur["sec"] = ln[4:]
        elif cur["sec"]:
            cur[cur["sec"]].append(ln)
    last, last_turn = None, None
    with open(dst, "w", encoding="utf-8") as f:
        for s in snaps:
            f.write(s["head"] + "\n")
            b = board_lines(s["board"])
            body = [x for x in b if not x.startswith("TURN ")]
            turn = re.search(r"\| Turn (\d+) \|", s["head"])
            turn = turn.group(1) if turn else None
            if not b:
                f.write("board: (none rendered)\n")
            elif body == last and turn == last_turn:
                f.write("board: (same)\n")
            elif last is None or turn != last_turn:
                f.write("board FULL:\n" + "\n".join("  " + x for x in body) + "\n")
            else:
                old = set(last)
                now_keys = set(key(x) for x in body)
                ch = ["  ~ " + x for x in body if x not in old]
                gone = ["  - " + x for x in last if x not in set(body) and key(x) not in now_keys]
                f.write("board CHANGES:\n" + "\n".join(ch + gone) + "\n")
            if b:
                last, last_turn = body, turn
            d = [x.rstrip() for x in s["decision"] if x.strip()]
            f.write("decision:\n" + "\n".join(d) + "\n" if d else "decision: (none)\n")
            t = [re.sub(r"^\[TRC\] ", "", x) for x in s["trace"] if x.strip()]
            if t:
                f.write("trace:\n" + "\n".join(t) + "\n")

if __name__ == "__main__":
    main()
