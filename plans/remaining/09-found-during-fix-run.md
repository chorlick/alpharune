# 09 — Defects found during the fix run

Open items only. Closed items of this plan are in `plans/completed/09-found-during-fix-run.md`.

## O. From the sweep-verify-ledger report (2026-10-11)

- [ ] **9.O2** 9.E1 was closed as "not reproduced" (1,420 games without the resume failure) with its cause unidentified. The bounded stress tests stay in the suite; the stage 3, 4 and 5 gates each run the two original command lines from `plans/reports/perf-search.md` section 5 ten times and record the result here. Tick at the final gate if no run fails; a failure reopens it with the seed. — **owner: orchestrator (gates)**
  - 2026-10-11, sweep-verify-engine final run at `92c611d` (with the answer and legality checks in): both command lines ten times each, 20/20 rc=0, no resume failure.

## KK. From the perf-engine final report (2026-10-11)

- [ ] **9.KK1** 6.10's second bullet: a `GameState` copy is expensive because of what a `GameObject` holds (`name`, `tags`, string-keyed `card_counters`, read by name in several hundred card files); the copy is the largest single cost left in search (about 4% of `mcts:sims=50`, about 18 µs each). Make the per-object copy cheap (static facts read from the `CardDef`, counters keyed by an interned id or a small flat map) with numbers before and after. Moved to stage 4 by name: it is an API change over the same card files as 5.5. — **owner: strong-ids**
