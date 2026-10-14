# batch-4a — final report: cards, the validator, test tooling

## 1. Result

- Branch `fix/batch-4a`. 23 items, all closed: 22 FIXED, A2 done (a deck list). No NOT A DEFECT, no QUESTION.
- Gate on the final tree: the code is that of `2712d68`, which has master `a0cef94` merged in (master had only plans,
  docs and scripts since batch 1). The commit after it adds this report and the plan moves, nothing else.
- Clean rebuild (`cmake --build build --target clean`, then `all` plus `riftbound_tests riftbound
  riftbound_clone_equiv_test riftbound_openspiel_tests deck_fuzz`), lane A, `-j3`: **0 warnings in engine, card and
  test code**. The log has 8 warnings, all in OpenSpiel's own sources (`games/hive`, `kriegspiel`, `bridge/double_dummy_solver`,
  `gin_rummy_test`), compiled because I built the `all` target.
- **`riftbound_tests`: 3,249 passed, 0 failed** (1 disabled, as on master). `riftbound_openspiel_tests`: 27 passed.
  `riftbound_clone_equiv_test`: 10/10.
- Checked build (`-DRIFTBOUND_CHECK_BOARD_INDEX=ON`, `build-check`, 0 warnings): suite 3,249 passed.
- Smoke (fiora vs yi_rebuild, 200 games, seed 20261010, 3 threads): rc 0, **P1 117 / P2 83 / 0 draws / 17 turns / 278
  decisions — identical to batch 1.** See section 4.
