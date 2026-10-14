# batch-1b — final report: triggers from 19 read ISMCTS games; combat flow (CR 323.11-323.12, 468.5-468.7)

## 1. Result

- Branch `fix/batch-1b`, from master `d125a2c`; master merged once (`52428e2`, plans only: master had not
  moved past `bada264` — **batch 4b was not on master** when the gate ran; see section 10). Gate run at
  `044d099`; the commit after it adds plans and this report only.
- Clean rebuild (`--target clean`, then `riftbound_tests riftbound riftbound_openspiel_tests
  riftbound_clone_equiv_test deck_fuzz`), lane B, `-j3`: **0 warnings** (386 steps). Checked build
  (`-DRIFTBOUND_CHECK_BOARD_INDEX=ON`, `build-checked`, first configured in this batch, full compile): 0
  warnings.
- **`riftbound_tests`: 3,336 passed, 0 failed** (master: 3,277; +59 new, see section 7).
  `riftbound_openspiel_tests`: 27 passed. `riftbound_clone_equiv_test`: 10/10. Checked build suite: 3,336
  passed.
- Smoke (fiora vs yi_rebuild, 200 games, seed 20261010, 3 threads): rc 0, **P1 121 / P2 79 / 0 draws / 17
  turns / 272 decisions** (master 122 / 78 / 0 / 17 / 271; section 4 explains the move).
- MCTS batch, 4 games, seed 7: rc 0 without the fault; rc 1 with `RIFTBOUND_STEP_FAULT_AT=40` ("a game failed
  ... injected engine fault at decision 40", 0 draws).
- ISMCTS, ten runs each (`--agent1 ismcts:sims=50 --agent2 random --games 2 --threads 1 --seed 20261010` and
  `--agent1 ismcts:sims=20 --agent2 random --games 8 --seed 1`, fiora vs yi_rebuild): rc 0 twenty times, the
  same games every time and **the same games as batch 2's report** (8-3 in 369 decisions and 8-2 in 246;
  3-5), no "snapshotted decision".
- Pairing run (39 lists, 7 games per ordered pairing, 10,647 games, seed 20262201): optimised and checked
  **0 failures, 0 games without a winner**.
- Fuzz, fresh seeds: **no new failure group** after one fix (section 6). The first checked run had one new
  group; it was a defect of batch 2's Cleanup shortcut that this branch's games reached, fixed (J1) and the
  whole gate run again.
- Throughput unchanged: about 659 games/s before, 658 after (section 5).

What was not done as asked, said plainly:

- **Batch 4b is not merged into this branch.** It had not reached master when my gate ran (its gate was
  running). A trial merge (`git merge-tree`) shows three textual conflicts (CLAUDE.md, the judge questions,
  `card.h`) and semantic contact points listed in section 10. Whoever merges second must re-run the gate.
- **Commits per mechanism**: G9, G10, G11+G12 (one mechanism), G13, I2, I1 are a commit each, staged hunk by
  hunk from one working tree. Two intermediate states were built (`231cf6e` after G13, `6bbe327` after I2) for
  the smoke attribution; the others were not compiled alone. While staging I misplaced one hunk with zero
  context (`trigger_manager.cpp`, the G9 commit): caught on review and rebuilt from master's file before the
  next commit, so no commit holds it.
- **The new test file was committed after the code** (`fab6ca5`), not before it; the tests were written and
  run red before the fixes (the counts below are from a build of master `d125a2c` with the final test file).
