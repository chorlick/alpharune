#!/usr/bin/env python3
"""Tools for the 19-game reading: the game table and the citation check.

  games19_check.py table <plan.tsv> <trace-dir>
      One Markdown row per game of the plan: number, decks, seed, winner,
      final score, turns, decisions, and whether the log holds an engine error.

  games19_check.py cite <file.md> <log-dir> [--default gNN]
      Every `backticked` fragment that begins with a trace prefix "[D:012.03]"
      must occur, ignoring runs of spaces, as one line's text in the log of a
      game named in the same "### " block (g01..g19, <log-dir>/gNN.log). --default
      adds a game to every block (a reader's notes file is about one game). A fragment that is the beginning of a log line (a bare
      prefix "[D:146.03]", or a line cut short, with or without "...") is
      accepted and counted apart as "shortened". Prints every fragment not
      found and exits 1 if there is one.
"""
import os, re, sys

def norm(s):
    return re.sub(r"\s+", " ", s).strip()

def table(plan, trdir):
    k = 0
    print("| # | deck1 (P1) | deck2 (P2) | seed | winner | score P1-P2 | turns | decisions | engine error |")
    print("|---|---|---|---|---|---|---|---|---|")
    for line in open(plan):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        a, b, seed, a1, a2 = line.split("\t")
        k += 1
        clean = lambda x: x.replace(":", "-").replace("=", "")
        log = open(os.path.join(trdir, f"{a}__{b}__s{seed}__{clean(a1)}__{clean(a2)}.log"), errors="replace").read()
        over = re.search(r"Game over: winner=(\d+) \| reason=([^|]*)\| turns=(\d+)", log)
        scores = re.findall(r"SCORE: P(\d) \+\d+ via [^\n]*-> (\d+)", log)
        sc = {"1": 0, "2": 0}
        for p, v in scores:
            sc[p] = int(v)
        decs = re.findall(r"DECISION #(\d+)", log)
        err = "yes" if re.search(r"engine_error|exception|what\(\)", log) else "none"
        if over:
            w, turns = "P" + over.group(1), over.group(3)
        else:
            w, turns = "NONE", "?"
        print(f"| {k} | {a} | {b} | {seed} | {w} | {sc['1']}-{sc['2']} | {turns} | {decs[-1] if decs else 0} | {err} |")

def cite(path, logdir, default):
    logs = {}
    def lines_of(g):
        if g not in logs:
            p = os.path.join(logdir, g + ".log")
            logs[g] = [norm(re.sub(r"^\s*\[(TRC|DBG|INF|WRN)\]\s*", "", l)) for l in open(p, errors="replace")] if os.path.exists(p) else None
        return logs[g]
    text = open(path, encoding="utf-8").read()
    total = bad = short = 0
    for block in re.split(r"\n(?=### )", text):
        games = sorted(set(re.findall(r"\bg(?:0[1-9]|1[0-9])\b", block)) | ({default} if default else set()))
        head = block.strip().split("\n")[0][:90]
        for frag in re.findall(r"`(\[D:[^`]*)`", block):
            total += 1
            f = norm(re.sub(r"^\[D:([0-9.]+)\]\s*", r"[D:\1] ", frag))
            if f.endswith("..."):
                f = f[:-3].rstrip()
            ok = False
            for g in games:
                ls = lines_of(g)
                if ls and any(l == f for l in ls):
                    ok = True
                    break
                if ls and any(l.startswith(f) for l in ls):
                    ok = True
                    short += 1
                    break
            if not ok:
                bad += 1
                print(f"NOT FOUND in {','.join(games) or '(no game named)'}: {frag}\n    block: {head}")
    print(f"{total} quoted trace lines, {short} shortened, {bad} not found")
    sys.exit(1 if bad else 0)

def main():
    if sys.argv[1] == "table":
        table(sys.argv[2], sys.argv[3])
    elif sys.argv[1] == "cite":
        default = sys.argv[sys.argv.index("--default") + 1] if "--default" in sys.argv else None
        cite(sys.argv[2], sys.argv[3], default)
    else:
        sys.exit(__doc__)

if __name__ == "__main__":
    main()
