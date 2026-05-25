# Riftbound Simulation Engine

A high-performance C++20 game engine that simulates 1v1 Riftbound TCG matches
end-to-end. Drop-in compatible with [OpenSpiel](https://github.com/google-deepmind/open_spiel)
so any tree-search algorithm (MCTS, ISMCTS, CFR, …) can plug straight in.

> **Disclaimer.** This codebase was largely *vibe-coded* — built rapidly with
> heavy AI-assisted iteration, prioritising "works on the test decks today" over
> "production-grade everywhere." Released **as-is** for research and hobby use.
> Expect rough edges in: card coverage (~80 of 787 cards manually implemented;
> rest are auto-generated stubs of varying fidelity), edge-case rules
> interactions, performance on stress workloads, and corners of the OpenSpiel
> wrapper. PRs welcome; production deployment at your own risk.

> The training / ML side — neural agents, self-play loops, model checkpoints —
> lives in a separate sibling repo. This repo is just the simulator + AI
> baselines via OpenSpiel.

## What works today

- **Full rules engine.** Awaken → channel → draw → main → end turn loop,
  FEPR chain resolution, combat with damage assignment, scoring, mulligans,
  battlefields, gear/equip, 23 keyword mechanics, replacement effects.
- **787 cards** loaded from `cards/registry.json`. ~80 manually implemented
  with full behavior (champions, legends, key spells in test decks); the
  rest have auto-generated stubs that cover simple effects.
- **Deterministic.** Given a seed, the same game replays identically.
- **OpenSpiel integration.** `RiftboundGame` / `RiftboundState` implement
  the OpenSpiel `Game` / `State` interfaces. MCTS, ISMCTS, and random
  agents work out of the box.
- **HTML replays.** Per-game rendered HTML with board snapshots,
  decision points, trace log, arrow-key navigation.
- **536 unit tests** covering engine behavior + card mechanics.
- **Engine-fiber step machine.** `boost::context::fiber`-based cooperative
  yields at decision points. No OS thread per game state, supports
  thousands of `Clone()` calls per decision for branching search.

## Quick start

```bash
# Install deps (Ubuntu/Debian)
sudo apt-get install cmake ninja-build g++ libboost-all-dev nlohmann-json3-dev

# Configure + build (first time clones OpenSpiel + abseil into build/_deps, ~30s)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run unit tests
RIFTBOUND_ROOT=. ./build/riftbound_tests

# Play random vs MCTS (single game, stdout + HTML replay)
./build/riftbound \
    --agent1 random --agent2 mcts:sims=50 \
    --deck1 decks/miss_fortune_test.json \
    --deck2 decks/miss_fortune_test.json \
    --render-html on

# Batch 100 games on 8 threads (random vs random, no UI)
./build/riftbound \
    --agent1 random --agent2 random \
    --deck1 decks/miss_fortune_test.json \
    --deck2 decks/miss_fortune_test.json \
    --games 100 --threads 8

# Play AS A HUMAN against MCTS-50 in your browser. The binary
# auto-starts a Boost.Beast webserver on http://127.0.0.1:8080
# whenever any seat is human; trace logging + HTML replay are also
# auto-enabled. Open the URL in any browser to play.
./build/riftbound \
    --agent1 human --agent2 mcts:sims=50 \
    --deck1 decks/miss_fortune_test.json \
    --deck2 decks/miss_fortune_test.json

# Want to play as P2 instead? Swap the seats:
./build/riftbound \
    --agent1 mcts:sims=50 --agent2 human \
    --deck1 decks/miss_fortune_test.json \
    --deck2 decks/miss_fortune_test.json

# Hot-seat (two humans in one browser tab, take turns):
./build/riftbound --agent1 human --agent2 human \
    --deck1 decks/miss_fortune_test.json \
    --deck2 decks/miss_fortune_test.json

# Spectator (force the web UI ON for an AI-vs-AI game):
./build/riftbound --agent1 random --agent2 mcts:sims=20 --web on \
    --deck1 decks/miss_fortune_test.json \
    --deck2 decks/miss_fortune_test.json
```

Pass `--help` for the full flag list. The web UI's WebSocket protocol
is documented in [`docs/play-api.md`](docs/play-api.md).

## Binaries

| Binary | Purpose |
|---|---|
| `build/riftbound` | Unified game runner. All modes (single / batch / web UI / spectator), all agents (`random` / `human` / `mcts:sims=N` / `ismcts:sims=N`), HTML replay, trace/debug logging — all driven by CLI flags. |
| `build/riftbound_tests` | Google Test suite. |
| `build/src/openspiel/riftbound_clone_equiv_test` | Validates Clone() correctness — clones mid-game, runs the same actions on original + clone, asserts identical terminal state. |
| `build/src/openspiel/riftbound_clone_microbench` | Microbenchmark — times Clone() throughput. |
| `build/src/openspiel/riftbound_parity_baseline` | Diagnostic — confirms OpenSpiel wrapper produces statistically identical results to the raw engine. |

## Plug in your own agent

Subclass `riftbound::AgentInterface` (see `src/agents/agent_interface.h`)
and add a branch to `buildAgent()` in `src/main.cpp` — adding a new
`--agent foo:...` spec is ~30 lines. The `MctsAgent` / `IsMctsAgent`
adapters in `src/agents/mcts_agent.{h,cpp}` show how to wrap an
OpenSpiel `Bot` behind the same interface.

For learned agents: a sibling repo holds the training pipeline + model
architectures (Deep CFR, OSFP, online CFR / ReBeL inference, etc.).
That repo links against this one as a static library — engine repo
stays slim and dependency-light.

## Card data pipeline

`cards/registry.json` is the single source of truth for card definitions.
To rebuild it from scratch:

```bash
# 1. Scrape the official Riftbound gallery (writes cards/json/*.json)
python3 scripts/fetch_cards.py

# 2. Apply errata (patches ability_text from errata/ documents)
python3 scripts/apply_errata.py

# 3. Build the registry (assigns stable integer IDs, joins errata)
python3 scripts/card_registry.py

# 4. Code-gen C++ stubs for newly-added cards
python3 scripts/generate_cards.py

# 5. Import a text deck list
python3 scripts/deck_import.py decks/my_deck.txt
```

| Script | Purpose |
|---|---|
| `scripts/fetch_cards.py` | Scrape Riftbound's official gallery into per-card JSONs. Run once per set release. |
| `scripts/apply_errata.py` | Patch card text with corrections from `errata/`. |
| `scripts/card_registry.py` | Assemble `cards/registry.json`. Source of truth for card-to-integer mapping. |
| `scripts/deck_import.py` | Convert text deck lists (Piltover Archive format) into engine JSON. Validates against tournament rules. |
| `scripts/generate_cards.py` | Code-gen C++ Card subclass stubs from registry. Hand-edits go in `src/cards/manual/`. |
| `scripts/generate_replays.py` | Bulk-generate HTML replays of every deck pair under `decks/`. Useful for visual V&V after card changes. |
| `scripts/audit_deck_cards.py` | Per-deck audit of card-implementation status (FULL / PARTIAL / STUB / MISSING). |

## Repository layout

```
src/core/        ID types, events, intents, game state, card database
src/engine/      Turn loop, chain manager, effect executor, trigger manager,
                 batch runner, step driver (fiber-based)
src/cards/       Card subclasses — generated/ + manual/ overrides
src/agents/      AgentInterface + RandomAgent + HumanAgent
src/io/          HTML replay writer, ASCII state renderer
src/openspiel/   OpenSpiel Game / State subclasses, action vocabulary,
                 match runner, clone-correctness tests
src/ml/          Engine-side shared infra: feature extractor +
                 CFR utilities. No torch — these emit data for
                 downstream ML consumers.
src/rules/       Deck validator (tournament rules)
src/effects/     Effect type definitions

tests/           Google Test — engine behavior + card mechanics
cards/           registry.json, errata-applied card data, ban list
decks/           Sample decks (per-archetype .json + .txt formats)
rules/           core-rules.md, tournament-rules.md, core-rules.pdf
errata/          Official errata documents
scripts/         Card-data pipeline + replay generation
```

## Reference

- `rules/core-rules.md` — full game rules (sections 000–826)
- `rules/tournament-rules.md` — deck construction + tournament policies
- `CLAUDE.md` — coding standards + architecture conventions for contributors

## Status

This is a working development snapshot. The engine, OpenSpiel wrapper, and
test suite are stable. Card coverage is improving steadily — see
`scripts/audit_deck_cards.py` output for per-deck implementation status.

## License

TBD. Card data fetched from public Riot sources; card art is *not* bundled
(images are loaded on-demand from Riot's CDN by replay viewers / web UIs).
Riftbound is the property of Riot Games — this engine reproduces published
public rules for research and educational use.
