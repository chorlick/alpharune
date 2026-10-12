#!/usr/bin/env python3
"""Which log lines name a card that is hidden at that moment?

Usage: hidden_info_scan.py <trace-dir> <out.txt> [max-games]

The CLI prints every log line without its audience, so a seat's view cannot
be replayed from stdout. This scan does the next best thing from the replay
texts (which name every hand and facedown card): for every trace line it
asks whether the line names a card that, on the board rendered just AFTER the
line, is in a hand or facedown and is nowhere public (no copy on the board,
in a trash, in banishment or on the chain, before or after). Lines of the
kinds the engine emits privately by construction are counted apart:
  "  DREW:", "  PEEKED:", "  MULLIGAN:", "HIDE: <card> at", "HIDDEN: <card> hidden at".
Everything else is grouped by line kind with an example, to be read against
the emitting statement (RIFT_TRACE = both players; RIFT_TRACE_PRIVATE /
RIFT_TRACE_FOR / RIFT_LOG_FOR = an audience).
"""
import collections, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from deck_level_checks import parse_board, abbr

PRIVATE_KINDS = [re.compile(p) for p in (
    r'^\s+DREW: ', r'^\s+PEEKED: ', r'^\s+MULLIGAN: ', r'^HIDE: .+ at BF#\d+ by', r'^HIDDEN: .+ hidden at',
)]
# kinds whose audience is computed from the options (Card::promptAudience);
# the matching TGT_PICKED line is a plain RIFT_TRACE and is listed with the rest
PROMPT_KINDS = re.compile(r'^(TGT_PROMPT|TGT_PAIR_PROMPT_[AB]|SELECT_PROMPT)')

def public_names(b, chain):
    pub = set()
    cells = []
    for p in (1, 2):
        z = b[p]
        pub.update(z["trash"]); pub.update(z["banish"])
        cells += z["board_units"] + z["board_gear"] + z["bf_units"]
    pub_cells = {re.sub(r'\(.*$', '', c).strip() for c in cells}
    return pub, pub_cells, {n for n, _ in chain}

def hidden_names(b):
    hid = {}
    for p in (1, 2):
        for cell in b[p]["hand"]:
            hid.setdefault(re.sub(r'(\(\d+M\)|\[[SG]\]).*$', '', cell).strip(), set()).add(p)
        for n in b[p]["facedown"]:
            hid.setdefault(n, set()).add(p)
    return hid

def main():
    tdir, outp = sys.argv[1:3]
    maxg = int(sys.argv[3]) if len(sys.argv) > 3 else 10 ** 9
    kinds = collections.Counter()
    private_kinds = collections.Counter()
    prompt_kinds = collections.Counter()
    example = {}
    files = sorted(f for f in os.listdir(tdir) if f.endswith(".replay.txt"))[:maxg]
    for fn in files:
        snaps = open(os.path.join(tdir, fn), errors="replace").read().split("=== SNAP ")[1:]
        boards = []
        for sn in snaps:
            parts = sn.split("--- decision\n")
            board_txt = parts[0].split("--- board\n")[1] if "--- board\n" in parts[0] else ""
            trace = sn.split("--- trace\n")[1] if "--- trace\n" in sn else ""
            boards.append((parse_board(board_txt.split("\n")) if board_txt else None, trace))
        for i, (bd, trace) in enumerate(boards):
            nxt = next((b for b, _ in boards[i + 1:] if b), None)
            if not nxt or not bd:
                continue
            hid = hidden_names(nxt[0])
            pub_a, cells_a, chain_a = public_names(nxt[0], nxt[2])
            pub_b, cells_b, chain_b = public_names(bd[0], bd[2])
            hid_b = hidden_names(bd[0])
            for line in trace.split("\n"):
                m = re.match(r'\[(TRC|DBG|INF|WRN)\] \[D:(\d+)\.\d+\] ?(.*)$', line)
                if not m:
                    continue
                body = m.group(3)
                for n in hid:
                    if n not in body or len(n) < 4:
                        continue
                    if n in pub_a or n in pub_b or abbr(n) in cells_a or abbr(n) in cells_b or n in chain_a or n in chain_b:
                        continue
                    kind = re.sub(r'[:(].*$', '', body.strip())[:40]
                    full_kind = re.sub(r'\d+', 'N', body.strip().split(n)[0])[:60]
                    if any(p.search(body) for p in PRIVATE_KINDS):
                        private_kinds[kind] += 1
                    elif PROMPT_KINDS.search(body.strip()):
                        prompt_kinds[kind] += 1
                    else:
                        kinds[full_kind] += 1
                        example.setdefault(full_kind, f"{fn[:-11]} D:{m.group(2)}: {body[:150]}")
                    break
    with open(outp, "w") as out:
        out.write(f"games scanned: {len(files)}\n\nprivate by construction:\n")
        for k, v in private_kinds.most_common():
            out.write(f"  {v:6} {k}\n")
        out.write("\nprompt lines (audience computed by Card::promptAudience):\n")
        for k, v in prompt_kinds.most_common():
            out.write(f"  {v:6} {k}\n")
        out.write("\nother lines naming a card that is hidden right after the line:\n")
        for k, v in kinds.most_common():
            out.write(f"  {v:6} {k!r}\n         e.g. {example[k]}\n")
    print(open(outp).read()[:7000])

if __name__ == "__main__":
    main()
