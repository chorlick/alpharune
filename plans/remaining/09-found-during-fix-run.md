# 09 — Defects found during the fix run

Open items only. Closed items of this plan are in `plans/completed/09-found-during-fix-run.md`.

## O. From the sweep-verify-ledger report (2026-10-11)

- [ ] **9.O2** 9.E1 was closed as "not reproduced" (1,420 games without the resume failure) with its cause unidentified. The bounded stress tests stay in the suite; the stage 3, 4 and 5 gates each run the two original command lines from `plans/reports/perf-search.md` section 5 ten times and record the result here. Tick at the final gate if no run fails; a failure reopens it with the seed. — **owner: orchestrator (gates)**
  - 2026-10-11, sweep-verify-engine final run at `92c611d` (with the answer and legality checks in): both command lines ten times each, 20/20 rc=0, no resume failure.


## SS. From the checkpoint first interim report (2026-10-11)

- [ ] **9.SS2** `kCardVocabSize` is 1100: ids 1..955 are registered through Vendetta and 956..1100 (145 slots) are reserved for Radiance. The gallery data (`cards/raw/gallery_raw.json`) numbers Radiance `RAD-001/167` and holds 192 Radiance entries with about 180 distinct (name, text, cost, Might, type) combinations, so 145 slots may not hold the set. Establish from the gallery data how many ids Radiance needs (distinct game cards, tokens it introduces, minus anything that already has an id such as basic runes), and since this stage changes the layout anyway, size `kCardVocabSize` (and `openspiel::kNumCardDefIds`, the feature dim and the card-keyed action ranges derived from it) so that Radiance fits with stated headroom; record the count and the reasoning in `docs/checkpoint-breaking-2026-10.md` and CLAUDE.md. If 145 is enough, say so with the count and leave the size alone. — **owner: checkpoint**