- MCTS batch, 4 games, seed 7: rc 0 without the fault (4-0); rc 1 with `RIFTBOUND_STEP_FAULT_AT=40` ("a game failed:
  game 1 [seed=7]: RIFTBOUND_STEP_FAULT_AT: injected engine fault at decision 40", 0 draws).
- ISMCTS, ten runs each: `ismcts:sims=50 --agent2 random --games 2 --threads 1 --seed 20261010`: rc 0 ten times, the same
  two games each time (8-2 in 325 decisions, 8-1 in 224 — the same as batch 1). `ismcts:sims=20 --games 8 --seed 1`:
  rc 0 ten times, 4-4 each time. No "did not reach the snapshotted decision".
- Pairing run, random vs random, 7 games for every ordered pairing of the **39** lists in `decks/` (with `daisy.txt`):
  1,521 pairings, **10,647 games on each build, 0 failures, 0 games without a winner, 0 flagged** (batch 1: checked
  build 3 engine errors, all F3). The two builds played the same games (P1 5,356 / P2 5,291 on both).
- Fuzz: section 6. **No new failure group; F3, F4 and F9 are gone** (0 in 300,000 optimised and 60,000 checked games).
- **Daisy! 754 (A2)**: resolved 37 times in 35 games of the pairing run (in 1,064 games where it was in a deck).

## 2. What was not done as asked, said plainly

- **The fixes are the previous agent's** (stopped 2026-10-11). I did not rewrite them. I read every source and test diff
  of the branch, checked the executor guards each touched card relies on, ran everything, and finished the gate. I did
  not re-derive every verdict from the rule book; the verdicts and rule citations are in the code comments and
  `plans/completed/10-testing-phase.md`, and I found none I disagree with.
- **The by-design test updates were already committed** in `d4b77e8`; the merged tree passed the suite unchanged when I
  rebuilt it. Nothing was left to update. They are listed in section 5.
- **Red first.** The previous agent's red run (46 tests on the pre-fix code): 34 failed; the 12 that passed are positive
  halves ("from hand the location is chosen", "next turn is a first time again", "its controller's own banish
  empowers it", ...) and one pin. Seven tests were written after that run and had never been shown red. I showed them
  today by reverting only their fixes in the worktree (`trigger_manager.cpp`, Ava Achiever, Dazzling Aurora, Blastcone
  Fae, Kennen, Keeper of Balance, and the three facedown-stamp lines of `Card::forgetPlayPayments`), building and
  running them, then restoring from git and rebuilding: **6 of 7 failed**; the seventh,
  `D55_LeBlanc_TheTemporaryKillDoesNotHappenAtHerBattlefield`, is a pin of batch 1's half of D55, which the revert does
  not touch.
- **Only the head was built and tested**, not each commit.
- **The checked build was not cleaned** before the gate: it was rebuilt incrementally in its own directory (0 warnings);
  only the optimised tree had the clean rebuild.
- **Smoke at 3 threads**, not 6 (lane limit). The outcome depends on the seed, not on the thread count.
- **D55's cited game** (`yasuao_sb` vs `leblanc_sb`, seed 20262378) replays to a normal end but diverges long before the
  cited decision (LeBlanc is played at D:274, the game ends in turn 19), so it shows nothing; D55 rests on its tests.
- **Throughput** was not measured on its own; the fuzz runs show the same rate as batch 1 (about 500 games/s per thread).

## 3. Verdicts

All 23 are defects (A1, A2 are tooling) and all are closed. Per item, outcome and test names:
`plans/completed/10-testing-phase.md`, section "Batch 4a". In short:

| item | verdict | fix |
|---|---|---|
| V1, D12 | defect | `DeckValidator::checkCopyLimits`: one card of a [Unique] name (CR 825.3.a) across main, champion and sideboard |
| D18 | defect | Astral Heron's erratum: "the next card you play this turn" (`this_turn_only`) |
| D20 | defect | Falling Star: both instructions, then `killLethallyDamagedTogether` |
| D45 | defect | Bellows Breath chooses its units in its own finalize pass, so each [Repeat] execution chooses its own (CR 820.2.a) |
| D47 | defect | `playIgnoringCost` / `playPayingCost` refuse a location `unitMayBePlayedTo` rules out; Temporal Breach's unit stays banished (Vendetta FAQ) |
| D52, F9 | defect | Star Spring records the play (unit + `playOrdinalThisTurn`); no return before its marker |
| D55 | defect | second half: only the [Temporary] ability is suppressed at LeBlanc's battlefield; first half (batch 1) pinned |
| D56 | defect | `isBeginningPhase` (CR 315.2): Beginning and Scoring Steps only |
| D59 | defect | the granted-Accelerate mark is a per-play flag |
| D62 | defect | Thrill of the Hunt, Dazzling Aurora, Ava Achiever: no unit to a battlefield where units can't be played |
| D65 | defect | Mel's extra -1 [M] only to a unit the resolving item chose |
| D66 | defect | Herald of the Arcane chooses the Recruit's location at activation |
| D69, D70 | defect | a card played from facedown is bound to its battlefield (CR 811.1.d.2-5): `hiddenBinding`, `atHiddenBattlefield`, `tokenPlayDests(ctx)`; facedown stamps are per-play flags |
| D73 | defect | Fortified Position's Shield lasts that combat (a continuous effect), not the turn |
| D75 | defect | "When you banish": the banish names who performed it (CR 412.1, 412.4) |
| D81 | defect | Minefield burns (CR 441), with Burn Out |
| F3 | defect | battlefield replacement and its reversal go through `leftBoard`; the executor writes no board state to an object not in play (Vi, Hotheaded; **Pendulum Blade**); Fading Memories checks its target |
| F4 | defect | Emperor's Divide offers only units it can move, each once |
| A1 | defect (tooling) | `Intent::declines` marks the "No" of a yes/no; `random-player` reads it instead of the values 0 / 1 |
| A2 | done | `decks/daisy.txt` |

### The brief's specific checks

- **Pendulum Blade (F3)**: fixed through the guard in `EffectExecutor::giveTemporaryMight` (nothing is given to an
  object not in play); test `F3_PendulumBlade_NoMightForABearerThatHasLeft` kills the bearer while the ability waits on
  the chain. 0 F3 in 60,000 checked fuzz games and 10,647 checked pairing games (batch 1: 46 and 3).
- **`giveTemporaryMight` and other writers**: now guarded, with `giveTemporaryKeyword`, `exhaustObject`, `takeControl`,
  `takeControlUntilSourceLeaves` (beside `dealDamage`, `buffUnit`, `stunUnitBy`, `readyObject`, `empower`, `spendBuff`,
  `moveToBase`, which already were). Every card the branch touches writes board state only through those, except
  Fortified Position (checks `location` before writing its mark) and Fading Memories (checks the target is still
  legal before setting [Temporary]); Star Spring writes its own `card_counters`, which are kept off the board by design.
  `healObject` has no guard (section 9).
- **D55 first half**: pinned by `Batch4aTest.D55_LeBlanc_TheTemporaryKillDoesNotHappenAtHerBattlefield` (a [Temporary]
  unit at her battlefield survives its controller's Beginning Step; one elsewhere dies and its [Deathknell] draws). The
  batch-4a part of B9 is marked done in the remaining plan.

## 4. Smoke and decision flow

117 / 83 / 0 draws / 278 decisions, identical to batch 1. The two lists contain two cards this branch changed:
Dazzling Aurora (its location list now comes from `playLocations`, which differs only with Rockfall Path or a
base-only player — neither is in these lists) and Star Spring (differs only when the same card is played there twice
in a turn, or two units in one resolution). The new `Intent::declines` is not seen by the random agent. Same aggregate
to the decision, so I take it the 200 games are the same games; I did not diff the traces.

Fuzz, mean decisions per game: random 266.0 (batch 1: 266), random-player 258.9 (261), forced play 291.9 (291). Forced
play moves a little because A1 now lets it take option 0 of a two-mode, X-of-0-or-1 or "cost less" prompt.

## 5. Tests changed by design

Each is a pin of the old behaviour; old -> new, with the rule. No test was deleted without a replacement and no
assertion was weakened to pass.

- `C0Test.FortifiedPosition_OffersEveryUnit`: old — the Shield was a "this turn" keyword written to the unit
  (`keywords.has`), no combat needed. New — a combat is in progress and the Shield is a continuous effect
  (`hasKeyword`). Card text "this combat" (D73).
- `C3Test.BellowsBreath_TargetSetsAreUnitsAtOneLocation` -> `BellowsBreath_ChoosesUnitsAtOneLocation`: old — one Play
  intent per target set; new — one Play, the units are prompts, the second offers only units where the first is.
  CR 355, 820.2.a (D45).
- `VerifyCardsTest.BellowsBreath_FirstTargetMoved_TheControllerPicksWhichGroup...`, `..._UnitsThatMovedTogetherAreStillAGroup`,
  `..._FirstTargetGone_TheOthersStillTakeOne`: the three units are chosen through the prompts (scripted) instead of the
  intent's targets; prompt counts 1 -> 4, 1 -> 4, 0 -> 3. The damage assertions are unchanged.
- `VerifyCardsTest.BellowsBreath_MayBePlayedChoosingNoUnitWhileUnitsAreAvailable`: old — four plays (none, a, b, a+b);
  new — one play whose first prompt offers "No more units", a and b (CR 355.13).
- `VendettaB.Vendetta_Mel_EmpoweredExtraMinusMightAndUncounterable`: the resolving item now names the unit in
  `chosen_objects`; the old test gave the extra -1 to a unit nothing had chosen. "to a unit it chooses" (D65).
- `SuppressionTest.LeBlanc_SuppressesTemporaryTriggerHere` -> `LeBlanc_DoesNotSuppressATemporaryUnitsOtherTriggers`:
  old — no trigger of a [Temporary] unit there; new — its other triggered ability goes on the chain. "Your [Temporary]
  effects ... don't trigger" (D55).
- `SweepTriggersTest.OwnDeathTrigger_IsSuppressedForATemporaryUnitAtLeBlancsBattlefield` ->
  `..._OfATemporaryUnitAtLeBlancsBattlefieldStillTriggers`: XP unchanged -> +1 (its [Deathknell] is not a [Temporary]
  effect; D55).
- `RandomPlayerAgent.WhatIsAPass`, `.WithPassZeroItNeverPassesWhileAnotherOptionIsLegal`: a prompt of the bare values 0
  and 1 is no longer a no / yes; the yes/no cases mark the "No" with `declines` (A1).
- `PerfEngineSources.CardCodeWalksEveryObjectOnlyWhereItIsListed`: + Fortified Position (walks every object to drop
  its "this combat" memory from objects that left the board).
- `PerfEngineDeathBatch.EveryCardBatchSaysWhoDies`: helper count 21 -> 22 (Falling Star, D20).
- `StrongIds.EveryObjectIdReadFromAnIntegerSlotIsOnTheAuditedList`: + Bellows Breath (its chosen units in `resume_data`).

New tests: 53 in `tests/cards/test_batch_4a.cpp`, plus `DeckValidatorTest.UniqueCardOnlyOnce`.

## 6. Fuzz, final tree

`deck_fuzz --known docs/testing-known-failures.txt` (F3, F4, F9 removed from it), optimised `--decks 5000
--games-per-pair 20`, seeds 20261501-3; checked `--decks 2500 --games-per-pair 8`, seeds 20261601-3. Batch 1's numbers
(its report, section 6) in brackets.

| build | agents | games | failures | groups | no winner / long chains / hung |
|---|---|---|---|---|---|
| optimised | random | 100,000 | 0 [1] | — [F7 1] | 0 / 0 / 0 |
| optimised | random-player | 100,000 | 1 [6] | F6 1 [F5 2, F6 2, F7 1, F9 1] | 0 / 0 / 0 |
| optimised | random-player:pass=0 | 100,000 | 2 [40] | F6 2 [F4 37, F7 3] | 0 / **1** / 0 |
| checked | random | 20,000 | 0 [6] | — [F3 6] | 0 / 0 / 0 |
| checked | random-player | 20,000 | 0 [12] | — [F3 12] | 0 / 0 / 0 |
| checked | random-player:pass=0 | 20,000 | 0 [28] | — [F3 21, F4 6, F6 1] | 0 / 0 / 0 |
| optimised | pairing, random, 39 lists | 10,647 | 0 | — | 0 / 0 / 0 |
| checked | pairing, random, 39 lists | 10,647 | 0 [3, F3] | — | 0 / 0 / 0 |

- **No new failure group.** F3 (66 in batch 1's tables), F4 (43) and F9 (1) do not occur. F5 and F7 (batch 3) did not
  occur this time either; they are rare (3-15 per million) and are not fixed here.
- **The long chain** (forced play, Matriarch of War / Ambessa vs Might of Demacia / Garen, game seed 1557994370, 1,062
  decisions, a winner at 8): Imposing Challenger and Irresistible Faefolk moving each other — F8's loop. Batch 1's bound
  ended it as designed (`MAY_NOT_OFFERED: Irresistible Faefolk ... accepted 100 times in this chain — Tournament Rules
  505.7`), and the game played on to a winner. 200 items is that bound's size (2 x 100), so a forced-play game that
  meets the pair is flagged; batch 1's run had none by chance (9 per million in round 1). Not a defect of this branch.
- Daisy! in the random-deck fuzz: resolved once in 300,000 games (it needs four tags among your units; random decks
  rarely have them). The list is what exercises it (section 1).

## 7. Trainer notes

- `kStateFeatureDim`, `kCardVocabSize`, the feature layout and the action-id scheme are unchanged. `Intent::declines`
  is a new field, excluded from `operator==`, the action id and the history token.
- **Prompts that changed:**
  - Bellows Breath: one Play intent instead of one per set of units; then a prompt per unit (value 0 "No more units"
    first, then units; after the first only units at its location), asked again for each [Repeat] execution.
  - Herald of the Arcane: a location prompt for the Recruit at activation (base or a controlled battlefield), when
    there is more than one place.
  - Sprite Call, Desert's Call, Zed, From the Shadows, Illaoi, Prophet of the Great Kraken played from facedown: no
    location prompt (one place); no token when units can't be played there.
  - Windsinger, Blastcone Fae, Kennen, Keeper of Balance (play effect) from facedown: only units at that battlefield.
  - Emperor's Divide: only units it can move to base, each offered once.
  - Thrill of the Hunt, Dazzling Aurora: Rockfall Path (and any battlefield units can't be played to) is not offered.
    Ava Achiever: a hidden unit is not offered at Rockfall Path.
- **Behaviour:** Fortified Position's Shield ends with the combat; Astral Heron's discount ends with the turn (its
  `ability_text` string changed, erratum); Falling Star on one unit twice is one death; Mel's -1 only to chosen
  units; Master of Shadows only on its controller's banishes; Minefield can Burn Out; "your Beginning Phase" no longer
  includes the Awaken Phase; a unit an effect would play where it can't be played is not played.
- **Deck validation** rejects a second copy of a [Unique] card.
- `random-player` with pass=0 now takes option 0 of two-mode / X / "cost less" prompts.

## 8. What another stream needs

- **Batch 2**: D73 is listed under batch 2 as well; it is closed here (`a042b5f`). Batch 2's working tree touches
  `game_engine.cpp`, which this branch changes in five small places (the `declines` marks on four engine prompts and
  `isBeginningPhase` in `killUnit`); expect trivial conflicts at most.
- **Batch 4b, probably closed as side effects** (boxes left for 4b, not verified against their cited games):
  - **G23** (a Brush that replaced Vilemaw's Lair keeps "can't move from here to base"): `replaceBattlefieldWithToken` /
    `restoreReplacedBattlefield` now re-read the slot's rules from the card in it (`applyBattlefieldCardRules`);
    `F3_GreenFather_ABrushThatReplacedRockfallPathTakesUnits` shows it for "can't be played here".
  - **G21** (Kennen, Keeper of Balance from facedown stuns a unit in the enemy base): his play effect is bound to his
    battlefield (`D70_KennenKeeperOfBalance_HisPlayEffectIsBoundHisAttackIsNot`).
- **Batch 3, B2**: `Card::forgetPlayPayments` now drops the facedown stamps as every play begins, which removes the
  stale-mark case B2 describes for every card; the "one helper for the three from-face-down cards" half is still open.
- **G27** (batch 3) lists Astral Heron's discount spent on the opponent's turn as a D18 sighting: covered by
  `D18_AstralHeron_TheUnusedDiscountEndsWithTheTurn`.
- No `shared:` commits.

## 9. New defects found (not fixed)

- **Fortified Position's Shield ends if the battlefield card leaves mid-combat.** The Shield is applied by the card's
  `applyPassiveAura`, which runs only while Fortified Position is in its slot; a Green Father replacement during that
  combat would end a "this combat" effect that a resolved ability created and that should outlast its source.
  CODE-ONLY (needs a replacement during a combat at that battlefield).
- **`EffectExecutor::healObject` has no in-play guard**: it zeroes damage (harmless off the board) but emits `"healed"`
  for an object that has left. CODE-ONLY.
