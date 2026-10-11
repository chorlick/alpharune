# 09 — Defects found during the fix run

Open items only. Closed items of this plan are in `plans/completed/09-found-during-fix-run.md`.

## O. From the sweep-verify-ledger report (2026-10-11)

- [ ] **9.O2** 9.E1 was closed as "not reproduced" (1,420 games without the resume failure) with its cause unidentified. The bounded stress tests stay in the suite; the stage 3, 4 and 5 gates each run the two original command lines from `plans/reports/perf-search.md` section 5 ten times and record the result here. Tick at the final gate if no run fails; a failure reopens it with the seed. — **owner: orchestrator (gates)**
  - 2026-10-11, sweep-verify-engine final run at `92c611d` (with the answer and legality checks in): both command lines ten times each, 20/20 rc=0, no resume failure.

## RR. From the strong-ids final report (2026-10-11)

- [ ] **9.RR2** `Intent::chosen_battlefield` has two meanings: in `HideCard` it is a battlefield slot (`BattlefieldId`), in the setup decision `ChooseBattlefield` it is the position of a card in the player's battlefield list, before any battlefield exists. Give the setup choice a field of its own type. It changes an action slot and a history token, so it belongs with the encoding work. — **owner: checkpoint (stage 5)**
