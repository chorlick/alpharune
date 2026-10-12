#!/usr/bin/env python3
"""Print what is needed to read one card's behaviour against its text.

Usage:
  card_dossier.py <name> [--kinds play,trigger,...] [--n 2] [--after 40]
                  [--before 14] [--skip K] [--grep REGEX] [--board]
  env: TD_INV (inventory.json), TD_COV (coverage.json), TD_TR (trace dir)

Output: the card's def (cost, type, text), which lists hold it, the totals
from the coverage index, and for each requested kind up to N windows of the
game log around a sighting (different games), each headed by the command that
replays that game. --grep keeps only sightings whose window matches REGEX
(searched over every game of the index entry's kind, not only the stored
first sightings). --board also prints the board from the replay text at the
decision of the sighting.

Numbering: the log prefix "[D:027.03]" is the trace counter (what a replayed
game prints, and what findings cite); "DECISION #29" inside a line and the
replay text's "idx=29" are the engine's decision index, which runs ahead of
it. card_dossier.py --at <log-file-stem> <D> [--span N] prints the log lines
of decisions D..D+N of one game, --board adds the board at D.
"""
import argparse, json, os, re, sys

def load():
    S = os.environ.get("TD_DIR", ".")
    inv = json.load(open(os.environ.get("TD_INV", os.path.join(S, "inventory.json"))))
    cov = json.load(open(os.environ.get("TD_COV", os.path.join(S, "coverage.json"))))
    tr = os.environ.get("TD_TR", os.path.join(S, "tr"))
    return inv, cov, tr

def card_def(inv, name):
    for deck, secs in inv.items():
        for rows in secs.values():
            for r in rows:
                if r["card"]["name"] == name:
                    return r["card"]
    return None

def replay_cmd(fn):
    d1, d2, s, a1, a2 = fn[:-4].split("__")
    fix = lambda a: a.replace("ismcts-sims", "ismcts:sims=").replace("mcts-sims", "mcts:sims=")
    p = lambda d: (f"<sb>/{d}.txt" if d.endswith("_sb") else f"decks/{d}.txt")
    return (f"./build/riftbound --deck1 {p(d1)} --deck2 {p(d2)} --seed {s[1:]} "
            f"--agent1 {fix(a1)} --agent2 {fix(a2)} --trace --debug")

CLEAN = re.compile(r'^\s+\[(TRC|DBG|INF|WRN)\] ')
def window(lines, i, before, after):
    lo = max(0, i - before)
    for j in range(i, lo - 1, -1):
        if "CHOSE: [" in lines[j] and "MakeChoice" not in lines[j] and "PassPriority" not in lines[j]:
            lo = max(lo, j - 1)
            break
    hi = min(len(lines), i + after)
    out = []
    for j in range(lo, hi):
        mark = ">>" if j == i else "  "
        out.append(mark + CLEAN.sub(lambda m: ("d " if m.group(1) == "DBG" else ""), lines[j].rstrip("\n")))
    return out

def board_at(tr, fn, d):
    """Board, legal options and chosen option of trace decision `d` (the D of
    "[D:ddd.mm]"). The replay attaches "[D:ddd.00] DECISION ..." and the CHOSE
    line to the snapshot BEFORE the one that shows that decision's board, so
    this prints the first non-seam snapshot after the one holding that line."""
    path = os.path.join(tr, fn[:-4] + ".replay.txt")
    if not os.path.exists(path):
        return []
    snaps = open(path, errors="replace").read().split("=== SNAP ")[1:]
    tag = f"[D:{d:03d}.00]"
    for i, sn in enumerate(snaps):
        if tag in sn:
            for nxt in snaps[i + 1:]:
                if "[SEAM" in nxt.split("\n", 1)[0]:
                    continue
                return ("=== SNAP " + nxt.split("--- trace")[0]).rstrip("\n").split("\n")
    return []

