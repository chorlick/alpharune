# Fix run status

Maintained by the orchestrator. The authoritative copy is the one on the
`fix/integration` branch (`.worktrees/integration/plans/STATUS.md`); the copy
in the main checkout lags until integration is merged to master.

States: `pending` → `running` → `returned` → `merged` (or `blocked`).

## Run

| Field | Value |
|---|---|
| Current stage | 1 |
| Integration branch | `fix/integration` (`.worktrees/integration`) |
| Baseline commit (master at stage 0) | `c7143f1` |
| Baseline tests | 1511 run: 1508 passed, 2 failed, 1 skipped (1 disabled). Failing at baseline: `CardTestFixture.BaronNashor_IntegrationFullPlayPathCreatesPit`, `ChainTestFixture.FullGameRunsWithSpellsInDecks` (both: deleted deck file, item 7.40) |
| Last master sync into integration | `c7143f1` (branch point) |
| Last integration → master merge | — |
| Open checkboxes at start | 514 |

## Streams

| Stage | Stream | State | Branch head | Tests at merge | Notes |
|---|---|---|---|---|---|
| 1 | search | running | | | |
| 1 | helpers | running | | | |
| 1 | executor | running | | | |
| 1 | chain-triggers | running | | | |
| 1 | engine-auras-costs | running | | | |
| 1 | engine-play-combat | running | | | |
| 1 | io-cli | pending | | | gated: main checkout must have no uncommitted `src/io`, `src/main.cpp`, `docs/play-api.md` |
| 1 | integrity | pending | | | |
| 2 | cards-c0 | pending | | | |
| 2 | cards-c1 | pending | | | |
| 2 | cards-c2 | pending | | | |
| 2 | cards-c3 | pending | | | |
| 2 | cards-c4 | pending | | | |
| 2 | cards-c5 | pending | | | |
| 2 | cards-c6 | pending | | | |
| 2 | cards-c7 | pending | | | |
| 3 | sweep | pending | | | |
| 3 | perf-engine | pending | | | |
| 3 | perf-search | pending | | | |
| 3 | cleanup | pending | | | |
| 4 | strong-ids | pending | | | |
| 5 | checkpoint | pending | | | |

## Gates

| Gate | Passed | Evidence |
|---|---|---|
| Stage 0 baseline | yes | `c7143f1`; tests 1508/1511 (2 baseline failures above); smoke: 200 games fiora vs yi_rebuild, `--seed 20261010 --threads 6`, exit 0, P1 128 / P2 72 / 0 draws, no exception; `riftbound_clone_equiv_test` FAILS at baseline (hard-coded `decks/leblanc_test.txt`, item 7.40) and must pass at the stage 1 gate |
| Stage 1 | | |
| Stage 2 | | |
| Stage 3 | | |
| Stage 4 | | |
| Stage 5 | | |
| Final | | |

## Carry-over

Items a stream reported under "Needs another stream" or "New defects found",
and who picked them up. Every row must end `done` before the final gate.

| From | Item | Assigned to | State |
|---|---|---|---|

## Log

One line per orchestrator action that changed state (merge, gate, respawn).

- 2026-10-10 stage 0: plans committed on master as `c7143f1`; `fix/integration` created; baseline build, tests and smoke recorded. Gate passed.
- 2026-10-10 stage 1: spawned search, helpers, executor, chain-triggers, engine-auras-costs, engine-play-combat (6 running). integrity and io-cli wait for a slot (io-cli gate is open: main checkout clean).
