#!/usr/bin/env python3
"""Honest-ISMCTS round robin of the deck lists (testing round 2, part 2).

Usage: round2_ismcts_rr.py <out.tsv> [--decks decks] [--games 2] [--seed 2151030000]
                           [--agent ismcts:sims=200] [--binary build/riftbound]
                           [--workers 3] [--hold 720] [--pause 30] [--lock <path>]

Every ordered pairing of the *.txt lists (mirrors included), `--games` games
each, the same agent in both seats. Game g of pairing number p (pairings in
sorted order, deck1 outer) has engine seed seed + p * games + g. One process
per game (`riftbound --deck1 .. --deck2 .. --agent1 A --agent2 A --seed S`);
`--workers` at a time. The run lane's lock is taken for at most --hold
seconds of starting games, then let go once the running games end, and taken
again after --pause seconds, so another lane gets the lane between holds.
One line per game is appended to out.tsv:
    deck1 deck2 seed rc winner reason turns seconds
A game already in out.tsv is not played again. A game whose exit code is not
0 keeps its output in <out.tsv>.err/<deck1>__<deck2>__<seed>.txt.
"""
import argparse, concurrent.futures, fcntl, os, re, subprocess, time

LOCK = "/home/pirateporkchop/code/riftbound-engine/.worktrees/.run.lock"


def play(a, d1, d2, seed):
    cmd = [a.binary, "--deck1", f"{a.decks}/{d1}.txt", "--deck2", f"{a.decks}/{d2}.txt",
           "--agent1", a.agent, "--agent2", a.agent, "--seed", str(seed), "--render-html", "off"]
    t0 = time.time()
    p = subprocess.run(cmd, capture_output=True, text=True, env=dict(os.environ, RIFTBOUND_ROOT="."))
    dt = time.time() - t0
    m = re.search(r"Game over: winner=(\d+) \| reason=([^|]*)\| turns=(\d+)", p.stdout)
    winner, reason, turns = (m.group(1), m.group(2).strip(), m.group(3)) if m else ("-", "-", "-")
    if p.returncode != 0 or not m:
        os.makedirs(a.out + ".err", exist_ok=True)
        with open(f"{a.out}.err/{d1}__{d2}__{seed}.txt", "w") as f:
            f.write(" ".join(cmd) + "\n" + p.stdout + "\n--- stderr\n" + p.stderr)
    return f"{d1}\t{d2}\t{seed}\t{p.returncode}\t{winner}\t{reason}\t{turns}\t{dt:.1f}\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--decks", default="decks")
    ap.add_argument("--games", type=int, default=2)
    ap.add_argument("--seed", type=int, default=2151030000)
    ap.add_argument("--agent", default="ismcts:sims=200")
    ap.add_argument("--binary", default="build/riftbound")
    ap.add_argument("--workers", type=int, default=3)
    ap.add_argument("--hold", type=float, default=720)
    ap.add_argument("--pause", type=float, default=30)
    ap.add_argument("--lock", default=LOCK)
    a = ap.parse_args()
    names = sorted(f[:-4] for f in os.listdir(a.decks) if f.endswith(".txt"))
    todo = []
    p = 0
    for d1 in names:
        for d2 in names:
            for g in range(a.games):
                todo.append((d1, d2, a.seed + p * a.games + g))
            p += 1
    done = set()
    if os.path.exists(a.out):
        for line in open(a.out):
            f = line.split("\t")
            done.add((f[0], f[1], int(f[2])))
    todo = [t for t in todo if t not in done]
    print(f"{len(todo)} games to play", flush=True)
    while todo:
        with open(a.lock, "a") as lk:
            fcntl.flock(lk, fcntl.LOCK_EX)
            start = time.time()
            with concurrent.futures.ThreadPoolExecutor(a.workers) as ex:
                running = set()
                while todo or running:
                    while todo and len(running) < a.workers and time.time() - start < a.hold:
                        running.add(ex.submit(play, a, *todo.pop(0)))
                    if not running:
                        break
                    fin, running = concurrent.futures.wait(running, return_when=concurrent.futures.FIRST_COMPLETED)
                    with open(a.out, "a") as o:
                        for f in fin:
                            o.write(f.result())
            fcntl.flock(lk, fcntl.LOCK_UN)
        print(f"hold of {time.time() - start:.0f}s done, {len(todo)} left", flush=True)
        if todo:
            time.sleep(a.pause)


if __name__ == "__main__":
    main()
