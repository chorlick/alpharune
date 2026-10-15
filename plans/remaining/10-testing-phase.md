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

- [ ] **M2** X-cost activated abilities and "pay [X]" effects do not count [Add] abilities when they work out the largest X the player can pay (Hextech Anomaly, Ancient Henge, Bullet Time's `max_x`): the payment itself can use them (CR 357.1.a, `addForPayment`), so an X the player could reach is not offered — CODE-ONLY
- [ ] **M3** A spell whose play intent names two or more targets and that has no pair helper or finalize pass (Singularity, Deathgrip, Smoke and Mirrors, Piercing Light: `getTargetRequirements` count > 1) gets no targets when an effect plays it (Fizz, Void Rush, Void Burrower, Wild Claw, Temporal Breach's kin): the effect-play path asks one play-time target or a spell's one intent target (G16 / D32 / D57), and `chooseFirstExecution` asks nothing for these, so the spell resolves with none. Ask them as for a hand play (`spellTargetSets`), or give the cards a pass — CODE-ONLY
- [ ] **M4** Thrill of the Hunt ("Banish a friendly unit, then its owner plays it to any battlefield"): with a unit its controller does not own, the spell's controller picks the battlefield and the owner plays it there; check whether the owner, who plays it, chooses the battlefield (CR 355.2.a) — QUESTION if the text does not settle it
- [ ] **M5** (convention, question if the owner wants one) An ability's effect play of a token or a card settles its location at finalization (Flame Chompers, Immortal Phoenix, Soulgorger, Herald of the Arcane, Zed after B5); CR 185.2.a ("tokens ... played ... following all the applicable steps for playing a card") can be read as the play's own 355.2 choice, at resolution. One judge question for the whole convention if the owner wants it settled — QUESTION (convention). Batch M1 found the rule that settles it: CR 420.3.b.1-420.3.b.2 say the play locations an effect names are "checked on finalization of the resulting Game Object" (the examples: a unit played to "an enemy occupied battlefield", Rek'Sai's "here"). That means they are chosen as the played card or token is finalized, during the resolution. The 23 spells and abilities batch M1 read do it that way; the abilities listed above settle the location at their own finalization, which the rule does not support



## Found by batch M1 (2026-10-15, `plans/reports/batch-m1.md` section 7; verified against the card text and code)

- [ ] **R1** Lunar Boon: the "LUNAR BOON: drew <card>" trace lines are `RIFT_TRACE` (both players), so they name the drawn cards to the opponent. The discard prompt's label also lists every card in hand. Use the private form (D11 kind) — CODE-ONLY
- [ ] **R2** Dragon Form: "Its base Might becomes 5 this turn" is written as `temp_might_bonus += 5 - base_might`. Two Dragon Forms on one unit stack: a base-2 unit becomes 8, a base-7 unit becomes 3 — CODE-ONLY
- [ ] **R3** Safety Inspector: the two units "each player must kill" are killed one after the other, not in one `DeathBatch`, so they do not die together (CR 370.1.a.2) — CODE-ONLY
- [ ] **R4** Acceptable Losses: the controller's gear is killed before the opponent chooses theirs, so the two are not simultaneous (CR 370.1.a.2; compare Cull the Weak). The options are also listed in object-table order, not by `boardObjects` — CODE-ONLY
- [ ] **R5** Token play locations:
  - Iterative Design and Vanguard Armory build their own lists, which do not leave out a battlefield units can't be played to (`battlefieldTakesUnitsFrom`).
  - Iterative Design, Vanguard Armory, Recruit the Vanguard and Guards! pass no `location_choices`, so an "additional copy" effect does not get the same choice.

  Use `playTokensChoosingLocations` / `tokenPlayDests` — CODE-ONLY
- [ ] **R6** The order of "banish/draw one ... recycle the rest":
  - Wild Claw, Reinforce and Dazzling Aurora recycle the rest before the chosen card is played. Lightning Rush trashes the rest before the draw.
  - Lightning Rush also puts the chosen card back on top and calls `drawCards(1)` instead of `drawHeldCard`.
  - Wild Claw and Promising Future (and Reinforce's location list) are built by hand. They ignore the unit's own play permissions and player grants (`playLocations`).

  CODE-ONLY
- [ ] **R7** Cursed Sarcophagus: `canActivateAbility` asks whether ANY copy the player controls, in any zone, tracks a banished unit. So a copy with nothing banished with it can be activated and exhausted for nothing. Use `canActivateAbilityAt` with `source`, as The Zero Drive does. A replayed Sarcophagus also keeps its `tracked_objects` (CR 124.1) — CODE-ONLY
- [ ] **R8** Siphon Power ("Choose a battlefield") and Firestorm ("all enemy units at a battlefield") choose a unit to stand for the battlefield, but the battlefield is the target (CR 355.10.b). As a result:
  - a battlefield with no unit (Siphon) or no chooseable enemy unit (Firestorm) can't be chosen;
  - the anchor's Deflect is charged;
  - an anchor moved in response makes the spell do nothing.

  CODE-ONLY
- [ ] **R9** Whirlwind: each player's pick is not a target (CR 355.10.e). Yet the pick is filtered by "can't be chosen" and charged Deflect (CR 809 applies to spells that target) — CODE-ONLY
- [ ] **R10** Back Off: "If you played this from your hand, draw 1" is tested as "not played from facedown". A play from the trash or by an effect from another zone still draws (`Card::kPlayedNotFromHand` says where the play came from) — CODE-ONLY
- [ ] **R11** Arcane Shift: "Banish a friendly unit, then its owner plays it". For a unit its controller does not own, the controller is asked where it is played. Batch 5d's M4 settles the same question for Thrill of the Hunt; apply the answer here — CODE-ONLY
- [ ] **R12** Card data:
  - Disposal Order's `ability_text` runs its modes together ("them.Draw 1.").
  - Hwei, Brooding Painter's has no spaces ("Draw 1.Gear", "runes.Unit").
  - Scryer's Bloom's def carries `Keyword::Predict`, which belongs to its ability, not the card (plans/08 8.8). The ledger test does not catch it.

  CODE-ONLY
- [ ] **R13** Picks from a hidden zone made with `pickTarget`: Here to Help, Decree of Strength, Void Rush, Double Trouble, Ornn, Fate Weaver, Rift Herald, Insightful Investigator, Ashe, Bone Skewer. These are not targets (CR 355.10.a), but the prompt is labelled "target:" and runs the target bookkeeping (no Deflect is charged off the board). Give them a non-target helper and prompt kind (with batch 5d's prompt kinds) — CODE-ONLY
- [ ] **R14** (QUESTION if the owner wants one) Maduli the Gatekeeper reads "an occupied enemy battlefield" as "a battlefield with enemy units". "Occupied" is defined (CR 170.11.a) but "enemy battlefield" is not; the other reading is "controlled by an opponent" — QUESTION

## Found by batch 5c-perf (2026-10-15, `plans/reports/batch-5c-perf.md` section 5)

- [ ] **P1** (owner decision on scope) Throughput is still 14% short of the pre-batch-1 rate per decision (187k against 218k decisions/s). What is left, by size: the Cleanup after every chain step (27%; 94% change nothing, but skipping a pass needs write tracking of the ~100 fields aura hooks read — a project); signals2 dispatch (17%, about 6% the library's own machinery; reworking the bus was ruled out earlier by the owner); allocation (13%: `Intent` holds 7 vectors — small-vector storage would cut most per-decision allocations); a `GameState` copy per pre-play probe and optional cost (2.7%, and the main cost of search, where every node copies); `refreshFlowGrants` reading every trash card each pass (2.9%); `canAffordWith` walking base runes per card per generation (3%) — perf
