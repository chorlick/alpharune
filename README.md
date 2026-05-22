# Riftbound Simulation Engine

A C++20 game engine that simulates 1v1 Riftbound TCG matches end-to-end, with
the OpenSpiel framework wired up for tree-search agents and a Python training
loop for ONNX-deployed neural agents. This is a research codebase — not a
product — but the engine, the OpenSpiel wrapper, and the training pipeline
all work as standalone tools.

## What works today

- **Engine.** Full turn loop (awaken → channel → draw → main → end), FEPR
  chain resolution, combat with damage assignment, scoring, mulligans,
  battlefields, gear/equip, 23 keyword mechanics, replacement effects.
- **Cards.** 787 cards loaded from `cards/registry.json`. 76 manually
  implemented with full behavior (champions, legends, key spells in test
  decks). The other 711 have auto-generated stubs — many work for simple
  effects; complex effects are partial or no-op. A backlog of ~42
  do-nothing cards in test decks is tracked in `CLAUDE.md` and is the
  next polish target.
- **Agents.** `RandomAgent` (uniform random), `ModelAgent` (ONNX
  inference). A V1 model architecture with card embeddings + attention
  over actions, trained via supervised + REINFORCE self-play, ships in
  `scripts/train_agent.py`. Documented as plateaued — V2 is the next
  architecture direction.
- **OpenSpiel wrapper.** Information-set-correct `ObservationTensor`,
  bit-packed `Intent ↔ ActionID` encoding, replay-based `Clone()`,
  MCTSBot integration. `riftbound_openspiel` is the unified
  match-runner CLI. The replay Clone is the main throughput bottleneck;
  Phase C-1 of the OpenSpiel port is rewriting the engine as a native
  step machine to make Clone an O(1) memcpy.
- **Tests.** 98 unit tests via Google Test + 10-game clone equivalence
  test via OpenSpiel + 1000-game statistical parity vs the non-OpenSpiel
  batch runner.

## Build

```bash
# Engine only (fast)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# With OpenSpiel wrapper (clones OpenSpiel + abseil into build/_deps,
# adds ~30s to first configure)
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DRIFTBOUND_BUILD_OPENSPIEL=ON
cmake --build build-release
```

**Dependencies:** cmake, g++ (C++20), libboost-all-dev,
nlohmann-json3-dev, ninja-build, ONNX Runtime (fetched by CMake).

**Python (for training):**
```bash
conda activate riftbound && pip install -r scripts/requirements.txt
```
(torch, numpy, onnx, onnxruntime, tensorboard)

## Run

```bash
# Play a single game between two random agents
./build-release/riftbound decks/leblanc_test.json decks/leblanc_test.json \
    -r cards/registry.json

# Batch 100 games on all cores, write training data
./build-release/riftbound deck1.json deck2.json \
    -r cards/registry.json --games 100 --threads 0 -o /tmp/data.jsonl

# Play an ONNX model against random
./build-release/riftbound deck1.json deck2.json \
    -r cards/registry.json \
    --agent1 model:models/miss_fortune/v002.onnx --agent2 random

# OpenSpiel: random vs random
./build-release/src/openspiel/riftbound_openspiel \
    --agent1 random --agent2 random --games 100 --threads 4

# OpenSpiel: MCTS vs random (deterministic seed for reproducible A/Bs)
./build-release/src/openspiel/riftbound_openspiel \
    --agent1 random --agent2 mcts:sims=5 \
    --games 100 --seed 42

# Full unit test suite
RIFTBOUND_ROOT=. ./build-release/riftbound_tests

# OpenSpiel clone-equivalence (correctness, not throughput)
RIFTBOUND_ROOT=. ./build-release/src/openspiel/riftbound_clone_equiv_test
```

`riftbound_openspiel --help` lists every flag.

## Scripts

Tooling under `scripts/`. The card-data pipeline (`fetch → errata →
registry → import`) builds `cards/registry.json` from scratch; the
others are dev conveniences.

