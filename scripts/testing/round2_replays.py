#!/usr/bin/env python3
"""Replay the flagged and the longest games of deck_fuzz results, one at a time
under the run lane, and keep their trace tails.

Usage: round2_replays.py <out-dir> <result.json> [...] [--longest 3] [--tail 80]
                         [--binary build/deck_fuzz] [--lock <path>]

For each result: every game of `flagged` (long chain, turn cap, no winner, no
reason) and the `--longest` longest games that ended. Each replay is the
result's own command line (seeds, agents, guide) with `--tail` lines and its
own binary path replaced by --binary, written to <out-dir>/<kind>_<deckA>_<deckB>_<seed>.txt.
A replay already written is not run again. Prints one line per replay:
kind, decisions, turns, longest chain, legends, file.
"""
import argparse, fcntl, json, os, shlex, subprocess

LOCK = "/home/pirateporkchop/code/riftbound-engine/.worktrees/.run.lock"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("results", nargs="+")
    ap.add_argument("--longest", type=int, default=3)
    ap.add_argument("--tail", type=int, default=80)
    ap.add_argument("--binary", default="build/deck_fuzz")
    ap.add_argument("--lock", default=LOCK)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    seen = set()
    for path in a.results:
        d = json.load(open(path))
        games = [(k, g) for k, v in d["flagged"].items() for g in v]
        games += [("longest", g) for g in d["length"].get("longest", [])[:a.longest]]
        for kind, g in games:
            key = (g["deck_a"], g["deck_b"], g["game_seed"])
            if key in seen:
                continue
            seen.add(key)
            args = shlex.split(g["command"])
            args = [x for x in args if not x.startswith("RIFTBOUND_ROOT=")]
            args[0] = a.binary
            if "--tail" in args:
                i = args.index("--tail")
                args[i + 1] = str(a.tail)
            name = lambda d: os.path.basename(str(d)).replace(".txt", "")
            fn = os.path.join(a.out, f"{kind}_{name(g['deck_a'])}_{name(g['deck_b'])}_{g['game_seed']}.txt")
            if not os.path.exists(fn):
                with open(a.lock, "a") as lk:
                    fcntl.flock(lk, fcntl.LOCK_EX)
                    with open(fn, "w") as f:
                        f.write(" ".join(args) + "\n")
                        f.flush()
                        subprocess.call(args, stdout=f, stderr=subprocess.STDOUT,
                                        env=dict(os.environ, RIFTBOUND_ROOT="."))
                    fcntl.flock(lk, fcntl.LOCK_UN)
            print(f"{kind}\t{g['decisions']}\t{g['turns']}\t{g['longest_chain']}\t{g['legends']}\t{fn}", flush=True)


if __name__ == "__main__":
    main()
