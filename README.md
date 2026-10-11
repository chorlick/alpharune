# Riftbound Simulation Engine

A high-performance C++20 game engine that simulates 1v1 Riftbound TCG matches
end-to-end. Drop-in compatible with [OpenSpiel](https://github.com/google-deepmind/open_spiel)
so any tree-search algorithm (MCTS, ISMCTS, CFR, …) can plug straight in.

> **Disclaimer.** Released **as-is** for research and hobby use.
> Expect rough edges in: card coverage (955 hand-authored cards; a few are
> still stubs or carry a known engine gap, see `scripts/card_coverage.py`),
> edge-case rules interactions, performance on stress workloads, and corners of the OpenSpiel
> wrapper. PRs welcome; production deployment at your own risk.

> The training / ML side — neural agents, self-play loops, model checkpoints —
> lives in a separate sibling repo. This repo is just the simulator + AI
> baselines via OpenSpiel.

## What works today
- Uses Piltover archive formatted game decks
- **Full rules engine.** Awaken → channel → draw → main → end turn loop,
  FEPR chain resolution, combat with damage assignment, scoring, mulligans,
  battlefields, gear/equip, 23 keyword mechanics, replacement effects.
- **955 cards**, each a hand-authored C++ class owning its own data + behavior.
  `python3 scripts/card_coverage.py` classifies every card file (implemented /
  stub / vanilla, plus in-code gap comments); its work lists are under
  [`docs/card-coverage/`](docs/card-coverage/).
- **Deterministic.** Given a seed, the same game replays identically.
- **OpenSpiel integration.** `RiftboundGame` / `RiftboundState` implement
  the OpenSpiel `Game` / `State` interfaces. MCTS, ISMCTS, and random
  agents work out of the box.
- **HTML replays.** Per-game rendered HTML with board snapshots,
  decision points, trace log, arrow-key navigation.
- **855 unit tests** covering engine behavior + card mechanics.
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
    --deck1 decks/fiora.txt \
    --deck2 decks/yi_rebuild.txt \
    --render-html on

# Batch 100 games on 8 threads (random vs random, no UI). Add
# --render-html on to write replays/<timestamp>/replay_gameN.html.
./build/riftbound \
    --agent1 random --agent2 random \
    --deck1 decks/fiora.txt \
    --deck2 decks/yi_rebuild.txt \
    --games 100 --threads 8

# Play AS A HUMAN against MCTS-50 in your browser. The binary
# auto-starts a Boost.Beast webserver on http://127.0.0.1:8080
# whenever any seat is human; trace logging + HTML replay are also
# auto-enabled. Open the URL in any browser to play.
./build/riftbound \
    --agent1 human --agent2 mcts:sims=50 \
    --deck1 decks/fiora.txt \
    --deck2 decks/yi_rebuild.txt

# Want to play as P2 instead? Swap the seats:
./build/riftbound \
    --agent1 mcts:sims=50 --agent2 human \
    --deck1 decks/fiora.txt \
    --deck2 decks/yi_rebuild.txt

# Hot-seat (two humans in one browser tab, take turns):
./build/riftbound --agent1 human --agent2 human \
    --deck1 decks/fiora.txt \
    --deck2 decks/yi_rebuild.txt

# Spectator (force the web UI ON for an AI-vs-AI game):
./build/riftbound --agent1 random --agent2 mcts:sims=20 --web on \
    --deck1 decks/fiora.txt \
    --deck2 decks/yi_rebuild.txt
```

Pass `--help` for the full flag list. The web UI's WebSocket protocol
is documented in [`docs/play-api.md`](docs/play-api.md).

## Play against the AI (web UI)

The fastest way to play a game yourself, as Player 1, against a
50-simulation MCTS bot:

```bash
./build/riftbound \
    --agent1 human --agent2 mcts:sims=50 \
    --deck1 decks/fiora.txt \
    --deck2 decks/yi_rebuild.txt
```

Because a seat is `human`, the binary auto-starts a small Boost.Beast
web server and prints a URL:

```
  → Open http://127.0.0.1:8080 in your browser to play.
```

Open that in a browser and play. The game blocks at each of your
decision points until you click an action; the MCTS opponent moves on
its own. (MCTS cost scales with `sims` — `sims=50` is snappy; `sims=500`
can take minutes *per decision*, so start low.)

Useful flags:

| Flag | Default | Meaning |
|---|---|---|
| `--agent1` / `--agent2` | `random` | Per-seat agent: `human`, `random`, `mcts:sims=N`, `ismcts:sims=N`. |
| `--web` | `auto` | `auto` starts the UI whenever a seat is human; `on` forces it (spectate AI-vs-AI); `off` disables it. |
| `--port` / `--bind` | `8080` / `127.0.0.1` | Where the UI listens. Use `--bind 0.0.0.0` to reach it from another machine: the printed URL then carries an access token that the server requires. |
| `--god-mode` | `auto` | `auto`/`on` enables the in-browser state editor when a human is seated. While it is on the UI shows every hand and deck. `off` hides the editor **and** the opponent's hand and both decks' order, for a fair game. |
| `--seed` | `0` | Fix the RNG for a reproducible game (`0` = random per game). |
| `--wild` | off | Wild format: allow banned cards in decks (ignores each card's `banned` flag so banned cards can be played). |

### What the UI looks like

The page is a single resizable dashboard (drag the dividers between
panels to resize):

- **Header** — engine version + build, the game seed, and a live
  connection indicator.
- **Board** (left) — the live ASCII board state, with a status line at
  the top showing turn number, phase, both scores (P1 green / P2 red),
  and whose decision it is.
- **Legal Actions** (top-right) — your legal moves as clickable buttons.
  **Click one to take it.** When the engine is resolving or the MCTS
  opponent is thinking, the buttons grey out and show *"engine is
  processing…"* so you can't double-act. Mid-resolution choices (e.g.
  "Discard a card", "Choose one —") show a prompt header plus
  human-readable option labels (`Yes`, `No`, a mode name, an `X` value)
  rather than bare indices.
- **Recent Actions** — a calm, high-signal feed of what each side just
  did (plays, kills, scores) so you can follow the opponent without
  reading the firehose.
- **Trace** — the full event log (every phase, decision, effect, damage,
  draw, etc.). Verbose; the Recent Actions feed is the readable summary.
- **Card Details** (right) — **hover any card name** anywhere in the UI
  to load that card's art (fetched on demand from Riot's CDN).
- **God Mode** (only when `--god-mode` is on) — a state editor to move
  cards between zones, set might/damage/exhaustion, edit scores/energy,
  reorder decks, and change phase. Handy for setting up specific
  scenarios or debugging.

Other modes: `--agent1 mcts:sims=50 --agent2 human` to play as P2,
`--agent1 human --agent2 human` for hot-seat (two players, one tab,
taking turns), and `--web on` with two AI seats to spectate. The
underlying WebSocket protocol is in [`docs/play-api.md`](docs/play-api.md).

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

## Card data

Card data is **hand-authored C++** — the source files are the single source of
truth. There is no `registry.json` and no code-generation pipeline. Each card
class implements `Card::def()`, returning its `CardDef` (cost, domains, tags,
keywords, art URL, `banned` flag, …); the engine builds its card table from those
at startup (`CardDB::buildFromClasses`). To add or edit a card, edit its
`src/cards/<type>/<id>_<slug>.cpp` directly.

```bash
# Export the compiled card table to registry-style JSON (validation / external use)
./build/riftbound --dump-registry /tmp/registry.json

# (optional) Pull raw card metadata from the official gallery, for reference when
# authoring new cards. Writes cards/raw/gallery_raw.json — it does NOT feed the engine.
python3 scripts/fetch_cards.py
```

| Script | Purpose |
|---|---|
| `scripts/fetch_cards.py` | Scrape Riftbound's official gallery into `cards/raw/gallery_raw.json`. Reference data for authoring new cards; not consumed by the engine. |
| `scripts/card_coverage.py` | Classify every `src/cards/<type>/<id>_<slug>.cpp` (implemented / metadata-only / stub / vanilla) and its in-code gap comments; exits non-zero while any card is a stub. `--write-lists` refreshes `docs/card-coverage/`. |
| `scripts/generate_replays.py` | Run `build/riftbound --render-html on` once per deck pair under `decks/` (the binary plays one pair per run) and file the HTML replays per pair. Visual V&V after card changes. |
| `scripts/rules_to_md.py` | Regenerate `rules/core-rules.md` from the rules PDF. |

## Repository layout

```
src/core/        ID types, events, intents, game state, card database
src/engine/      Turn loop, chain manager, effect executor, trigger manager,
                 batch runner, step driver (fiber-based)
src/cards/       Card subclasses — one TU per card under units/ spells/ gear/
                 legends/ battlefields/ runes/ (named <id>_<slug>.cpp), each
                 owning its CardDef data + behavior. Per-type bases live in
                 their type dir (gear/equip_base.h, units/weaponmaster_base.h);
                 cross-cutting helpers in card_helpers.h; cards_init.cpp is the
                 checked-in registration aggregator (one line per card).
src/agents/      AgentInterface + RandomAgent + HumanAgent
src/io/          HTML replay writer, ASCII state renderer, play server
                 (web UI) and its god-mode state editor
src/openspiel/   OpenSpiel Game / State subclasses, action vocabulary,
                 clone-correctness tests
src/ml/          Engine-side shared infra: feature extractor +
                 CFR utilities. No torch — these emit data for
                 downstream ML consumers.
src/rules/       Deck validator (tournament rules)
src/effects/     Effect type definitions

tests/           Google Test — engine behavior + card mechanics
cards/           raw/gallery_raw.json (reference art/metadata for authoring)
decks/           Sample decks (.txt deck lists, Piltover Archive format)
rules/           core-rules.md, tournament-rules.md, core-rules.pdf
errata/          Official errata documents
scripts/         Card gallery fetch, card coverage report, batch replay
                 generation, rules PDF -> Markdown
```

## Reference

- `rules/core-rules.md` — full game rules (2026-10-09 edition, sections 000–832)
- `rules/tournament-rules.md` — deck construction + tournament policies
- `CLAUDE.md` — coding standards + architecture conventions for contributors

## Status

This is a working development snapshot. The engine, OpenSpiel wrapper, and
test suite are stable. Card coverage is improving steadily — see
`scripts/card_coverage.py` output for per-card implementation status.

## License

TBD. Card data fetched from public Riot sources; card art is *not* bundled
(images are loaded on-demand from Riot's CDN by replay viewers / web UIs).
Riftbound is the property of Riot Games — this engine reproduces published
public rules for research and educational use.
