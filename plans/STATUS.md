# Fix run status

Maintained by the orchestrator. The authoritative copy is the one on the
`fix/integration` branch (`.worktrees/integration/plans/STATUS.md`); the copy
in the main checkout lags until integration is merged to master.

States: `pending` → `running` → `returned` → `merged` (or `blocked`).

## Run

| Field | Value |
|---|---|
| Current stage | 0 (not started) |
| Integration branch | `fix/integration` (not created) |
| Baseline commit (master at stage 0) | — |
| Baseline tests | — |
| Last master sync into integration | — |
| Last integration → master merge | — |
| Open checkboxes at start | 514 |

## Streams

| Stage | Stream | State | Branch head | Tests at merge | Notes |
|---|---|---|---|---|---|
| 1 | search | pending | | | |
| 1 | helpers | pending | | | |
| 1 | executor | pending | | | |
| 1 | chain-triggers | pending | | | |
| 1 | engine-auras-costs | pending | | | |
| 1 | engine-play-combat | pending | | | |
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
| Stage 0 baseline | | |
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
