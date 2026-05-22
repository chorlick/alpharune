#!/usr/bin/env python3
"""Bulk replay generation across every ordered deck pair in decks/.

Runs N games of `riftbound_openspiel` for each deck1 × deck2 pair (incl.
mirrors) and stashes the resulting HTML replays under
``replays/<deck1>_v_<deck2>/``. Used for V&V — review batch-generated
games after card-implementation changes.

Agents are per-seat: ``agent1`` plays P1, ``agent2`` plays P2. The seat
assignment is stable across all deck pairings, so to cover both
orientations of an asymmetric matchup (e.g. mcts:5 vs random), run the
script twice with the agents swapped.

Examples
--------

::

    # 5 games per pair, random vs random.
    scripts/generate_replays.py random random 5

    # MCTS holds seat 1, random holds seat 2.
    scripts/generate_replays.py mcts:sims=10 random 3

    # Override sources / verbose binary output.
    scripts/generate_replays.py random random 5 \\
        --decks-dir decks/featured --out-dir /tmp/replays --verbose

Requires ``build-release/src/openspiel/riftbound_openspiel`` — build with
``cmake --build build-release --target riftbound_openspiel``.
"""

from __future__ import annotations

import argparse
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from queue import Queue

from tqdm import tqdm

# Regex for the per-game line the binary prints when --quiet is OFF.
# Format: "Game N: <winner> wins (...) ..."  — we don't need the rest,
# only that "Game N:" means one game has just completed.
GAME_LINE_RE = re.compile(r"^Game\s+\d+\s*:")

log = logging.getLogger("generate_replays")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Generate HTML replays for every ordered deck pair.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "agent specs: 'random' or 'mcts:sims=N'\n"
            "agents map directly to the binary's --agent1 / --agent2 flags."
        ),
    )
    p.add_argument("agent1", help="P1 agent spec (random | mcts:sims=N)")
    p.add_argument("agent2", help="P2 agent spec (random | mcts:sims=N)")
    p.add_argument("games", type=int, help="Games to run per deck pair")
    p.add_argument(
        "--decks-dir",
        type=Path,
        default=Path("decks"),
        help="Directory of deck JSON files (default: decks/)",
    )
    p.add_argument(
        "--out-dir",
        type=Path,
        default=Path("replays"),
        help="Output root; per-pair subdirs are created here "
        "(default: replays/)",
    )
    p.add_argument(
        "--binary",
        type=Path,
        default=Path("build-release/src/openspiel/riftbound_openspiel"),
        help="Path to the riftbound_openspiel binary "
        "(default: build-release/src/openspiel/riftbound_openspiel)",
    )
    p.add_argument(
        "--registry",
        type=Path,
        default=Path("cards/registry.json"),
        help="Path to cards/registry.json (default: cards/registry.json)",
    )
    p.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Show the binary's full stdout (default: suppress, log "
        "summary line only).",
    )
    p.add_argument(
        "--seed",
        type=int,
        default=0,
        help="Base RNG seed forwarded to the binary (0 = nondeterministic).",
    )
    p.add_argument(
        "-t", "--threads",
        type=int,
        default=1,
        help="Number of deck pairs to run concurrently (default: 1, "
        "0 = os.cpu_count()). Each worker drives one binary invocation. "
        "MCTS bots are not thread-safe, but each thread runs its own "
        "process so cross-pair safety isn't an issue here.",
    )
    p.add_argument(
        "--pair-mode",
        choices=("combinations", "permutations"),
        default="combinations",
        help="Pairing scheme over the decks directory:\n"
        "  combinations (default) — unordered pairs incl. mirrors. "
        "(A,B) and (B,A) collapse to one matchup. For 11 decks: 66 pairs.\n"
        "  permutations — all ordered pairs incl. mirrors. P1/P2 seat "
        "assignment is fixed so (A,B) and (B,A) are distinct. Use this "
        "for asymmetric agents (e.g. mcts vs random). For 11 decks: 121.",
    )
    return p.parse_args(argv)


