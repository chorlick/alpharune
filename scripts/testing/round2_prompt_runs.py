#!/usr/bin/env python3
"""Play the games of a run_deck_games plan again with build/prompt_trace and keep
what every decision asks, then check that it is the same game.

Usage: round2_prompt_runs.py --out <dir> --plan <plan.tsv> [--threads 3]
                             [--binary build/prompt_trace]

For game k of the plan (deck1 deck2 seed agent1 agent2) writes
<out>/gNN.prompts.log (the trace with the "[PRM]" lines). The check: the
"[TRC]"/"[DBG]" lines of that log must equal, in order, those of the
run_deck_games log of the same game (<out>/<d1>__<d2>__s<seed>__<a1>__<a2>.log);
a game that differs is reported and its prompts are not to be used.
Prints one line per game: name, exit code, equal or not, PRM decisions.
"""
import argparse, concurrent.futures, os, subprocess

def clean(a):
    return a.replace(":", "-").replace("=", "")

def trace_lines(path):
    out = []
    for ln in open(path, errors="replace"):
        s = ln.strip()
        if s.startswith("[TRC]") or s.startswith("[DBG]") or s.startswith("[INF]") or s.startswith("[WRN]"):
            out.append(s)
    return out

def run(args, k, row):
    d1, d2, seed, a1, a2 = row
    name = f"g{k:02d}"
    path = os.path.join(args.out, name + ".prompts.log")
    if not (os.path.exists(path) and os.path.getsize(path) > 0):
        cmd = [args.binary, "--deck1", f"decks/{d1}.txt", "--deck2", f"decks/{d2}.txt",
               "--agent1", a1, "--agent2", a2, "--seed", seed]
        with open(path, "w") as f:
            rc = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT,
                                env=dict(os.environ, RIFTBOUND_ROOT=".")).returncode
    else:
        rc = 0
    ref = os.path.join(args.out, f"{d1}__{d2}__s{seed}__{clean(a1)}__{clean(a2)}.log")
    same = os.path.exists(ref) and trace_lines(ref) == trace_lines(path)
    prm = sum(1 for ln in open(path, errors="replace") if ln.startswith("  [PRM] #"))
    return f"{name}\t{rc}\t{'equal' if same else 'DIFFERENT'}\t{prm}"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--plan", required=True)
    ap.add_argument("--threads", type=int, default=3)
    ap.add_argument("--binary", default="build/prompt_trace")
    a = ap.parse_args()
    rows = [tuple(l.strip().split("\t")) for l in open(a.plan) if l.strip() and not l.startswith("#")]
    with concurrent.futures.ThreadPoolExecutor(min(3, a.threads)) as ex:
        for line in ex.map(lambda kr: run(a, kr[0], kr[1]), enumerate(rows, 1)):
            print(line, flush=True)

if __name__ == "__main__":
    main()
