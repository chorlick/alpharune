# Batch M1: choices made without a finalize pass

Branch `fix/batch-m1`, from master `9db0efb`. Master `4a2ebd5` (a status commit) is merged. **Batch 5d had not
merged when this gate ran**, so the gate is on this branch plus current master (see section 6).

## 1. Count and method

**156 card files** ask a player something in a callback that does not opt into a finalize pass. The plan's figure
was about 107; that count used six helpers only. `scripts/scan_choices_without_finalize.py` reproduces the list. It
counts every choice helper, every direct `requestChoice` call, and the discard, location and synchronous prompts that
`onResolve` / `onTrigger` / `onEquippedTrigger` / `onActivate` can reach.

The list and the verdicts for each card and each choice are in `plans/reports/batch-m1-list.md`. Three of the 156
are false positives of the scan: Akshan, Jax and Yone inherit `kFinalizeTrigger` from `WeaponmasterUnit`.

**Who read what.** Five sub-agents each read their share of the files in full (29 + 27 + 32 + 33 + 35). Before their
reports came back, I had already set a verdict for every file from its printed text. Their verdicts were checked
against mine, and every disagreement was settled from the card text and the rules. I read each file that changed in
full before changing it.

## 2. Verdicts

- **Moved to finalization: 24 cards.**
  - Gear and legends:
    - Bottled Constellation: the leading may, plus the kill-3 cost, which is paid as the ability is finalized (CR 383.3.b).
    - Glowstone: the player.
    - Unforgiven: the destination.
  - Spells:
    - Convergent Mutation: its second target.
    - Morbid Return and The Harrowing: the unit in the trash, which is a target because the trash is public. They now also require one to be there before they can be played.
    - Icathian Rain: instances 2 to 6.
    - Zenith Blade: the friendly unit (CR 355.12). Its "may" stays at resolution.
    - Arcane Shift: both units, chosen as a pair.
    - Tricksy Tentacles: the units and the location. If the group no longer fits at resolution, the controller picks a CR 355.11.b subset.
    - Call to Battle, Twilight Step, Shadow Dash: the destination.
    - Stare Down, Bone Skewer: the battlefield.
    - Disposal Order: the mode and the cards.
    - Shadows of the Past: up to 2 units.
    - Decree of Discord: the group (CR 355.11.b subset).
    - Acceleration Gate: up to 4 objects. Ready objects are now offered too, because the text has no "exhausted".
  - Units:
    - Dr. Mundo: the three cards from the trash.
    - Fae Dragon: up to four units, buffed units included (CR 427.1.c).
    - Iascylla: the delayed ability's may and its unit.
    - Maduli: the destination.
    - Corrupted Dragon: the may and the units.
- **Correctly at resolution.** Each such call now has a one-line `// At resolution: ... (CR ...)` comment: 89 calls in 70 files. They are:
  - picks from a hidden zone (CR 355.10.a; banishment is not public);
  - a choice made by each player or by another player (355.10.e);
  - a "must" (355.10.f);
  - where an effect plays a card or token (CR 420.3.b.2);
  - a spell's "may" (355.12, 355.18), and a "may" that is not the first part of the ability (383.3.a.3);
  - [Add] abilities (401.2);
  - Albus Ferros's buffs (355.9.a).
- **Already made at finalization.** Every play-time and activation-time target the engine asks for.
- **Questions for the judges.** I added three to `docs/judge-questions-2026-10.md`, each with the engine's current behaviour pinned by a test:
  - **14 — Card Sharp.** Is "you and each opponent may" a leading may? Pinned: each player decides at resolution.
  - **15 — Dr. Mundo with fewer than three cards in the trash.** Pinned: the ability is removed under CR 403.4, read strictly as in Q6. It is added to the removal sweep's list with that reason.
  - **16 — A replaced death inside "kill X to".** CR 357.2.a says a replaced cost is still paid; the engine's `killAsCost` says it is not. Bottled Constellation now follows the engine's reading, the same as Dusk Rose Lab and Overzealous Fan.

**Rule found for M5.** CR 420.3.b.1-420.3.b.2 settle item M5: an effect play's location is "checked on finalization
of the resulting Game Object", which happens as the effect resolves. The abilities M5 lists choose it at their own
finalization, and this rule does not support that. I added this to M5; the owner decides what to do with the
convention.

## 3. Engine change, helper and docs

- **Engine** (`GameEngine::chooseFirstExecution`): an activated ability now runs its `kFinalizeActivate` pass after
  an intent target or an engine-asked target. Without this, no helper could express Unforgiven's destination,
  because the pass ran only for abilities that had no target. Equip is excluded. Only Unforgiven has both today.
- **New helper** `chooseFittingSubset` in `card_helpers.h`: CR 355.11.b / 355.11.b.1, a legal subset with as many
  targets as possible. It brute-forces up to 16 targets and throws above that.
- **CLAUDE.md**:
  - the activation pass after a target;
  - the group helper;
  - which choices stay at resolution, and the comment convention for them.

## 4. Tests

- **New tests:** `test_batch_m1_spells.cpp` (29) and `test_batch_m1_abilities.cpp` (16), with shared agents in
  `batch_m1_agents.h`. Each test checks that the choice is asked before P2's first priority and is stored for
  resolution. Where it applies, a target that leaves in response is a mistarget and nothing is asked again.
- **Written first:** of the first 44 tests, 40 failed on the unchanged cards (`m1_before.log`). The other four pin
  behaviour that stays. The 45th test (the Arcane Shift Deflect gate) came after the fuzz finding.