| Script | Purpose |
|---|---|
| `scripts/fetch_cards.py` | Scrape the official Riftbound gallery into per-card JSONs under `cards/json/`. Run once per set release. |
| `scripts/apply_errata.py` | Patch card `ability_text` with corrections from `errata/` documents. Run after `fetch_cards.py`. |
| `scripts/card_registry.py` | Assemble `cards/registry.json` — stable integer IDs + feature vectors used by the engine, importer, and ML. Single source of truth for card-to-int mapping. |
| `scripts/deck_import.py` | Convert a text deck list (Piltover Archive format) into the JSON the engine consumes. Validates against tournament rules. `python3 scripts/deck_import.py decks/myDeck.txt`. |
| `scripts/generate_cards.py` | Code-gen C++ Card subclass stubs from `registry.json`. Re-run after registry changes; hand-edits go in `src/cards/manual/`. |
| `scripts/generate_replays.py` | Bulk replay generation for V&V. Runs N games of every deck pair under `decks/` and stashes per-pair HTML replays under `replays/<deck1>_v_<deck2>/`. Threaded — runs pairs in parallel. Agents are per-seat. Defaults to **combinations** (unordered, incl. mirrors); pass `--pair-mode permutations` for the full N×N when asymmetric agents make seat order meaningful. |

**Bulk replay generation examples:**
```bash
# 5 games per matchup, random vs random, default (combinations, 1 thread).
# 11 decks → 66 unordered pairs → 330 games:
scripts/generate_replays.py random random 5

# Same but use all cores in parallel (one binary invocation per pair):
scripts/generate_replays.py random random 5 --threads 0

# Asymmetric agents — switch to permutations so seat order is preserved.
# 11 decks → 121 ordered pairs → 363 games:
scripts/generate_replays.py mcts:sims=10 random 3 \
    --pair-mode permutations --threads 8

# Override deck source / output directory:
scripts/generate_replays.py random random 5 \
    --decks-dir decks/featured --out-dir /tmp/replays
```

Agents map directly to the binary's `--agent1` / `--agent2` flags, so
the seat assignment is stable across all deck pairings. For asymmetric
matchups (mcts vs random), combinations mode only tests one seat
orientation — use `--pair-mode permutations` to cover both.
Requires `tqdm` (already in `scripts/requirements.txt`).

## What's planned

The headline architectural items left, in dependency order:

1. **Phase C-1 commits 6–9** — Convert the engine's chain / combat /
   cleanup subsystems to be resumable without the StepDriver worker
   thread, then collapse `Clone()` to `memcpy(GameState)`. Unblocks
   real MCTS at scale and any AlphaZero-style training.
2. **Fill in the ~42 do-nothing cards** in test decks. Each is a small
   card-class override; the backlog is in `CLAUDE.md`.
3. **Phase C-2A continued** — Remaining V2 data-emission PRs (spatial
   mapping, factored legal masks, binary serializer V2, etc.) once the
   model that consumes them is being built.
4. **Phase C-2B** — V2 transformer + pointer-head architecture in C++
   via LibTorch, replacing the Python training scripts entirely.
5. **Phase B-2** — Explicit OpenSpiel chance nodes (currently
   `kSampledStochastic`), prerequisite for CFR / NFSP.

## Repository layout

See `CLAUDE.md` for the architectural deep-dive and the per-phase work
log. The short version:

```
src/core/        types, ids, events, game state, card database
src/engine/      turn loop, chain manager, effect executor, trigger manager,
                 batch runner, step driver
src/cards/       card subclasses (generated/, manual/)
src/agents/      random + ONNX model agents
src/ml/          shared feature extractor + V2 tensor types
src/openspiel/   game/state subclasses, action encoding, match runner
tests/           Google Test suites
cards/           registry.json + per-card JSON + ban list
decks/           sample decks
docs/            engine-design.md, ml-training-design.md,
                 potential-model-architecture.txt (Phase C-2 plan)
scripts/         card-data pipeline + deck importer + replay batch runner
```

## Status

Tagged `v0.1.0` — the engine and tooling listed under "What works
today" are stable; everything under "What's planned" is in flight.
This is a working development snapshot, not a finished product.