- The first gate attempt built `all` (OpenSpiel's own test programs too); I stopped it and built the five
  targets the gate uses. The warning count is from a clean rebuild of those (ccache replays a cached
  compile's warnings).

## 2. Verdicts

| item | verdict | rule | mechanism | red shown on |
|---|---|---|---|---|
| G5 | FIXED by batch 1 | 319.5, 323.4 | a Cleanup after every chain step kills Forgefire Cape's victim before the first Focus decision | pre-batch-1 `59b443c` |
| G6 | FIXED by batch 1 | 323.5 | the Cleanup after the Deathknell's resolution is in an Open State and drops control | pre-batch-1 `59b443c` |
| G9 | FIXED, 20 cards | 383.2.a.1, 383.1, 727.1.c.1, 812.1.c, 824.1.c | the condition is the trigger condition, not re-tested at resolution; new `Card::triggerConditionOn(state, TriggerEvent)` with the subject's printed `CardDef` | master |
| G10 | FIXED + QUESTION (judge question 9, pinned) | 190.6.a, 190.6.b, 190.6.d | battlefield phase abilities are the battlefield controller's (the turn player's while uncontrolled); Dusk Rose Lab's "your" is its trigger condition; Obelisk of Power / The Arena's Greatest act for the turn player | master |
| G11 | FIXED | 420.1.a, 420.3 | the Champion Zone play's source is the Champion Zone; effect plays announce "not from hand" unless from a hand | master |
| G12 | FIXED | 420.4.a, 420.4.a.1 | a spell's "not from hand" is announced as it resolves (`Card::kPlayedNotFromHand`, `onSpellResolved`) | master |
| G13 | FIXED | 317.1.a, 391.2, 391.5.a | "Disempower / Empower it at end of turn": a delayed AtEndOfTurn ability about the unit (`DelayedAbility::subject`, `::ability_index`); `empower_flip_eot` removed | master |
| G26 | QUESTION (judge question 10, pinned) | 420.4.a vs "the new object did not exist when the spell was played" | none; note at Ravenbloom Student | — (pin) |
| I1 | FIXED | 323.11, 323.12 | one staged showdown or combat per pass, showdowns first; the turn player chooses among two or more — **a new prompt** | master |
| I2 | FIXED | 468.3-468.7 | result (`CombatResultEvent`) -> chain -> establish control (hidden cards removed, 468.5.c) -> chain with the combat still on -> combat ends (designations, Neutral State, `CombatEndedEvent`) -> chain | master |

Red: `tests/test_batch1b_triggers.cpp` (58 tests) built against master `d125a2c`: **41 failed** as the findings
describe; the 17 that passed are G5 and G6 (closed by batch 1), G26's pin, the G13 pin (a unit that left the
board), and positive controls (Frozen Fortress uncontrolled; Dusk Rose Lab on its controller's turn; hand
plays of G11; Blitzcrank played to a battlefield; one staged combat; a No Result combat; the conquer readers
Tryndamere, Yeti Brawler, Trapping Grounds, Piltover Enforcer; Hextech Gauntlets with 3 excess; Blighted
Battleaxe without a conquer). G5 and G6 built against the pre-batch-1 master `59b443c` (extracted to scratch,
deps from this tree): **both fail** as the findings describe (Tideturner on the board at Focus; the battlefield
still controlled at the next decision).

### G9: the cards

