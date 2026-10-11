# 09 — Defects found during the fix run

Open items only. Closed items of this plan are in `plans/completed/09-found-during-fix-run.md`.

## O. From the sweep-verify-ledger report (2026-10-11)

- [ ] **9.O2** 9.E1 was closed as "not reproduced" (1,420 games without the resume failure) with its cause unidentified. The bounded stress tests stay in the suite; the stage 3, 4 and 5 gates each run the two original command lines from `plans/reports/perf-search.md` section 5 ten times and record the result here. Tick at the final gate if no run fails; a failure reopens it with the seed. — **owner: orchestrator (gates)**
  - 2026-10-11, sweep-verify-engine final run at `92c611d` (with the answer and legality checks in): both command lines ten times each, 20/20 rc=0, no resume failure.

## TT. From the checkpoint final report (2026-10-11)

- [ ] **9.TT1** Loose ends of stage 5, all small: (a) hand reveals are announced as private — Sabotage 156, Mindsplitter, Decree of Strength, Insightful Investigator and Ashe emit `CardRevealedEvent` with `revealed_to` = the caster although the text says "They reveal their hand"; in 1v1 the owner already knows their hand so nothing differs, but the event should say what the card says: make the reveal public (both players) and pin it, or pin with a comment why caster-only is right; (b) `KnowledgeReveals.AnOpponentsRevealedTopCardIsKnownToBoth` (Blind Fury) and `ALookAtTheTopCardsIsKnownBeforeTheOfferMadeFromThem` (Fate Weaver) pass before and after the fix and do not show the gap their comments name: make each check the moment the gap existed (while the prompt is open) so it is red on the old code, or correct the comment to say it is a pin; (c) three tests loop over card ids `1..1100` (`test_review_fix_sweep_triggers.cpp` twice, `test_review_fix_verify_engine.cpp`): read `ml::kCardVocabSize`, and grep `src/` and `tests/` for any other literal 1100 / 1101 / 955 / 956 that means the id space. — **owner: checkpoint**