PATS = {
    "play": lambda n: re.compile(r'(?:PLAY|SPELL): ' + re.escape(n) + r' \(id=|CHOSE BF #\d+ \(' + re.escape(n) + r'\)|CHANNELED: ' + re.escape(n) + r' \('),
    "hide": lambda n: re.compile(r'HIDE: ' + re.escape(n) + r' at BF#'),
    "trigger": lambda n: re.compile(r'TRIGGER: ' + re.escape(n) + r' (?:\(P\d\)|\[)'),
    "activate": lambda n: re.compile(r'Activate(?:Reaction)?Ability \(src=' + re.escape(n) + r'\)'),
    "finalize": lambda n: re.compile(r'FINALIZE-CHOICES: ' + re.escape(n) + r' \('),
    "res_abil": lambda n: re.compile(r'RESOLVE: ' + re.escape(n) + r' \(chain_item=\d+, ability=1'),
    "res_spell": lambda n: re.compile(r'RESOLVE: ' + re.escape(n) + r' \(chain_item=\d+, ability=0, spell=1'),
    "any": lambda n: re.compile(re.escape(n)),
}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("name")
    ap.add_argument("--at", nargs=2, metavar=("GAME", "D"))
    ap.add_argument("--span", type=int, default=6)
    ap.add_argument("--kinds", default="play,trigger,activate")
    ap.add_argument("--n", type=int, default=2)
    ap.add_argument("--before", type=int, default=14)
    ap.add_argument("--after", type=int, default=40)
    ap.add_argument("--skip", type=int, default=0)
    ap.add_argument("--grep")
    ap.add_argument("--board", action="store_true")
    ap.add_argument("--files", help="regex on the log file name")
    args = ap.parse_args()
    inv, cov, tr = load()
    if args.at:
        fn, d0 = args.at[0] + ".log", int(args.at[1])
        print("    " + replay_cmd(fn))
        if args.board:
            print("\n".join(board_at(tr, fn, d0)))
        for line in open(os.path.join(tr, fn), errors="replace"):
            m = re.search(r'\[D:(\d+)\.', line)
            if m and d0 <= int(m.group(1)) <= d0 + args.span:
                print(CLEAN.sub(lambda m: ("d " if m.group(1) == "DBG" else ""), line.rstrip("\n")))
        return
    c = card_def(inv, args.name)
    if not c:
        sys.exit("unknown card: " + args.name)
    e = cov[args.name]
    cost = f"[{c.get('energy_cost', '-')}]" + (f" + {c['power_cost']} Power" if c.get("power_cost") else "")
    print(f"### {c['name']}  #{c['id']}  {c['card_type']}{'/' + c['super_type'] if c.get('super_type') else ''}  cost {cost}  Might {c.get('might', '-')}"
          f"  domains {c.get('domains', [])}  tags {c.get('tags', [])}  keywords {c.get('keywords', [])}")
    print("TEXT: " + c.get("ability_text", "").replace("\n", "\n      "))
    if c.get("effect_text"):
        print("EFFECT TEXT: " + c["effect_text"])
    print("SOURCE: " + c["file"])
    print("LISTS: " + ", ".join(f"{d}({'/'.join(s)})" for d, s in sorted(e["decks"].items())))
    print("COUNTS: in_games=%d " % e["in_games"] + " ".join(f"{k}={v}" for k, v in e["count"].items()))
    gre = re.compile(args.grep) if args.grep else None
    fre = re.compile(args.files) if args.files else None
    for kind in args.kinds.split(","):
        pat = PATS[kind](args.name)
        shown = 0
        skipped = 0
        files = sorted(f for f in os.listdir(tr) if f.endswith(".log"))
        if fre:
            files = [f for f in files if fre.search(f)]
        # spread: start from the stored first sightings' games, then the rest
        pref = [f for f, _, _ in e["first"].get(kind, [])]
        order = [f for f in pref if f in files] + [f for f in files if f not in pref]
        for fn in order:
            if shown >= args.n:
                break
            lines = open(os.path.join(tr, fn), errors="replace").readlines()
            for i, line in enumerate(lines):
                if not pat.search(line):
                    continue
                w = window(lines, i, args.before, args.after)
                if gre and not any(gre.search(x) for x in w):
                    continue
                if skipped < args.skip:
                    skipped += 1
                    break
                m = re.search(r'\[D:(\d+)\.', line)
                dec = int(m.group(1)) if m else -1
                print(f"\n--- {kind} @ {fn[:-4]} decision {dec}")
                print("    " + replay_cmd(fn))
                if args.board:
                    print("\n".join(board_at(tr, fn, dec)))
                print("\n".join(w))
                shown += 1
                break
        if shown == 0:
            print(f"\n--- {kind}: no sighting")

if __name__ == "__main__":
    main()