def validate_paths(args: argparse.Namespace) -> tuple[Path, Path, Path]:
    """Resolve and validate the binary / registry / decks dir. Returns
    their absolute paths. Exits on any missing prerequisite."""
    bin_abs = args.binary.resolve()
    if not bin_abs.is_file() or not bin_abs.stat().st_mode & 0o111:
        log.error(
            "binary not found or not executable: %s\n"
            "build with: cmake --build build-release "
            "--target riftbound_openspiel",
            bin_abs,
        )
        sys.exit(1)
    registry_abs = args.registry.resolve()
    if not registry_abs.is_file():
        log.error("registry not found: %s (run from repo root)", registry_abs)
        sys.exit(1)
    decks_abs = args.decks_dir.resolve()
    if not decks_abs.is_dir():
        log.error("decks-dir not a directory: %s", decks_abs)
        sys.exit(1)
    return bin_abs, registry_abs, decks_abs


def discover_decks(decks_dir: Path) -> list[Path]:
    decks = sorted(decks_dir.glob("*.json"))
    if not decks:
        log.error("no *.json deck files found in %s", decks_dir)
        sys.exit(1)
    return decks


def run_pair(
    bin_abs: Path,
    registry_abs: Path,
    cards_dir_abs: Path,
    deck1: Path,
    deck2: Path,
    agent1: str,
    agent2: str,
    games: int,
    seed: int,
    pair_out_dir: Path,
    verbose: bool,
    inner_bar: "tqdm | None" = None,
) -> str:
    """Run a single pair through the binary in a tmpdir, move replays to
    pair_out_dir, return the summary line ("P1 wins: X" etc.).

    If `inner_bar` is provided, stream the binary's stdout and tick
    the bar once per "Game N:" line so the operator sees live game-
    completion progress for this pair.
    """
    if pair_out_dir.exists():
        shutil.rmtree(pair_out_dir)
    pair_out_dir.mkdir(parents=True)

    captured: list[str] = []
    rc = 0
    # Binary writes to ./replays/replay_gameN.html relative to its CWD,
    # so run it inside a tmpdir and move the artifacts after. We
    # symlink the cards/ tree in case the binary's card-data lookups
    # use relative paths.
    with tempfile.TemporaryDirectory(prefix="riftbound_replay_") as tmp:
        tmp = Path(tmp)
        (tmp / "cards").symlink_to(cards_dir_abs)
        # --no-progress: suppress the binary's own tqdm so it doesn't
        # collide with ours. --quiet is intentionally OFF so we get
        # per-game "Game N: ..." lines to tick our inner bar.
        #
        # Wrap with `stdbuf -oL` so std::cout is line-buffered through
        # the pipe. Without this, the binary's per-game lines wouldn't
        # flush until process exit, defeating the inner-bar updates.
        # `stdbuf` is part of GNU coreutils; if absent we'd fall back to
        # the binary's default (block-buffered) writes — same end result,
        # just no live progress within a pair.
        cmd = [
            str(bin_abs),
            "--agent1", agent1,
            "--agent2", agent2,
            "--deck1", str(deck1),
            "--deck2", str(deck2),
            "-r", str(registry_abs),
            "--games", str(games),
            "--render-html",
            "--no-progress",
        ]
        if shutil.which("stdbuf"):
            cmd = ["stdbuf", "-oL"] + cmd
        if seed > 0:
            cmd += ["--seed", str(seed)]
        log.debug("running: %s (cwd=%s)", " ".join(cmd), tmp)

        # Stream stdout line by line so the inner bar updates as games
        # complete. bufsize=1 + text=True gives us line-buffered reads.
        proc = subprocess.Popen(
            cmd,
            cwd=tmp,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        assert proc.stdout is not None
        for line in proc.stdout:
            captured.append(line)
            if inner_bar is not None and GAME_LINE_RE.match(line.strip()):
                inner_bar.update(1)
        rc = proc.wait()

        # Move artifacts from <tmp>/replays/ → pair_out_dir before the
        # tmpdir is cleaned up.
        replay_src = tmp / "replays"
        if replay_src.is_dir():
            for f in replay_src.glob("replay_game*.html"):
                shutil.move(str(f), str(pair_out_dir / f.name))

    out = "".join(captured)
    if rc != 0:
        log.error(
            "binary failed (rc=%d) for %s vs %s:\n%s",
            rc, deck1.stem, deck2.stem, out,
        )
        return "(crashed)"
    if verbose:
        log.info("binary output:\n%s", out.rstrip())

    # Extract the summary line for the progress display.
    summary = []
    for line in out.splitlines():
        s = line.strip()
        if s.startswith(("P1 wins:", "P2 wins:", "Draws:")):
            summary.append(s.replace("  ", " "))
    return " | ".join(summary) if summary else ""


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(levelname)-5s %(message)s",
        datefmt="%H:%M:%S",
    )

    bin_abs, registry_abs, decks_abs = validate_paths(args)
    cards_dir_abs = registry_abs.parent

    decks = discover_decks(decks_abs)
    out_dir_abs = args.out_dir.resolve()
    out_dir_abs.mkdir(parents=True, exist_ok=True)

    # combinations: each unordered pair once (incl. mirrors). For decks
    # [A, B, C]: (A,A), (A,B), (A,C), (B,B), (B,C), (C,C) — 6 pairs.
    # permutations: full N×N cartesian (incl. mirrors). For [A, B, C]:
    # all 9 ordered pairs. Use permutations when P1/P2 asymmetry
    # matters (asymmetric agent specs).
    if args.pair_mode == "combinations":
        pairs = [(decks[i], decks[j])
                 for i in range(len(decks))
                 for j in range(i, len(decks))]
    else:  # permutations
        pairs = [(d1, d2) for d1 in decks for d2 in decks]
    n_threads = args.threads if args.threads > 0 else (os.cpu_count() or 1)
    n_threads = min(n_threads, len(pairs))  # no point spawning more than 1/pair
    log.info(
        "agent1=%s | agent2=%s | %d games × %d pairs = %d games total | "
        "threads=%d -> %s",
        args.agent1, args.agent2, args.games, len(pairs),
        args.games * len(pairs), n_threads, out_dir_abs,
    )

    # Stacked tqdm bars:
    #   position=0  — outer "matchups" bar tracks total pairs completed.
    #   position=1..N — N inner per-worker bars, each shows games-within-
    #     a-pair as they complete. When a worker finishes a pair, its bar
    #     closes (leave=False clears the line); the next pair on that
    #     same worker creates a fresh bar in the same slot. Slots are
    #     handed out via a Queue so concurrent workers never collide on
    #     the same vertical line.
    slot_queue: Queue[int] = Queue()
    for i in range(n_threads):
        slot_queue.put(i + 1)  # positions 1..N (0 is the outer)

    outer_bar = tqdm(
        total=len(pairs),
        unit="pair",
        desc="matchups",
        position=0,
        leave=True,
    )

    def _work(deck1: Path, deck2: Path) -> tuple[Path, Path, str]:
        slot = slot_queue.get()
        try:
            label = f"{deck1.stem[:18]:>18s} v {deck2.stem[:18]:<18s}"
            inner = tqdm(
                total=args.games,
                unit="game",
                desc=label,
                position=slot,
                leave=False,
            )
            try:
                pair_dir = out_dir_abs / f"{deck1.stem}_v_{deck2.stem}"
                summary = run_pair(
                    bin_abs=bin_abs,
                    registry_abs=registry_abs,
                    cards_dir_abs=cards_dir_abs,
                    deck1=deck1.resolve(),
                    deck2=deck2.resolve(),
                    agent1=args.agent1,
                    agent2=args.agent2,
                    games=args.games,
                    seed=args.seed,
                    pair_out_dir=pair_dir,
                    verbose=args.verbose,
                    inner_bar=inner,
                )
            finally:
                inner.close()
        finally:
            slot_queue.put(slot)
        return deck1, deck2, summary

    try:
        with ThreadPoolExecutor(max_workers=n_threads) as pool:
            futures = {
                pool.submit(_work, d1, d2): (d1, d2) for d1, d2 in pairs
            }
            for fut in as_completed(futures):
                d1, d2 = futures[fut]
                try:
                    _, _, summary = fut.result()
                except Exception as e:
                    log.exception("worker failed for %s vs %s: %s",
                                  d1.stem, d2.stem, e)
                    summary = "(error)"
                if summary:
                    # Write summary above the bars via tqdm.write so it
                    # doesn't garble the live progress display.
                    tqdm.write(f"{d1.stem} vs {d2.stem} — {summary}")
                outer_bar.update(1)
    finally:
        outer_bar.close()

    log.info(
        "Done. %d pairings × %d games written under %s",
        len(pairs), args.games, out_dir_abs,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
