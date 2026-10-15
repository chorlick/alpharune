# 10 — Findings of the testing phase

Open items only. Closed items move to `plans/completed/10-testing-phase.md` with their outcome.

Found after the fix run by two find-and-record streams: `test-decks` (the 18 constructed lists read against
card text and the rules; detail, quoted trace lines and one-command replays in
`docs/testing-findings-decks-2026-10.md`) and `test-fuzz` (random legal decks from the whole pool; detail in
`docs/testing-findings-fuzz-2026-10.md` and `plans/reports/test-fuzz-1.md`).

Confidence is the finding stream's own grading and has not been checked: every item is verified against the
card text and `rules/core-rules.md` before anything is changed. An item ends FIXED with a test shown red
first, or NOT A DEFECT with evidence, a pinning test and a note at the code site, or as a QUESTION written
up for the judges with the current behaviour pinned.

Fixes are batched by root (user decision): one branch, one rebuild and one gate per batch.

## Batch 1 — Triggers and the chain: where abilities are queued, finalized and resolved; cleanup between chain items; the fuzz findings

All 21 items closed: `plans/completed/10-testing-phase.md` (report: `plans/reports/batch-1.md`).

## Batch 2 — Combat timing and continuous effects

All items closed: `plans/completed/10-testing-phase.md` (report: `plans/reports/batch-2.md`).

## Batch 3 — Targets, finalization and what goes on the chain: spells without targets, choices at resolution, counters, [Add], Equip and Quick-Draw

All items closed, with B2, B5, the batch-3 parts of B9 and H3, G14-G17, G19, G20, G27, J2, J3 and K1: `plans/completed/10-testing-phase.md` (report: `plans/reports/batch-3.md`).

## Batch 4 — Cards, options offered or withheld, payments, hidden information, the validator

Batch 4 is split so that its card-level part can run beside the engine-flow batches:
- **4a (cards, validator, test tooling; started 2026-10-11 beside batch 1):** all 23 items closed (V1, D12, D18, D20, D45, D47, D52, D55, D56, D59, D62, D65, D66, D69, D70, D73, D75, D81, F3, F4, F9, A1, A2): `plans/completed/10-testing-phase.md` (report: `plans/reports/batch-4a.md`). D73 was also listed under batch 2; it is closed here.
- **4b (generators, payments, log audience, questions; after the engine-flow batches):** D9, D11, D25, D28, D37, D43, D46, D48, D50, D58, D63, D71, D74, D80. All closed, with B3, B6-B8, G1, G2, G4, G18, G21-G25, H1, H2, I3 and the batch-4b part of H3: `plans/completed/10-testing-phase.md` (report: `plans/reports/batch-4b.md`).

## Found by reading 19 ISMCTS games (2026-10-11, `plans/reports/test-games19.md`, detail in `docs/testing-findings-games19-2026-10.md` section 4) — seen on the binary BEFORE batch 1; each is first re-checked on the current tree

- [ ] **G28** (after the batches) ISMCTS at 50 simulations plays near-uniformly (End Turn with ready runes, helpful effects on enemy units, option [0] of two-option prompts favoured, opponent's objects chosen at Targon's Peak): check whether the search tells own from opposing objects in a prompt and whether option order biases it — agents, observation

## Batch 5 — Trainer-facing engine work (owner, 2026-10-14: settled before v6 starts; trainer plan `../riftbound-trainer/plans/README.md` section 0)

- [ ] **T1** What a decision asks: every prompt carries a description — who asks, which card and ability (`ChainItem` / source), the kind of question (may / mode / X / target / cost / order / rune / location ...), and what each option means — exposed on `StepResult` and the decision API; a `MakeChoice` is no longer a bare integer to an observer. Covers every helper (`confirmOptional`, `pickMode`, `pickXAmount`, `pickTarget`, `pickTargetPair`, `selectSeveral`, `splitDamageAmong`, `confirmNow`, `chooseObjectNow`, engine prompts). Test: every prompt of every decision of a few hundred random and `random-player` games has a description, and two prompts with different meaning never share one — serious (trainer item 8)
- [ ] **T4** Completeness of the observation: list every fact a player may know (trainer plan section 2) and check each reaches the observation (knowledge tracker, known top/bottom runes per observer, revealed cards still in a hand, public zones up to a whole deck); the reverse of the leak test: two states that differ in a fact the observer may know give different observations — trainer items 10, 14

## Found by batch 2 (2026-10-14, `plans/reports/batch-2.md` sections 5 and 11)

