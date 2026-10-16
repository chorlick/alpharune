#!/usr/bin/env python3
"""Run the fuzz chunks of testing round 2, one at a time, each under the run lane.

Usage: round2_fuzz.py <plan.tsv> [--lock <path>] [--pause 30] [--only <name> ...]

plan.tsv, one chunk per line, tab separated:
    name  build-dir  out.json  arguments...
(the arguments are passed to <build-dir>/deck_fuzz as they are, one per
column). A chunk whose out.json exists is skipped, so the plan can be run
again after an interruption. Each chunk takes the run lane's lock
(flock(2) on --lock, the lock `flock(1)` takes) for its own duration only and
lets it go before the next; between chunks the driver waits --pause seconds
so another lane waiting on the lock gets it. stdout+stderr of a chunk go to
<out.json minus .json>.log; exit codes are appended to <plan>.done.tsv
(name, exit code, seconds, binary version line). Run from the repository root
(RIFTBOUND_ROOT=. is set for the child).
"""
import argparse, fcntl, os, subprocess, sys, time

LOCK = "/home/pirateporkchop/code/riftbound-engine/.worktrees/.run.lock"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("plan")
    ap.add_argument("--lock", default=LOCK)
    ap.add_argument("--pause", type=float, default=30)
    ap.add_argument("--only", nargs="*")
    a = ap.parse_args()
    rows = [l.rstrip("\n").split("\t") for l in open(a.plan) if l.strip() and not l.startswith("#")]
    env = dict(os.environ, RIFTBOUND_ROOT=".")
    first = True
    for row in rows:
        name, bdir, out = row[0], row[1], row[2]
        if a.only and name not in a.only:
            continue
        if os.path.exists(out):
            continue
        if not first:
            time.sleep(a.pause)
        first = False
        binary = os.path.join(bdir, "deck_fuzz")
        cmd = [binary] + row[3:] + ["--json", out]
        log = out[:-5] + ".log"
        with open(a.lock, "a") as lk:
            fcntl.flock(lk, fcntl.LOCK_EX)
            t0 = time.time()
            with open(log, "w") as lf:
                lf.write(" ".join(cmd) + "\n")
                lf.flush()
                rc = subprocess.call(cmd, stdout=lf, stderr=subprocess.STDOUT, env=env)
            dt = time.time() - t0
            fcntl.flock(lk, fcntl.LOCK_UN)
        st = os.stat(binary)
        with open(a.plan + ".done.tsv", "a") as d:
            d.write(f"{name}\t{rc}\t{dt:.0f}\t{binary}\tmtime={int(st.st_mtime)}\t{time.strftime('%H:%M:%S')}\n")
        print(f"{name}: rc={rc} {dt:.0f}s", flush=True)


if __name__ == "__main__":
    main()