The finding's eight: Curator of the Sands, Lady of Luminosity, Blitzcrank Impassive, Vanguard Captain (Gutter
Palace, Patched Porobot, Poppy Paragon and Pyke Dockside Butcher were already batch 1's). Every other [Legion]
"When you play me": Trifarian Gloryseeker, Dangerous Duo, Scrapyard Champion, Darius Executioner 243 / 547.
Found by a scan of every card without a trigger condition whose resolution returns on a condition: Lux
Illuminated and Yordle Explorer (printed cost of the played card), Wuju Apprentice ([Level 6]), Nasus
Ascended, Covert Informant, Baccai Witherclaw, Noxian Emissary ([Empowered]), Obelisk of Power and The
Arena's Greatest ("first Beginning Phase", with G10), and while checking I2's conquer readers two Equipment:
Hextech Gauntlets ("if you assigned 3 or more excess damage") and Blighted Battleaxe ("if I didn't conquer
this turn"). One behaviour change beyond "no empty item": Curator of the Sands judges a unit's or gear's
**printed** Energy cost, so a [7]+ unit played by an effect or from face down now triggers it (it read the
Energy spent, which was the printed cost for a hand play and 0 otherwise). Read and left as they are (the
condition follows the effect's verb, an effect condition judged at resolution): Loose Cannon, Loyal Poro,
Lonely Poro, Dune Drake, Hallowed Tomb.

### G10 and the judge question

Frozen Fortress's ability belongs to the battlefield's controller (190.6.a); question 9 (a) asks whether "each
player's Beginning Phase" indicates another player. Dusk Rose Lab: triggers only in its controller's Beginning
Phase; uncontrolled it does not trigger (190.6.d, "'you' refers to no one"); question 9 (b) gives the 190.6.b
reading. `fireBattlefieldTriggers` now names no player for AtStartOfMain and AtEndOfTurn too; no registered
battlefield has those today.

### I1: what it adds to the decision flow

A `MakeChoice` asked of the **turn player**, only when two or more battlefields have a staged showdown (or,
with none, two or more a staged combat) as `advanceMainPhase` settles: one option per battlefield,
`chosen_value` = its index, `chosen_objects` = its battlefield card, label "Showdown begins at — X" / "Combat
begins at — X". One per pass, so three staged combats ask twice. Showdowns go first (step 9 before step 10),
which also changes the order when a combat is staged at a lower battlefield than a showdown. In the smoke it
appears in 2 of 200 games (section 4).

### I2: the cards that read the combat at a conquer

`last_combat_*` / excess damage: Tryndamere Barbarian, Sivir Ambitious, Yeti Brawler, Trapping Grounds,
Piltover Enforcer, Hextech Gauntlets — values recorded at the combat's start and kept after it: unchanged,
each tested (Sivir and Yeti by batch 2's B4 tests). Designation readers (Fiora Peerless, Fortified Position,
Forbidding Waste, Wuju Bladesman, Wielder of Water, Crimson Pigeons, Vex Cheerless, Draven Audacious, Akali
Silent, Ambessa, Shadow, Overzealous Fan) and spells that choose an attacker (Thwonk, Existential Dread, Cannon
Barrage, Decree of Focus, Sudden Storm): during the conquer abilities the designations, [Assault] / [Shield] and
"this combat" effects now still apply, and a [Reaction] in that chain window can choose an attacker — as the
rules put the end of the combat at 468.7. "When you win a combat" moved from the end of the combat to the
result (468.3), before control is established; "When a combat that I was in ends" after it.

## 3. G13 in detail

`DelayedAbility::subject` names the unit; the ability fires AtEndOfTurn in the Ending Step with the unit as
its chain item's `triggering_subject`; `DelayedAbility::ability_index` (Sanction: 0 "disempower it", 1
"empower it") becomes the item's `ability_index`. `EffectExecutor::leftBoard` erases the delayed abilities
about the object that left (CR 391.5.a): the same card played again is a new object (124.1) and is not
touched. The end-of-turn ability is made whether or not the first half changed anything.

## 4. Smoke and decision flow

| state | P1 / P2 / draws | decisions per game |
|---|---|---|
| master `d125a2c` | 122 / 78 / 0 | 271 |
| + G9, G10, G11/G12, G13 (`231cf6e`) | 122 / 78 / 0 | 271 |
| + I2 (`6bbe327`) | 122 / 78 / 0 | 271 |
| + I1 (`4843a0e`) and final (`044d099`) | 121 / 79 / 0 | 272 |

Only I1 moves it: game by game, two of 200 games differ (seeds 20261029 and 20261110), and each has one
"where the next combat begins" prompt (two combats staged after Rift Herald's turn in 20261029); the random
agent's choice of order changes the rest of the game. G9-G13 touch no card of these lists; I2 changes when
things happen, not what is decided, in these games. Fuzz decisions are in the JSON results.

## 5. Throughput

1,000 games, 1 thread, fiora vs yi_rebuild, alternating (batch 2's binary of master as "before"):

| | run 1 | run 2 | run 3 |
|---|---|---|---|
| master | 661.6 games/s | 661.0 | 656.1 |
| final `044d099` | 663.2 | 651.0 | 658.3 |

No change. Engine code that writes state without an event or log line: none added. Every write is followed
by an announcement (the combat's end, the begin prompt's decision line, the delayed ability's chain item, the
hidden-card removal's log line); the checked build compares every skipped Cleanup and found one hole that was
not mine (J1).

## 6. Fuzz, final tree

`deck_fuzz --decks 5000 --games-per-pair 20 --threads 3 --known docs/testing-known-failures.txt`, seeds
20262001-3 (checked: `--decks 2500 --games-per-pair 8`, seeds 20262101-3).

| build | agents | games | failures | groups | without a winner / long chains / hung |
|---|---|---|---|---|---|
| optimised | random | 100,000 | 0 | — | 0 / 0 / 0 |
| optimised | random-player | 100,000 | 1 | F6 1 (Heedless Resurrection) | 0 / 0 / 0 |
| optimised | random-player:pass=0 | 100,000 | 3 | F5 2 (Kennen, Keeper of Balance; Sinister Poro), F7 1 (Strike Down) | 0 / 1 / 0 |
| checked | random | 20,000 | 0 | — | 0 / 0 / 0 |
| checked | random-player | 20,000 | 0 | — | 0 / 0 / 0 |
| checked | random-player:pass=0 | 20,000 | 0 (1 before J1) | — | 0 / 0 / 0 |
| optimised | pairing, 39 lists x7 | 10,647 | 0 | — | 0 / 0 / 0 |
| checked | pairing, 39 lists x7 | 10,647 | 0 | — | 0 / 0 / 0 |

Against batch 2's table: F5 and F6 as before; **F7 (Strike Down) was seen again** in one pass=0 game — batch 2
listed it as gone, but it is in the known list and is not new. **New: none** after J1. The long chain (200
items) is Imposing Challenger and Irresistible Faefolk answering each other (102 triggers each): batch 1's F8
loop, stopped by the Tournament Rules 505 cap; the game ended normally. The first checked pass=0 run had one
new group, "a Cleanup that would not have been run ... changed the state" (Heart of the Tempest vs Fire Below
the Mountain, Azir Ascendant moving Experimental Hexplate next to Rumble, Scrapper): J1, section 11.

## 7. Tests changed by design

Old -> new, with the rule:
- `VendettaA.Vendetta_NasusAscended_ScoresOnConquerOnlyWhileEmpowered`, `VendettaB.Vendetta_CovertInformant_DrawsOnMoveOnlyWhileEmpowered`,
  `VendettaB.Vendetta_BaccaiWitherclaw_DeathknellOnlyIfEmpowered`, `.Vendetta_BaccaiWitherclaw_DeathknellUsesLastKnownEmpowered`,
  `VendettaCTest.Vendetta_NoxianEmissary_DeathknellOnlyWhileEmpowered`, `.Vendetta_NoxianEmissary_DisempoweredBeforeDyingGetsNothing`,
  `RulesCards2Test.RulesCards2_CR383_NoxianEmissary_NotEmpoweredAtDeath_NoTokens`, `AuditFix7Test.WujuApprentice_BelowLevel6NoDraw`,
  `AuditFix7Test.HextechGauntlets_ConquerRiderDraws`: called `onTrigger` with the condition false and expected
  no effect -> ask the trigger condition (`CardTestFixture::triggersNow`, new) and expect false (CR 383.2.a.1,
  727.1.c.1; G9).
- `AuditFix7Test.ArenasGreatest_OnlyFiresOncePerPlayer`, `.ArenasGreatest_PerPlayerGate`,
  `RulesCards2Test.RulesCards2_CR194_ArenasGreatest_PointGoesThroughScorePoints`: a per-player counter in the
  card gated a second firing; the point went to the ability's controller -> "first" is the trigger condition
  on `turns_taken`, the point goes to the turn player ("that player"; CR 190.6.a; G10).
- `VendettaCTest.Vendetta_TornadoWarrior_FromFaceDownEmpowersUntilEndOfTurn`,
  `VendettaD.Vendetta_Sanction_EmpowerThenDisempowerAtEndOfTurn`, `.Vendetta_Sanction_DisempowerThenEmpowerAtEndOfTurn`:
  asserted `empower_flip_eot` -> assert the delayed ability (subject, direction) (CR 317.1.a, 391.2; G13).

None weakened. Added: `tests/test_batch1b_triggers.cpp` (58 tests),
`DeckFuzz.B1_ATagGivenInTheAuraPassIsAChangeTheCleanupSees`. The test fixture gained `triggersNow` and the
begin-prompt routing (`EnginePathAgent::begin_prompts` / `begin_pick`).

## 8. Trainer notes

- **New prompt** (I1): "where the next showdown / combat begins", asked of the turn player with two or more
  candidates. `MakeChoice`, object = the battlefield card (the prompt's object slot), value = the
  battlefield's index. No new action ids, no new slot kind, no observation change.
- **Order of events**: staged showdowns begin before staged combats; "When you win a combat" resolves before
  control is established; conquer abilities resolve with the combat still on (`ns_state == Showdown`,
  designations present, [Assault] / [Shield] counted) and a chain-priority window there is part of the combat;
  "When a combat that I was in ends" after the conquer.
- **Fewer windows**: the 20 G9 cards no longer put an empty item on the chain (no priority window, no
  Closed State).
- **Ownership of battlefield abilities**: Frozen Fortress / Obelisk of Power / The Arena's Greatest abilities
  on the other player's turn are the battlefield controller's (who has priority first changes); Dusk Rose Lab
  no longer triggers outside its controller's Beginning Phase.
- **Heart of the Tempest** is empowered by Champion Zone plays and effect plays, and by a Flow spell only once
  it resolves; "from hand" cost modifiers no longer apply to a Champion Zone play.
- `kStateFeatureDim`, `kCardVocabSize`, the layout and the action-id scheme are unchanged.

## 9. Items closed

G5, G6, G9, G10, G11, G12, G13, G26, I1, I2 (and J1, found here). Outcomes and test names in
`plans/completed/10-testing-phase.md`. Not closed: none of mine.

## 10. Needs another stream / shared files

- **Batch 4b merge** (it was not on master): textual conflicts in CLAUDE.md, `docs/judge-questions-2026-10.md`
  (both branches number questions 9 and 10: the second to merge renumbers — mine are J5 -> 9 and J7 -> 10,
  referenced in `0765_dusk_rose_lab.cpp`, `0103_ravenbloom_student.cpp`, `tests/test_batch1b_triggers.cpp`,
  `plans/completed`, CLAUDE.md, this report) and `card.h` (adjacent hunks: my `TriggerEvent` /
  `kPlayedNotFromHand`, its `asYouPlayChoices` / Accelerate keys / `firstTimeEachTurn`). Semantic contact
  points to check after the merge: 4b's B6 stamp in `fireTrigger` sits after the condition call I renamed to
  `triggerConditionOn`; 4b's H1 "this combat" effect must end at 468.7 (`combat_in_progress` false /
  `CombatEndedEvent`), which is now after the conquer abilities; 4b's Accelerate on effect plays edits
  `playIgnoringCost`, where I compute `from_hand`; 4b's `withEngine` turn-player check (B8) against my tests
  (they pass a turn player only where the state's agrees).
- Shared files touched: `card.h` (`TriggerEvent`, `triggerConditionOn`, `kPlayedNotFromHand`),
  `card_helpers.h` (`legionActive`, `atEndOfTurnAbout`), `core/events.{h,cpp}` (`CombatResultEvent`),
  `core/game_state.h` (`DelayedAbility::subject`, `::ability_index`), `core/game_object.h` / `board_state.h`
  (`empower_flip_eot` removed), `engine/trigger_manager.{h,cpp}`, `engine/game_engine.{h,cpp}`
  (advanceMainPhase, chooseNextStaged, combatResolutionStep, removeForeignHiddenCards, executePlayCard,
  executePlaySpell, executePlayFromHidden, doExpirationBody, the Cleanup checksum), `engine/effect_executor.cpp`
  (playIgnoringCost, leftBoard), the test fixture, `tests/test_deck_fuzz.cpp`.
- G9 cards that are also batch 4b's cards: none (4b changed Immortal Phoenix, Wraith of Echoes, Lucian,
  Draven, Jayce, Lonely Poro, Shard of Undoing, Frigid Jewel, The List, Defender of Tomorrow, Green Father,
  Fortified Position, Time Warp, Facebreaker, Switcheroo, Ivern; I read Lonely Poro and left it).

## 11. New defects found

- **J1 (fixed, it blocked the gate)**: the Cleanup's change check did not read tags. Experimental Hexplate
  gives its bearer the Mech tag in the aura pass; Rumble, Scrapper's "Your Mechs have +1 [M]" hook ran first;
  the Cleanup saw no change after one pass and batch 2's shortcut would skip the next one, so in the optimised
  build the bearer lacked +1 [M] until something else was announced. The check now mixes the interned tag
  list. Batch 1b's games reached this board (deck A's Heart of the Tempest plays differently); nothing of the
  batch causes it. Test: `DeckFuzz.B1_ATagGivenInTheAuraPassIsAChangeTheCleanupSees`.
- **J2 (open)**: CR 323.11 / 323.12 apply in every Neutral Open State Cleanup; only `advanceMainPhase` begins
  staged showdowns and combats, so one staged outside the Main Phase waits for it.
- **J3 (open)**: CR 468.3.d.1 decides "stage another combat" from who is present at the result;
  `combatResolutionStep` decides it from who is present after the 468.4 chain.
- `CLAUDE.md` said a Champion Zone play does not get "from hand" cost modifiers; it did (its intent is the hand
  play's). Fixed with G11.
