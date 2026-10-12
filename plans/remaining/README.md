# What is still open

The review fix run is closed (760 items, `plans/completed/`). The **testing phase** that followed it has 93
open items in one file, grouped by fix batch:

| File | Items | What |
|---|---|---|
| `10-testing-phase.md` | 93 | F1-F9, V1 and two tooling items (A1, A2) from the random-deck fuzz; D1-D81 from reading the 18 constructed deck lists against card text and the rules |

| Batch | Root | Items | State |
|---|---|---|---|
| 1 | Triggers and the chain; cleanup between chain items; loops | 21 | running |
| 2 | Combat timing and continuous effects | 11 | waiting |
| 3 | Targets, finalization, counters, [Add], Equip and Quick-Draw; payments left unpayable | 25 | waiting |
| 4 | Cards, options, payments, hidden information, the validator, test tooling | 36 | waiting |

Confidence on each item is the finding stream's own and is checked before anything changes. More findings
arrive with each fuzz round (`docs/testing-phase-2026-10.md`).

Not plan items, kept elsewhere:
- Play-UI follow-ups deferred by the user: `docs/play-ui-followups.md`.
- Performance recommendations not acted on: `docs/perf-followups.md`.
- What a trainer or any stored data must change after stage 5: `docs/checkpoint-breaking-2026-10.md`;
  earlier API notes: `docs/trainer-notes-2026-10.md`.
- Questions still with the user: reprint region tags (per-printing vs union) and Karma, Channeler 235's
  tags (`plans/STATUS.md`).
- Rulings obtained from judges on 2026-10-11: `docs/judge-questions-2026-10.md`.