- **Changed tests** (old -> new), all because of the timing change:
  - Corrupted Dragon: the leading may is its own prompt now. Six tests changed (C7, VendettaB ×2, VerifyCards ×2, VerifyEngine).
  - Zenith Blade: prompt counts and order changed (choice_helpers ×2, C2).
  - Arcane Shift: the units are chosen as a pair, not from the intent (C3, VerifyCards ×2).
  - Acceleration Gate: a ready object is offered (VendettaE, VerifyCards).
  - Convergent Mutation: its pin now sees the prompt at play time.
  - Unforgiven: destination codes changed (C1).
  - Dr. Mundo: "ThreeOrFewerAreAllRecycledUnasked" became "ExactlyThree…" plus a pin for Q15 (C4).
  - Fae Dragon: a buffed unit is offered (CR 427.1.c).
  - Strong-id audit counts updated.
- **Sweeps** all green, with two list changes:
  - `FinalizePassSpellDeclarationsMatchTheirPickLists`: Disposal Order joins the modal map.
  - `Removal_…ExceptTheListedOnes`: Dr. Mundo is listed with the CR 403.4 reason (Q15).
  - `CardContract_…` and `D8_Sweep`: green.

## 5. Gate (final tree `bcf5675`, current master merged)

- **Build:** clean rebuild of `all` plus `deck_fuzz`, lane A, -j3. Our code has one warning, in
  `src/openspiel/clone_breakdown.cpp:51` (`-Wmismatched-new-delete`). It is pre-existing: M1 did not touch the file, and the target is built only by `all`. The other 8 warnings are in `_deps`. The checked build has 0 warnings.
- **Suites:**
  - `riftbound_tests`: 3,526 passed (1 disabled).
  - `riftbound_openspiel_tests`: 33 passed.
  - `clone_equiv`: 10/10.
  - Checked build: 3,526 passed.
  - `HiddenOrderLeak.*`: green.
- **Smoke** (fiora v yi_rebuild, 200 games): 136 / 64 / 0, 288 decisions. This is identical to master's binary, and a
  20-game `--trace` comparison is byte-identical apart from the banner. No card M1 changed is in those decks, so nothing moved.
- **MCTS seed 7:** rc 0 (1-3). With `RIFTBOUND_STEP_FAULT_AT=40`: rc 1, injected fault, 0 draws.
- **ISMCTS:** both fixed-seed commands, ten runs each, rc 0 every time, and each command's output was the same in all ten runs.
- **Fuzz, optimised,** 3 × 100,000 (seed 5150821, `--known`): 0 failures of any kind.
  - The earlier seed 5150811 flagged two games. Both are identical on master's `deck_fuzz` and have an explanation:
    - a 200-turn game in which Endless Riches skips every draw and both boards are empty;
    - a chain of 200, which is the Irresistible Faefolk / Imposing Challenger loop stopped by Tournament Rules 505.
  - The same seed's pass=0 run found **one real failure caused by M1**: Arcane Shift was offered when the only enemy unit had a Deflect it could not pay beside the cost. It is fixed by choosing the two units as a pair, and a test covers it.
- **Fuzz, checked,** 3 × 20,000 (seed 5150812): 0 failures.
- **Pairing,** every ordered pairing of the 39 lists × 7 games (10,647 games), on both builds: 0 failures. The two
  builds produced the same results.
- **Throughput,** 1,000 games, 1 thread, fiora v yi_rebuild, three runs alternating with master:
  - master: 636.7, 635.8, 626.2 (mean 632.9);
  - M1: 648.0, 636.1, 639.1 (mean 641.1);
  - the difference is noise, and the games are identical.
  - Whole-pool fuzz with random agents: master 1,952 games/s against M1 1,960. Here the finalize passes of the changed cards are exercised, and they cost nothing measurable.

## 6. Not done as asked

- **Batch 5d had not merged** when the gate ran (master was still `4a2ebd5`), so the gate is on M1 plus current
  master. When 5d merges, the merge needs:
  - the `PromptKind` argument on the direct `requestChoice` calls that M1 rewrote or added: Icathian Rain,
    Tricksy Tentacles, Decree of Discord, `chooseFittingSubset`, Call to Battle;
  - care with the comments in Thrill of the Hunt (5d's M4) and in Hextech Anomaly / Ancient Henge (5d's M2);
  - a renumbering of judge questions 14-16 if 5d also added questions;
  - a rerun of the gate.
- **One warning in our code** remains (`clone_breakdown.cpp`). It is pre-existing and outside M1's scope.

## 7. New defects found (not fixed; `plans/remaining/10-testing-phase.md`, items R1-R14)

- **R1** Lunar Boon tells the opponent which cards were drawn (the trace line is not private).
- **R2** Two Dragon Forms on one unit stack.
- **R3** Safety Inspector's two kills are not in one death batch.
- **R4** Acceptable Losses' kills are not simultaneous.
- **R5** Token location lists ignore battlefields that block unit plays, and leave no `location_choices` (Iterative Design, Vanguard Armory, Recruit the Vanguard, Guards!).
- **R6** "Recycle the rest" happens before the chosen card is played or drawn (Wild Claw, Reinforce, Dazzling Aurora, Lightning Rush). Lightning Rush also does not use `drawHeldCard`.
- **R7** Cursed Sarcophagus's activation gate looks at every copy, not its own.
- **R8** Siphon Power and Firestorm target a unit instead of the battlefield.
- **R9** Whirlwind charges Deflect on picks that are not targets.
- **R10** Back Off's "from your hand" check is wrong.
- **R11** Arcane Shift: the owner should choose where the unit is played (as 5d's M4 does for Thrill of the Hunt).
- **R12** Card text and data errors (Disposal Order, Hwei, Scryer's Bloom's keyword).
- **R13** Picks from hidden zones go through `pickTarget`.
- **R14** Maduli's "enemy battlefield" — a question.
