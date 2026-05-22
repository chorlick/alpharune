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

# Play random vs MCTS
./build/src/openspiel/riftbound_openspiel \
    --agent1 random --agent2 mcts:sims=50 \
    --deck1 decks/miss_fortune_test.json \
    --deck2 decks/miss_fortune_test.json \
    --games 100 --threads 8

# Same matchup with HTML replays into ./replays/
./build/src/openspiel/riftbound_openspiel \
    --agent1 random --agent2 mcts:sims=50 \
    --deck1 decks/miss_fortune_test.json \
    --deck2 decks/miss_fortune_test.json \
    --games 5 --render-html

# [PLANNED] Play AS A HUMAN against an AI, in your browser.
# `riftbound_play` auto-starts a local Boost.Beast webserver on
# http://127.0.0.1:8080. Open that URL in any browser to play.
# --agent1 defaults to `human`; --agent2 picks the AI.
./build/riftbound_play \
    --agent2 mcts:sims=50 \
    --deck1 decks/miss_fortune_test.json \
    --deck2 decks/miss_fortune_test.json
# Open http://127.0.0.1:8080 — you control P1, MCTS-50 controls P2.

# Equivalent, with the human seat explicit:
./build/riftbound_play \
    --agent1 human --agent2 mcts:sims=50 \
    --deck1 decks/miss_fortune_test.json \
    --deck2 decks/miss_fortune_test.json

# Want to play as P2 instead? Swap the seats:
./build/riftbound_play \
    --agent1 mcts:sims=50 --agent2 human \
    --deck1 decks/miss_fortune_test.json \
    --deck2 decks/miss_fortune_test.json
```

`riftbound_play` is the upcoming web-UI binary. The `HumanAgent`
plug-in (`src/agents/human_agent.h`) and its unit tests already
ship; the Beast HTTP+WebSocket server + browser frontend that
route human input through it are the next deliverable. Both seats
can also be human (hot-seat play — two browser tabs, take turns)
or both AI (spectator mode — watch the game render live).

Pass `--help` to any binary for the full flag list.

## Binaries

| Binary | Purpose |
|---|---|
| `build/riftbound` | Legacy CLI — random-vs-random games + HTML replay. Simplest entry point. |
| `build/riftbound_openspiel` | OpenSpiel-integrated match runner. Supports `random` / `mcts:sims=N` / `ismcts:sims=N` agents, deterministic seeding, batch threading, per-game HTML replay output. |
| `build/riftbound_play` | **[PLANNED]** Web UI binary. Auto-starts a Boost.Beast HTTP+WebSocket server on `127.0.0.1:8080`. Play in your browser against any built-in agent. Routes user input through `HumanAgent` (already shipping in `src/agents/`). |
| `build/riftbound_tests` | Google Test suite (531 tests). |
| `build/src/openspiel/riftbound_clone_equiv_test` | Validates Clone() correctness — clones mid-game, runs the same actions on original + clone, asserts identical terminal state. |
| `build/src/openspiel/riftbound_clone_microbench` | Microbenchmark — times Clone() throughput. |
| `build/src/openspiel/riftbound_parity_baseline` | Diagnostic — confirms OpenSpiel wrapper produces statistically identical results to the raw BatchRunner. |

## Plug in your own agent

Subclass `riftbound::AgentInterface` (see `src/agents/agent_interface.h`)
or any OpenSpiel `Bot` / `Evaluator`. The `riftbound_openspiel` binary's
agent registration in `src/openspiel/openspiel_match.cpp` shows the
pattern — adding a new `--agent foo:...` spec is ~30 lines.

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
