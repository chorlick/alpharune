# 09 — Defects found during the fix run

Open items only. Closed items of this plan are in `plans/completed/09-found-during-fix-run.md`.

## UU. User decision on the card id space (2026-10-11)

- [ ] **9.UU1** USER DECISION: reserve 210 card ids for Radiance, and room for Legacy, announced with roughly 400 cards. The id space (`ml::kCardVocabSize` / `openspiel::kNumCardDefIds`) is 1150 after 9.SS2 (955 registered + 195 reserved). Size it for 955 + 210 + 400 = 1565 with stated headroom (orchestrator: 1600 unless a derived size argues for another round number), name the reserved ranges where the constant is defined (Radiance 956..1165, Legacy 1166..), keep every `static_assert` and derived size consistent (`kStateFeatureDim`, `kCardKeys`, `kNumSlots`, `NumDistinctActions`, `kCardEmbeddingRows`, `kLayoutVersion`), re-pin the constants the tests pin, update `docs/checkpoint-breaking-2026-10.md` and CLAUDE.md, and measure what the larger vector costs (`InfoSetId`, `ObservationTensor`, the search batches, random play). — **owner: checkpoint**
