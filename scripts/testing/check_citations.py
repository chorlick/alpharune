#!/usr/bin/env python3
"""Check that the trace lines quoted in a findings file exist in the games cited.

Usage: check_citations.py <findings.md> <trace-dir> [<trace-dir> ...]

Splits the file at "### " headings. In each block it collects the game names
(<deck>__<deck>__s<seed>__<agent>__<agent>, or the short form without the
agents) and every `backticked` fragment that begins with a trace tag
("[D:012.03] ..."). A fragment is confirmed when, ignoring runs of spaces, it
occurs in the log of one of the games named in the same block. Fragments
without a "[D:" tag are searched as plain text in those games' logs and
replay texts (option lists come from the replay). Prints every fragment that
could not be found; exit status 1 if there is one.
"""
import os, re, sys

def norm(s):
    return re.sub(r'\s+', ' ', s).strip()

def main():
    path, dirs = sys.argv[1], sys.argv[2:]
    files = {}
    for d in dirs:
        for fn in os.listdir(d):
            if fn.endswith(".log"):
                files[fn[:-4]] = d
    text = open(path, encoding="utf-8").read()
    blocks = re.split(r'\n(?=### )', text)
    game_re = re.compile(r'\b([a-z_]+__[a-z_]+__s\d+(?:__[a-z0-9\-]+__[a-z0-9\-]+)?)')
    total = missing = nogame = 0
    cache = {}
    def load(g):
        if g not in cache:
            d = files[g]
            log = norm(open(os.path.join(d, g + ".log"), errors="replace").read())
            rp = os.path.join(d, g + ".replay.txt")
            rep = norm(open(rp, errors="replace").read()) if os.path.exists(rp) else ""
            cache[g] = (log, rep)
            if len(cache) > 6:
                cache.pop(next(iter(cache)))
        return cache[g]
    for b in blocks:
        title = b.split("\n", 1)[0][:90]
        games = []
        for g in game_re.findall(b):
            cands = [k for k in files if k == g or k.startswith(g + "__")]
            games += cands
        games = list(dict.fromkeys(games))
        frags = [f for f in re.findall(r'`([^`\n]{12,})`', b)]
        for f in frags:
            tagged = f.lstrip().startswith("[D:")
            looks = tagged or re.match(r'\s*(?:d )?(?:[A-Z][A-Z_\' ,\-]+: |CHOSE|INTENT|SPELL|PLAY|RESOLVE|TRIGGER|PAY_|EFFECT|KILL|MOVE|DECISION #|TGT_|MAY_|TARGET|FINALIZE|SCORE|CONQUER|COMBAT|DAMAGE_STEP|ASSIGN|SHOWDOWN|TOKEN|ATTACH|HIDE|XP|RECALL)', f)
            if not looks:
                continue
            total += 1
            if not games:
                nogame += 1
                continue
            nf = norm(re.sub(r'\s*\.\.\.\s*$', '', f))
            nf = re.sub(r'^d ', '', nf)
            parts = [p for p in (norm(x) for x in nf.split(" ... ")) if len(p) > 6]
            ok = False
            for g in games:
                log, rep = load(g)
                if all((p in log) or (p in rep) for p in parts):
                    ok = True
                    break
            if not ok:
                missing += 1
                print(f"NOT FOUND  [{title}]\n    {f[:200]}\n    games: {games[:3]}")
    print(f"{path}: {total} quoted trace fragments, {missing} not found, {nogame} in a block that names no game")
    sys.exit(1 if missing else 0)

if __name__ == "__main__":
    main()
