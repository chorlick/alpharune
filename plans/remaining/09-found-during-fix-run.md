# 09 — Defects found during the fix run

Open items only. Closed items of this plan are in `plans/completed/09-found-during-fix-run.md`.

## O. From the sweep-verify-ledger report (2026-10-11)

- [ ] **9.O2** 9.E1 was closed as "not reproduced" (1,420 games without the resume failure) with its cause unidentified. The bounded stress tests stay in the suite; the stage 3, 4 and 5 gates each run the two original command lines from `plans/reports/perf-search.md` section 5 ten times and record the result here. Tick at the final gate if no run fails; a failure reopens it with the seed. — **owner: orchestrator (gates)**
  - 2026-10-11, sweep-verify-engine final run at `92c611d` (with the answer and legality checks in): both command lines ten times each, 20/20 rc=0, no resume failure.


## SS. From the checkpoint first interim report (2026-10-11)

- [ ] **9.SS1** The knowledge tracker is not told when a card shows a player hidden cards: Sabotage 156 ("They reveal their hand. Choose a non-unit card from it") emits `CardRevealedEvent` for each card and then offers them as options, but nothing calls `knowledge::seen`, so `GameKnowledgeView::about` says the caster does not know cards it is choosing between. 28 card files emit `CardRevealedEvent`; only this one was checked. Read every emitter and every other place a player is shown a hidden card (looks, peeks, Predict, reveals of a hand or a deck top, a hidden card played from facedown, cards drawn "revealed"): the tracker must learn it for exactly the players who saw it, from one place (the event's handler, with the audience on the event) rather than card by card; a card returned to a hidden zone is forgotten or remembered as the rules of the tracker say. Tests per kind of reveal through the engine, for both observers. The flat features do not read the tracker, so this changes nothing in the current encoding; it is what the v6 per-observer observation will read. — **owner: checkpoint**
