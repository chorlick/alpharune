#!/usr/bin/env python3
"""Run fixed-seed traced games between deck lists and keep one log per game.

Usage:
  run_deck_games.py --out <dir> --plan <plan.tsv> [--threads 3] [--binary build/riftbound]

plan.tsv: one game per line, tab separated:
  deck1  deck2  seed  agent1  agent2
(deck names without directory or extension; agents as the CLI takes them.)

Each game is one process:
  build/riftbound --deck1 decks/<d1>.txt --deck2 decks/<d2>.txt --seed <s> \
      --agent1 <a1> --agent2 <a2> --trace --debug --render-html on --show-hand \
      --replay-dir <out>/html/<name>
and its stdout+stderr go to <out>/<name>.log with
<name> = <d1>__<d2>__s<seed>__<a1>__<a2> (agent names with ':' replaced by
'-', '=' dropped). The HTML replay is converted to <out>/<name>.replay.txt
(replay_to_text.py): board, legal options, chosen option and trace lines per
decision. A game whose log exists is not run again. Exit codes are appended
to <out>/index.tsv. Run from the repository root.
"""
import argparse, os, subprocess, sys, concurrent.futures, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import replay_to_text

def log_name(d1, d2, seed, a1, a2):
    clean = lambda a: a.replace(":", "-").replace("=", "")
    return f"{d1}__{d2}__s{seed}__{clean(a1)}__{clean(a2)}.log"

def deck_path(args, name):
    """decks/<name>.txt, or <sb-dir>/<name>.txt for a post-sideboard list
    (name ending in _sb, written by sideboarded_decks.py)."""
    if name.endswith("_sb"):
        return os.path.join(args.sb_dir, name + ".txt")
    return f"decks/{name}.txt"

def run_one(args, row):
    d1, d2, seed, a1, a2 = row
    path = os.path.join(args.out, log_name(d1, d2, seed, a1, a2))
    if os.path.exists(path) and os.path.getsize(path) > 0:
        return (row, None, path)
    rdir = os.path.join(args.out, "html", os.path.basename(path)[:-4])
    cmd = [args.binary, "--deck1", deck_path(args, d1), "--deck2", deck_path(args, d2),
           "--seed", str(seed), "--agent1", a1, "--agent2", a2,
           "--trace", "--debug", "--render-html", "on", "--show-hand",
           "--replay-dir", rdir]
    t0 = time.time()
    with open(path, "w") as f:
        try:
            rc = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT,
                                env=dict(os.environ, RIFTBOUND_ROOT="."),
                                timeout=args.timeout).returncode
        except subprocess.TimeoutExpired:
            rc = -999
    # The replay (board + legal options + trace per decision) as text, next
    # to the log: <name>.replay.txt.
    try:
        for sub in sorted(os.listdir(rdir)):
            html = os.path.join(rdir, sub, "replay.html")
            if os.path.exists(html):
                replay_to_text.convert(html, path[:-4] + ".replay.txt")
    except Exception as e:  # a failed game may leave no replay
        with open(path, "a") as f:
            f.write(f"\n[run_deck_games] replay conversion failed: {e}\n")
    return (row, (rc, time.time() - t0), path)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--plan", required=True)
    ap.add_argument("--threads", type=int, default=3)
    ap.add_argument("--binary", default="build/riftbound")
    ap.add_argument("--timeout", type=int, default=900)
    ap.add_argument("--sb-dir", default="decks_sb",
                    help="directory holding the <deck>_sb.txt lists")
    args = ap.parse_args()
    if args.threads > 3:
        sys.exit("at most 3 threads (lane rule)")
    os.makedirs(args.out, exist_ok=True)
    rows = []
    with open(args.plan) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            rows.append(tuple(line.split("\t")))
    with concurrent.futures.ThreadPoolExecutor(args.threads) as ex, \
         open(os.path.join(args.out, "index.tsv"), "a") as idx:
        for row, res, path in ex.map(lambda r: run_one(args, r), rows):
            if res is None:
                continue
            rc, dt = res
            idx.write("\t".join(row) + f"\t{rc}\t{dt:.1f}\t{os.path.basename(path)}\n")
            idx.flush()
            if rc != 0:
                print("NONZERO", rc, path, flush=True)
    print("done", len(rows))

if __name__ == "__main__":
    main()