- [ ] **T6** Derived facts as exact inputs (owner discussion, 2026-10-14: what the engine computes the network should not have to re-derive, and what a player could infer about hidden cards must be available): expose to the observation, per observer — effective Might and keywords after continuous effects, damage and lethal-on-board, combat designations and the combat in progress, each player's spendable resources (ready runes, floating Energy / Power, restricted pools) and the open timing window, and the opponent's legal card pool (cards of their legend's Domains, released sets, no banned card, minus copies already seen beyond the copy limit). Test: each fact matches what the engine itself uses for its rules on every decision of a few hundred games — trainer items 8-10

## Found by batch 4b (2026-10-14, `plans/reports/batch-4b.md` section 10)

- [ ] **K2** (after the owner's answer to G4) If an engine game is a Match game (CR 488.5), `GameRunner` / `BatchRunner` need series support so games two and three exclude battlefields already used (488.5, 488.6) — waits on G4

## Found by batch 5 / T3 (2026-10-14, `plans/reports/batch-5-t3.md` section 7)

- [ ] **L1** (agents, before ISMCTS teaches) Honest ISMCTS barely gains from its budget: head to head 100 v 50 51%, 200 v 100 55%, 400 v 200 47%, 400 v 50 61% (400 to 600 games each), and the clairvoyant variant at 100 simulations is no better than the honest one (49%, 50%). The score-difference leaf value is flat for most of a turn, so UCT spreads visits; a better leaf evaluation (or rollouts to the end of the turn) is what more simulations would need. Overlaps G28 — agents
- [ ] **L3** (owner, with G4) The setup battlefield choice is not searched by ISMCTS: the second player's search would see the first player's pick, held in a local of `setupBattlefields`, which a world cannot redraw; answered at random (CR 487.5). If engine games become Match games (CR 488.5) the pick must be made hidden-information-safe (state, not a local) before a search can make it — agents

## Found by batch 5b (2026-10-15, `plans/reports/batch-5b.md` section 7; N2, N3 are T1's)

- [ ] **N1** (batch 5c) The engine handles some simultaneous things in object-table (id) order: under other id labels 110 of 300 game pairs become different games (which identical rune pays, the order cards enter a deck together, ...). Not a leak (ids are random now); a determinism and test-coverage issue: order such ties by what the players see, or by the rules' order — CODE-ONLY

## Found by batch 3 (2026-10-15, `plans/reports/batch-3.md` section 10)

- [ ] **M1** (K3 kind, cards) 107 card files use a choice helper (`pickTarget`, `pickTargetPair`, `confirmOptional`, `pickMode`, `selectObject`, `selectSeveral`) and do not opt into a finalize pass (`choicesAtFinalization`). A choice the rules place at finalization — a target, a mode, a leading "may", a "[cost] to [effect]" payment (CR 355, 383.3, 403.1-403.2) — is then made at resolution, after both players had priority; a choice the effect itself makes ("choose" in the effect, a hidden-zone pick) belongs at resolution and is right. Each file needs reading and either the opt-in or a note at the card; batch 3 did the ones its items named — CODE-ONLY
- [ ] **M2** X-cost activated abilities and "pay [X]" effects do not count [Add] abilities when they work out the largest X the player can pay (Hextech Anomaly, Ancient Henge, Bullet Time's `max_x`): the payment itself can use them (CR 357.1.a, `addForPayment`), so an X the player could reach is not offered — CODE-ONLY
- [ ] **M3** A spell whose play intent names two or more targets and that has no pair helper or finalize pass (Singularity, Deathgrip, Smoke and Mirrors, Piercing Light: `getTargetRequirements` count > 1) gets no targets when an effect plays it (Fizz, Void Rush, Void Burrower, Wild Claw, Temporal Breach's kin): the effect-play path asks one play-time target or a spell's one intent target (G16 / D32 / D57), and `chooseFirstExecution` asks nothing for these, so the spell resolves with none. Ask them as for a hand play (`spellTargetSets`), or give the cards a pass — CODE-ONLY
- [ ] **M4** Thrill of the Hunt ("Banish a friendly unit, then its owner plays it to any battlefield"): with a unit its controller does not own, the spell's controller picks the battlefield and the owner plays it there; check whether the owner, who plays it, chooses the battlefield (CR 355.2.a) — QUESTION if the text does not settle it
- [ ] **M5** (convention, question if the owner wants one) An ability's effect play of a token or a card settles its location at finalization (Flame Chompers, Immortal Phoenix, Soulgorger, Herald of the Arcane, Zed after B5); CR 185.2.a ("tokens ... played ... following all the applicable steps for playing a card") can be read as the play's own 355.2 choice, at resolution. One judge question for the whole convention if the owner wants it settled — QUESTION (convention)


## Found by batch 5c-perf (2026-10-15, `plans/reports/batch-5c-perf.md` section 5)

- [ ] **P1** (owner decision on scope) Throughput is still 14% short of the pre-batch-1 rate per decision (187k against 218k decisions/s). What is left, by size: the Cleanup after every chain step (27%; 94% change nothing, but skipping a pass needs write tracking of the ~100 fields aura hooks read — a project); signals2 dispatch (17%, about 6% the library's own machinery; reworking the bus was ruled out earlier by the owner); allocation (13%: `Intent` holds 7 vectors — small-vector storage would cut most per-decision allocations); a `GameState` copy per pre-play probe and optional cost (2.7%, and the main cost of search, where every node copies); `refreshFlowGrants` reading every trash card each pass (2.9%); `canAffordWith` walking base runes per card per generation (3%) — perf
