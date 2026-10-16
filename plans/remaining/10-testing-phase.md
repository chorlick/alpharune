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


## Found by batch 2 (2026-10-14, `plans/reports/batch-2.md` sections 5 and 11)


## Found by batch 4b (2026-10-14, `plans/reports/batch-4b.md` section 10)

- [ ] **K2** (after the owner's answer to G4) If an engine game is a Match game (CR 488.5), `GameRunner` / `BatchRunner` need series support so games two and three exclude battlefields already used (488.5, 488.6) — waits on G4
  - Owner, 2026-10-15: Duel by default for now; series support is needed only when Match series are wanted — stays open, not scheduled.

## Found by batch 5 / T3 (2026-10-14, `plans/reports/batch-5-t3.md` section 7)

- [ ] **L1** (agents, before ISMCTS teaches) Honest ISMCTS barely gains from its budget: head to head 100 v 50 51%, 200 v 100 55%, 400 v 200 47%, 400 v 50 61% (400 to 600 games each), and the clairvoyant variant at 100 simulations is no better than the honest one (49%, 50%). The score-difference leaf value is flat for most of a turn, so UCT spreads visits; a better leaf evaluation (or rollouts to the end of the turn) is what more simulations would need. Overlaps G28 — agents
- [ ] **L3** (owner, with G4) The setup battlefield choice is not searched by ISMCTS: the second player's search would see the first player's pick, held in a local of `setupBattlefields`, which a world cannot redraw; answered at random (CR 487.5). If engine games become Match games (CR 488.5) the pick must be made hidden-information-safe (state, not a local) before a search can make it — agents
  - With Duel as the default there is no searched setup decision; stays open for Match mode only, not scheduled.

## Found by batch 5b (2026-10-15, `plans/reports/batch-5b.md` section 7; N2, N3 are T1's)

N1, N2, N3 closed by batch 5d.


## Found by batch 3 (2026-10-15, `plans/reports/batch-3.md` section 10)




## Found by batch M1 (2026-10-15, `plans/reports/batch-m1.md` section 7)

All closed by batch 6 (R1-R14), with M5: `plans/completed/10-testing-phase.md` (report: `plans/reports/batch-6.md`).

## Found by batch 5c-perf (2026-10-15, `plans/reports/batch-5c-perf.md` section 5)

- [ ] **P1** (owner decision on scope) Throughput is still 14% short of the pre-batch-1 rate per decision (187k against 218k decisions/s). What is left, by size: the Cleanup after every chain step (27%; 94% change nothing, but skipping a pass needs write tracking of the ~100 fields aura hooks read — a project); signals2 dispatch (17%, about 6% the library's own machinery; reworking the bus was ruled out earlier by the owner); allocation (13%: `Intent` holds 7 vectors — small-vector storage would cut most per-decision allocations); a `GameState` copy per pre-play probe and optional cost (2.7%, and the main cost of search, where every node copies); `refreshFlowGrants` reading every trash card each pass (2.9%); `canAffordWith` walking base runes per card per generation (3%) — perf
  - **Owner, 2026-10-15: "lets keep the gap for now."** No structural throughput work (write tracking, event bus). Revisit only if profiling during real training shows engine time matters. Closed as an owner decision once batch 7-perf lands.


## Found by batch 5d (2026-10-15, `plans/reports/batch-5d.md` section 8)

- [ ] **Q1** (N1 tail) Under other object ids 1 to 3 game pairs in 300 still become other games (the relabelled test allows 1 in 50): cards returned to a hand together enter it in the order the effect listed them; two Zero Drives triggering for one death track the banished unit by trigger order; an effect that buffed one of several identical tokens. Each is a list a card builds in id order — CODE-ONLY
- [ ] **Q2** (perf) Batch 5d costs +1.9% instructions on the throughput benchmark (T1's prompt carried at every decision: +0.7% on identical games; the rest the N1 orderings and other games); wall -2.6% at load 3.1-3.7 — perf

## Found by batch 6 (2026-10-15, `plans/reports/batch-6.md` section 9)

- [ ] **S1** Thermo Beam ("Kill all gear") kills the gear one after another, with no `DeathBatch`: they do not die together (CR 370.1.a.2), so a gear's "when a gear dies" ability can see another die in the same kill (CR 383.2.c.2), and their death abilities are not one simultaneous batch (383.3.d). Batch 6's R4 put Acceptable Losses' two gear in one batch; Thermo Beam is the other card that kills several gear in one effect — CODE-ONLY

## Found by round 2 (2026-10-15, `plans/reports/round2.md`, detail in `docs/testing-findings-round2-2026-10.md` section 4) — batch 8

- [ ] **U1** REGRESSION of D4: a [Weaponmaster] unit with no Equipment puts its ability on the chain, gives priority and resolves with nothing (`weaponmaster_base.h` never calls `confirmOptional`) — CERTAIN
- [ ] **U2** REGRESSION of D4: Azir, Sovereign's attack trigger with no token to move stays on the chain (no leading `confirmOptional`) — LIKELY
- [ ] **U3** Curtain Call / Rocket Barrage offered, a mode chosen, the pre-cost choice filter refuses every unit and `executePlaySpell` throws (the only fuzz failure, 1 game per million) — CERTAIN
- [ ] **U4** Two abilities triggered by one play (`CardPlayedEvent` and "played_not_from_hand") are separate batches: their controller is never asked the order (CR 383.3.d) — CERTAIN
- [ ] **U5** A battlefield replaced by Green Father's Brush loses its "here" (CR 439.1.a): Brush's swap-back does nothing, Seat of Power draws for its own slot, Green Father after a swap-back pays and does nothing — CERTAIN / LIKELY
- [ ] **U6** Jhin, Meticulous Killer's "[B]" play offered only from the Champion Zone and only to base — CERTAIN
- [ ] **U7** Optional additional costs never offered for a play from facedown (Pyke, Dockside Butcher) — CERTAIN
- [ ] **U8** Piercing Light's [Repeat] execution goes through the generic `makeNewChoicesFor` (base units offered, "up to one" not declinable) — CERTAIN
- [ ] **U9** Piercing Light played by an effect is offered "No target" for its required first unit — CERTAIN
- [ ] **U10** [Repeat] not offered on an effect play that pays (Fizz, Trickster) — LIKELY
- [ ] **U11** [Repeat] decided, and its choices made, after the base cost is paid (CR 355.1.a, 820.2) — LIKELY
- [ ] **U12** Rek'Sai, Breacher's granted [Accelerate] not offered to unit tokens played by an effect — LIKELY
- [ ] **U13** Kennen's [Flow] grant ignored on a spell that prints [Flow] (CR 829.1.c.3) — LIKELY
- [ ] **U14** Sacrifice's additional cost asked and recorded as a target (CR 355.10.c) — LIKELY
- [ ] **U15** Nami, Headstrong's "if you paid the additional cost" judged in the effect, not `triggerCondition` — LIKELY
- [ ] **U16** `describePrompt`: an effect-played spell is "its triggered ability"; an activated ability with a target or a reflexive trigger is "its play"; a battlefield is named by its owner — LIKELY
- [ ] **U17** Options a player cannot tell apart: order prompts for copies of one ability; a token copy and the card in the same words — QUESTION
- [ ] **U18** Trash-play payments skip forced rune picks and their rune prompts name no card — LIKELY
- [ ] **U19** Log lines that misstate: Fizz's "then recycled it"; Moonfall's "-> 1M"; `TGT_NOT_OFFERED ... at resolve time` at finalization — LIKELY
- [ ] **U20** Two decks-stream rules questions never carried into the judge questions (5+ [M] entering "becomes Mighty"; Prodigal Explorer's "chosen twice") — QUESTION
- [ ] **U21** Every Gold token is card 326; the Unleashed Gold 564 never enters play — QUESTION
- [ ] **C1** (batch 8, coverage for the stopping rule) Six cards below 200 resolutions per optimised million (Yasuo, Windrider 205 / 546; Eclipse Dragon; Swain, Visionary; Rell, Magnetic; Renekton, Rage Fueled): lists or a guided mechanism so each is exercised at the minimum in the pass, without lowering it

## Owner decisions, 2026-10-15

- [ ] **V1-G4** (batch 8) Battlefield setup mode. Owner on G4: "Duel for right now (the trainer will not choose random however)". A game-mode setting: **Duel (default)** — each player's battlefield drawn at random from their three (CR 487.5), from the engine's RNG, no setup decision; **Match** — each player chooses (CR 488.5, today's behaviour), selectable from the CLI, `GameRunner` / `RiftboundGame` parameters and the OpenSpiel game parameters, so the trainer's agent can choose. The pin `Batch4bTest.G4_Pin_EachPlayerChoosesTheirBattlefield` becomes a Match-mode test; add a Duel-mode test (random, seeded, no decision, the draw hidden from no one since battlefields are revealed). Smoke and fixed-seed games will move (two decisions fewer per game): explain. CLAUDE.md and `docs/trainer-notes-2026-10.md` updated.
