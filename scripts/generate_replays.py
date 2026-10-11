#!/usr/bin/env python3
"""Bulk replay generation across every deck pair in decks/.

The `riftbound` binary plays ONE deck pair per invocation. This script runs it
once for each pair of deck lists (`decks/*.txt`, mirrors included) with
`--render-html on`, and puts each pair's replays in their own directory:

    <out-dir>/<deck1>_v_<deck2>/<timestamp>/replay.html          (1 game)
    <out-dir>/<deck1>_v_<deck2>/<timestamp>/replay_gameN.html    (N games)

Used for V&V: review a batch of games after card-implementation changes.

Agents are per-seat: `agent1` plays P1 with deck1, `agent2` plays P2 with
deck2. With the default `--pair-mode combinations` each unordered pair is
played once; use `permutations` when the seats differ (e.g. mcts vs random).

Examples (run from the repo root, after `cmake --build build --target riftbound`):

    # 5 games per pair, random vs random
    python3 scripts/generate_replays.py random random 5

    # MCTS holds seat 1, both seat orders of every pair, 4 games at a time
    python3 scripts/generate_replays.py mcts:sims=10 random 3 \\
        --pair-mode permutations --threads 4

    # Other sources / destination
    python3 scripts/generate_replays.py random random 5 \\
        --decks-dir decks --out-dir /tmp/replays --binary build/riftbound
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Generate HTML replays for every deck pair.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="agent specs are passed to the binary's --agent1 / --agent2: "
               "random | mcts:sims=N | ismcts:sims=N",
    )
    p.add_argument("agent1", help="P1 agent spec")
    p.add_argument("agent2", help="P2 agent spec")
    p.add_argument("games", type=int, help="Games to run per deck pair (>= 1)")
    p.add_argument("--decks-dir", type=Path, default=Path("decks"),
                   help="Directory of .txt deck lists (default: decks/)")
    p.add_argument("--out-dir", type=Path, default=Path("replays"),
                   help="Output root; one sub-directory per pair (default: replays/)")
    p.add_argument("--binary", type=Path, default=Path("build/riftbound"),
                   help="Path to the riftbound binary (default: build/riftbound)")
    p.add_argument("--seed", type=int, default=0,
                   help="RNG seed passed to the binary (0 = random)")
    p.add_argument("-t", "--threads", type=int, default=1,
                   help="Parallel games within one pair, passed to the binary's "
                        "--threads (default: 1; pairs themselves run one at a time)")
    p.add_argument("--pair-mode", choices=("combinations", "permutations"),
                   default="combinations",
                   help="combinations (default): unordered pairs incl. mirrors; "
                        "permutations: every ordered pair incl. mirrors")
    p.add_argument("--wild", action="store_true",
                   help="Pass --wild (allow banned cards in decks)")
    p.add_argument("-v", "--verbose", action="store_true",
                   help="Show the binary's output for every pair, not only failures")
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.games < 1:
        print("games must be >= 1", file=sys.stderr)
        return 2

    binary = args.binary.resolve()
    if not binary.is_file() or not os.access(binary, os.X_OK):
        print(f"binary not found or not executable: {binary}\n"
              "build it with: cmake --build build --target riftbound", file=sys.stderr)
        return 2
    decks_dir = args.decks_dir.resolve()
    decks = sorted(decks_dir.glob("*.txt"))
    if not decks:
        print(f"no *.txt deck lists in {decks_dir}", file=sys.stderr)
        return 2
    out_root = args.out_dir.resolve()

    if args.pair_mode == "combinations":
        pairs = [(decks[i], decks[j])
                 for i in range(len(decks)) for j in range(i, len(decks))]
    else:
        pairs = [(d1, d2) for d1 in decks for d2 in decks]

    print(f"{args.agent1} vs {args.agent2}: {args.games} game(s) x {len(pairs)} "
          f"pair(s) -> {out_root}")

    failed = 0
    for n, (deck1, deck2) in enumerate(pairs, 1):
        pair_dir = out_root / f"{deck1.stem}_v_{deck2.stem}"
        pair_dir.mkdir(parents=True, exist_ok=True)
        before = set(pair_dir.rglob("*.html"))
        cmd = [
            str(binary),
            "--deck1", str(deck1), "--deck2", str(deck2),
            "--agent1", args.agent1, "--agent2", args.agent2,
            "--games", str(args.games), "--threads", str(args.threads),
            "--seed", str(args.seed),
            "--web", "off",
            "--render-html", "on", "--replay-dir", str(pair_dir),
        ]
        if args.wild:
            cmd.append("--wild")
        # The binary resolves nothing relative to its working directory except
        # the paths given here, all of which are absolute.
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True)
        written = sorted(set(pair_dir.rglob("*.html")) - before)
        label = f"[{n}/{len(pairs)}] {deck1.stem} v {deck2.stem}"
        if proc.returncode != 0:
            failed += 1
            print(f"{label}: FAILED (exit {proc.returncode})\n{proc.stdout}",
                  file=sys.stderr)
            continue
        print(f"{label}: {len(written)} replay(s) in "
              f"{written[0].parent if written else pair_dir}")
        if args.verbose:
            print(proc.stdout.rstrip())
        if len(written) != args.games:
            failed += 1
            print(f"{label}: expected {args.games} replay file(s), found "
                  f"{len(written)}", file=sys.stderr)

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
