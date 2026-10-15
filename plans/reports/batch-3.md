# batch-3 — final report: targets, finalization and what goes on the chain; [Add]; Equip and Quick-Draw; effect plays; chosen objects; combat flow

## 1. Result

- Branch `fix/batch-3`, from master `51f842a`. Master (batch 5 / T3) merged once, at `28d987c`. The gate ran on
  `1095f92`. The commits after it change only `plans/` and add this report.
- **Items: 39 closed.** 34 FIXED; 2 NOT A DEFECT, verified and pinned (D79; J3, whose outcome cannot change, with its code made literal);
  1 QUESTION written (judge question 13, for J3's 468.5 case); 3 DONE as verify-and-pin parts (B9, G27, H3, batch-3 parts).
  Five gate findings were fixed and tested under their items (section 3).
- Commits by mechanism: D (`5034619`), E + C for spells (`14ceb59`), C for abilities (`18d9228`), B (`7e30c40`),
  A (`db3c7b6`), F and effect plays (`3d112fb`), G (`677dddb`), H (`dd9df7f`). The gate findings are in `962297f`,
  `b80537d`, `dc5f6ca`, `ba31620` and `1095f92`. CLAUDE.md is in `ffdcaa1` and `606b4f3`, plans in `0346ee3` and
  `f149b48`.
- Clean rebuild at `606b4f3`: `--target clean`, then `all riftbound_tests riftbound riftbound_clone_equiv_test
  riftbound_openspiel_tests deck_fuzz`, lane A, `-j3`. **0 warnings in our code.** The log has 8 warnings, all in
  OpenSpiel's own sources. The incremental builds to `1095f92` (both build dirs) also had 0 warnings in our code.
- **`riftbound_tests`: 3,474 passed, 0 failed** (1 disabled, as on master; base 3,392). `riftbound_openspiel_tests`: 33 passed.
  `riftbound_clone_equiv_test`: 10/10.
- Checked build (`-DRIFTBOUND_CHECK_BOARD_INDEX=ON`, `build-checked`, 0 warnings in our code): **3,474 passed**.
- Smoke (fiora vs yi_rebuild, 200 games, seed 20261010, 3 threads): rc 0, **P1 128 / P2 72 / 0 draws / 17 turns / 288
  decisions**. Base `51f842a`, built from the same sources: 135 / 65 / 0 / 17 / 282, the same as batch 4b. Each commit's
  share of the change is in section 4.
- MCTS batch, 4 games, seed 7: rc 0 without the fault (3-1). With `RIFTBOUND_STEP_FAULT_AT=40`: rc 1, "a game failed:
  game 1 [seed=7]: RIFTBOUND_STEP_FAULT_AT: injected engine fault at decision 40", 0 draws.
- ISMCTS, ten runs of each command, decks yi_rebuild vs fiora:
  - `ismcts:sims=50 --agent2 random --games 2 --threads 1 --seed 20261010`: rc 0 ten times, the same two games every
    time. P2 won 4-8 in 279 decisions and 7-8 in 352. T3 had P2 4-8 in 321 and P1 8-1 in 143. The games change
    because the decision flow changed (section 8).
  - `ismcts:sims=20 --games 8 --seed 1` (3 threads): rc 0 ten times, 3-5 every time. The same eight games each time
    (one hash of the game lines in all ten). T3 had 4-4.
  - "did not reach the snapshotted decision" appears in no run.
- Pairing run, random vs random, 7 games for each ordered pairing of the 39 lists in `decks/` (seed 20261901):
  **10,647 games on each build.** 0 failures, 0 games without a winner, 0 flagged. Both builds played the same games:
  P1 5,276 / P2 5,371, 309.1 decisions per game. Batch 4b had 314.2.
- Fuzz: section 6. On the final tree there is **no failure of any kind** in 300,000 optimised and 60,000 checked
  games. F5, F6 and F7 are gone, and their lines are removed from `docs/testing-known-failures.txt`.
- Throughput: a game takes 10.3% longer than on base (section 5).

## 2. What was not done as asked, said plainly

- **The gate found five defects, and four of them came from this batch's own changes.** The first gate pass ran at
  `0346ee3`. Its checked suite and its fuzz found them:
  - Poro Snax and Scryer's Bloom left board state on a card in the trash. With an activation's choices now made
    before its cost, the two gears killed themselves in their finalize pass, and then the engine exhausted them.
  - Mesmerize was offered when its only choice carried a Deflect it could not pay.
  - Alpha Strike's split targets were offered without the spell's choice filter. This defect is older than the batch;
    the batch made it reachable.
  - A payment's [Add] choices could leave the payment short (Power Nexus with Butcher of the Sands).
  - Moonfall's "may" was offered on the raw list of units.
  - A pair's first target was judged without its own Deflect (Switcheroo).

  Each was shown red and fixed. Then everything in section 1 was run again on `1095f92`. The numbers in this report
  are from that second pass. The `0346ee3` pass is not reused.
- **Shell rules: two slips, both in measurements.**
  - My first throughput attempt held the run lock only around `true`, not around the games. That is six runs of
    about 1.7 s each, outside the lane. They were thrown away and run again under the lock. Only the locked
    numbers are reported.
  - A few loops took an agent name or a binary path from a loop variable (`--agents $a`, `$B/riftbound_$c`). No
    command was built from a variable. Where that mattered, I wrote the commands out in full.
- **ISMCTS on the first pass ran on a binary without two of the gate fixes.** Those runs are not reported. Both
  commands were run again, ten times each, on `1095f92`.
- **J3 has no red test.** Its outcome cannot differ with the registered cards: a 468.4 chain exists only after a win,
  and a unit that arrives in it leads to a new combat either way. The verdict is therefore NOT A DEFECT, with the
  code made literal. The one case the rules leave open is judge question 13, and the engine's behaviour there is pinned.
- **D32 and D57** were listed as "verify and pin". On this tree they were real defects, and they are FIXED with G16.
- The smoke movement is explained by building each mechanism commit in a scratch tree from `git archive` (section 4).
  The worktree's own build directories were not used for that.
- Nothing was pushed, and no PR was opened.

## 3. Verdicts

The completed-plan entries (`plans/completed/10-testing-phase.md`, section "Batch 3") list every test. In short:

| item | verdict | how |
|---|---|---|
| D21 | FIXED | a counter chooses the finalized item it counters as it is played (`Card::pickChainItem`, `ChainItem::chosen_item`); the item is removed wherever it lies by the one operation `ChainManager::removeItemFrom`; a mistarget if it left; counter-of-counter tested; 12 cards |
| D8, F7, G20 | FIXED | pair gates test pairs; `pickTargetPair` offers a first target only with a completing second, judged with the first one's Deflect owed too (`HypotheticallyUnpaid`); a whole-pair probe runs when a Deflect or a discount could decide it; a finalize-pass spell is probed whole when a Deflect is on the board; the split helper and Moonfall's "may" are filtered the same way; a pre-play pass that misses a required target throws |
| D3, D16, D17, D34 | FIXED | move destinations and play-time targets are chosen in the pre-play pass, before the cost. Thrill of the Hunt is NOT A DEFECT (CR 355.10.b), pinned |
| D22, D14, D24, D54, D78, B5, G17, D67 (Guerilla) | FIXED | choices at finalization for keyword abilities, [Quick-Draw] (now a triggered ability on the chain), Azir, Tornado Warrior, Evelynn, Kennen, Zed (the engine's convention for the location, see M5), Emperor's Divide, Guerilla Warfare |
| G15, D38, D15, G19, D67 (Swift Scout), D41 (Forge) | FIXED | an activation is pending, then its choices, then its cost (with the owed cost judged against each choice's Deflect), then finalized; "Kill this" is `kill_self`; Equip is a chain item; Poro Snax and Scryer's Bloom were converted (gate finding) |
| F5 | FIXED | "pay [N] to <choose>" triggers declare the cost still owed; a choice is offered only if its Deflect and [N] are payable together |
| D2, D30, D31, D64 | FIXED | [Add] abilities resolve as they are played, keep Focus and open no priority window; Add Reactions while a cost is paid, probed over orders, only viable ones offered (gate finding) |
| G16, D32, D57 | FIXED | effect plays that pay make the spell's choices before the cost, ask the one intent target, and are not made without a legal target |
| F6 | FIXED | a spell's non-resource additional cost is paid with its Energy and Power (CR 357.2) |
| K1 | FIXED | `playPayingCost` no longer forgets the flags its own payment set |
| G14 | FIXED | chosen objects are stamped with their incarnation; a re-entered object is a mistarget (0 bytes per copy) |
| J2 | FIXED | staged showdowns and combats begin at the end of every phase (CR 323.11 / 323.12); CR 315-317 order kept |
| J3 | NOT A DEFECT (outcome) + QUESTION 13 | staging decided at the result, 468.5 skipped when staged; the both-present case asked |
| D79 | NOT A DEFECT | closed by batch 4b; pinned |
| B2, H3 | FIXED / DONE | one `playedFromFacedown` helper; the stale-mark half pinned |
| B9, G27 | DONE | D32 / D57 fixed (see above); every G27 sighting has a test (D5 and D18 verified green from batches 2 and 4a) |

## 4. Smoke and decision flow

Fiora vs yi_rebuild, 200 games, seed 20261010, 3 threads. Each commit was extracted with `git archive`, built in a
scratch directory and played. The cards of this batch that are in the two lists are Repulse, Riposte, Defy, Not So
Fast, Charm, Alpha Strike, Divining Shells and Forge of the Future.

| tree | P1 / P2 | decisions per game | what changed in these games |
|---|---|---|---|
| `51f842a` base | 135 / 65 | 282 | — (= batch 4b's merged tree) |
| `5034619` D21 | **120 / 80** | 282 | Defy, Not So Fast, Repulse and Riposte choose the spell they counter. They are now playable against a spell under an ability, and the random agent picks among several spells: counters land on different spells |
| `14ceb59` spells choose before cost, pair gates | **128 / 72** | **290** | Charm's destination and Alpha Strike's choices move before the cost, adding prompts. Plays without a legal pair are no longer offered |
| `18d9228` abilities at finalization | 128 / 72 | 290 | none of those cards are in the lists |
| `7e30c40` activations choose before paying, Equip | **130 / 70** | **288** | Divining Shells is killed as it is activated; Forge of the Future chooses its cards at activation |
| `db3c7b6` [Add] | 130 / 70 | 288 | no [Add] card in the lists |
| `3d112fb` effect plays, F6, K1 | 130 / 70 | 288 | no effect play that pays in the lists |
| `677dddb` G14 | 130 / 70 | 288 | no leave-and-return in these games |
| `dd9df7f` J2 / J3 | **128 / 72** | 288 | 2 games: a combat staged outside the Main Phase begins in that phase |
| `1095f92` final | 128 / 72 | 288 | the merge and the gate fixes move nothing here |

## 5. Throughput

1,000 games, 1 thread, fiora vs yi_rebuild, seed 20261010, under the run lock, base and final alternating, three
runs each (wall seconds):

| run | base `51f842a` | final `1095f92` |
|---|---|---|
| 1 | 1.60 | 1.74 |
| 2 | 1.59 | 1.78 |
| 3 | 1.59 | 1.75 |

The mean is 1.593 s against 1.757 s: **10.3% more time per game**, or 629 games/s against 569 games/s. There are
2% more decisions per game (288 against 282). One run per commit (two runs each, in seconds):

| commit | runs | change from base |
|---|---|---|
| base | 1.58, 1.57 | — |
| `5034619` D21 | 1.67, 1.65 | +5% |
| `14ceb59` | 1.77, 1.78 | +12% |
| commits after `14ceb59` | 1.70-1.77 | flat |

The cost is in the play gates:
- the counters scan the whole chain for an item they can choose (`anyChainItem`);
- the pair and finalize-pass probes copy the game when a Deflect or a discount could decide a play;
- the pre-cost passes of the spells that now make their choices before their cost.

This adds to I4 (throughput, batch 5). The obvious lever is to cache the gate per decision; it is not done here.

## 6. Fuzz, final tree (`1095f92`)

`deck_fuzz --known docs/testing-known-failures.txt`, with fresh seeds. Optimised: `--decks 5000 --games-per-pair
20`, seeds 20263741-3. Checked: `--decks 2500 --games-per-pair 8`, seeds 20263841-3. Batch 4b's numbers are in
brackets.

| build | agents | games | failures | groups | no winner / long chains / hung | decisions per game |
|---|---|---|---|---|---|---|
| optimised | random | 100,000 | 0 [5] | — [F5 4, F6 1] | 0 / 0 / 0 | 262.3 [270.4] |
| optimised | random-player | 100,000 | 0 [14] | — [F5 12, F6 2] | 0 / 0 / 0 | 254.0 [261.5] |
| optimised | random-player:pass=0 | 100,000 | 0 [25] | — [F5 20, F6 5] | 0 / 0 / 0 | 290.3 [297.2] |
| checked | random | 20,000 | 0 [2] | — [F5 2] | 0 / 0 / 0 | 264.6 [271.4] |
| checked | random-player | 20,000 | 0 [1] | — [F5 1] | 0 / 0 / 0 | 254.0 [262.1] |
| checked | random-player:pass=0 | 20,000 | 0 [3] | — [F5 2, F6 1] | 0 / 0 / 0 | 292.2 [297.3] |
| optimised | pairing, 39 lists x7 | 10,647 | 0 [0] | — | 0 / 0 / 0 | 309.1 [314.2] |
| checked | pairing, 39 lists x7 | 10,647 | 0 [0] | — | 0 / 0 / 0 | 309.1 [314.2] |

- **No failure group at all.** F5, F6 and F7 are gone, and their lines are removed from the known-failures file.
- Decisions per game fell by about 7 in each run. [Add] opens no priority window. Plays without a legal pair are no
  longer offered. A payment no longer offers useless Adds.
- One run during the iterations (seed 20263723, pass=0) flagged a 200-item chain. Irresistible Faefolk ("When I
  move to a battlefield, you may move an enemy unit to that battlefield") and Imposing Challenger ("When I move,
  you may move an enemy unit here ... to a different battlefield") pull each other back and forth, both players
  always accepting. The loop rule (`ChainState::kMayLoopIterations`, 100 per player; Tournament Rules 505.4)
  stopped it, as designed. It is not a failure, and the final runs have none.
- The fuzz findings of the gate's first pass (section 2) were each replayed alone, tested red and fixed.

## 7. Tests changed by design

Each one pinned the old behaviour. For each: old -> new, and the rule. No test was deleted without a replacement,
and no assertion was weakened.

- **D21** (CR 355.9.a.2: a counter chooses a finalized item):
  - `C3Test.CountersAreOnlyPlayableAgainstTheChainTop` -> `CountersCanChooseASpellUnderAnAbility`.
  - `C4Test.Rebuttal_IsNotPlayableAgainstAnAbilityOnTopOfASpell` -> `Rebuttal_ChoosesASpellUnderAnAbilityOnTheChain`.
  - `RulesComplianceTest.Counter_FlowSpellIsBanishedNotTrashed` / `_OrdinarySpellGoesToTrash` /
    `_DoesNotUncountThePlayedCard`, `VendettaB` (Mel), `VendettaD.Vendetta_DecreeOfRage_CantBeCountered`: the helper
    `counterChainTop(ctx)` -> `counterChosenItem(ctx, id)`. The assertions are unchanged.
  - `VendettaE.Vendetta_Rebuttal_*` (3) and `ReviewFixC2Test.MysticReversal_TheNewControllerMakesNewChoices`: the
    first prompt is now the spell.
  - `VerifyCardsTest.Pin_Batch03_NotSoFastAndRepulse_ATriggersSubjectIsNotAChoice`, `RulesCards2Test` (3 hand-built
    abilities), the `LegalTargetsFixture` / `CardTestFixture` `pushSpellOnChain`: hand-built chain items are
    Finalized, which is what a player responds to.
  - `SweepCostsTest.FinalizePassSpellDeclarationsMatchTheirPickLists`: a spell that cannot be played on the board
    gets an enemy spell on the chain; Flurry of Feathers joins the modal list.
  - The StrongIds audited list.
- **D8 / D3 / D16 / D17** (CR 355.8, 355.16, 355.4):
  - `ChoiceHelperTest.PickTargetPairWithNoSecondTargetIsAFinishedPartialPair` -> `...OffersNoFirstTarget`.
  - `ReflexiveCardsTest.DragonsRage`, `C3Test.RelentlessPursuit_*` (2),
    `VerifyCardsTest.Pin_RelentlessPursuit_NoEquipment_StillMovesAndGrants` and
    `ReviewFixC2Test.Showstopper_OnlyABaseUnit_ToTheChosenBattlefield`: the unit and the destination are chosen in
    the pass. The intent names no target, as the generator emits it.
  - `ReviewFixC2Test.Defy_DoesNotCounterASpellThatCostsMoreThanOnePower`: Defy is no longer offered against such a
    spell.
- **C for abilities / B2**:
  - `Batch1TriggerConditions.EdgeOfNight_*`, `VendettaCTest.Vendetta_TornadoWarrior_*` (2),
    `RulesCardsTest.RulesCards_Evelynn_*` (3) and `VendettaA.Vendetta_Zed_ShadowCloneOnlyIfAdditionalCostPaid`:
    "the mark is consumed" -> "forgotten at the next play"; the trigger condition is asked (CR 383.2.a.1).
  - `VerifyCardsTest.Pin_EmperorsDivide_*` (2): one play with no intent target; the units are chosen in the pass.
- **B**:
  - `SweepCostsTest.TypedDiscardCost_SkyCrusierDiscardsTheGearThePlayerPicks`: the target is chosen, then the
    discard (CR 403.2 -> 405.1).
  - `VerifyCardsTest.Pin_ForgeOfTheFuture_ZeroCardsRecycled`: the cards are chosen at activation.
  - `Gold564Test.KillSelfAddsOneUniversalPower`: activated through the engine.
  - `C3Test.Conscription_SpendingFiveXpIsPaidOnceAndWidensTheChoice`: a small unit is added.
  - New: `C3Test.Conscription_WithOnlyABigEnemyThePaymentIsNotAsked`.
- **G16 / D57** (CR 355.8: no target, no play). A target was added to each; the assertions are unchanged:
  - `CostPaymentRulesTest.EffectPlay_FullCostIsChargedThroughTheNormalPath` and `.EffectPlay_IgnoringEnergyStillChargesPower`;
  - `C3Test.DragUnder_CostsTwoLessWhenAnEffectPlaysItFromOutsideTheHand`;
  - `SweepTriggersTest.DragUnder_PlayedOffTheDeckCostsTwoLess`.

## 8. Trainer notes

The user's instruction stands: no trainer work until every engine batch has landed. This is a list only.

- **Unchanged:** `kStateFeatureDim`, `kCardVocabSize`, the feature layout, **the action-id scheme and the history
  token.** No new slot kind is needed. Every new prompt is a `MakeChoice` naming an object or a value, or an
  existing activation intent.
- **Fewer decisions (mechanism A, D2 / D31):**
  - An [Add] activation opens no priority window and leaves no chain item.
  - Focus stays with the player who activated it.
  - Gold, the Seals, Scorn of the Moon and the other Add cards no longer give the opponent a response window.
- **New prompts:**
  - D21: a counter asks which spell or ability it counters, as it is played. It is a `MakeChoice` with
    `chosen_objects = {item's source}` and `chosen_value = item id`. Hard Bargain's [Repeat] asks for its second
    spell together with the first.
  - D30: while a cost is paid and the pool and runes are short, "activate an [Add] ability to pay" is asked. The
    options are `ActivateReactionAbility` intents, and only those after which the payment can still be made.
  - G16 / D32 / D57: an effect play that pays asks the spell's target before its rune prompts.
- **Prompts that moved earlier — the same prompts, before anyone can respond and before the cost:**
  - spells: move destinations (Ride the Wind, Charm, Showstopper, Dragon's Rage, Stormbringer, Relentless Pursuit,
    Void Assault), Shuriken Flip, Decree of Insight's target;
  - abilities: the keyword abilities (a Sand Soldier's [Weaponmaster]), Azir, Tornado Warrior, Evelynn, Kennen, Zed,
    Emperor's Divide, Guerilla Warfare, Swift Scout, Forge;
  - activated abilities: their target or mode now comes BEFORE their rune prompts (G15).
- **Prompts that moved later:** a spell's non-resource additional cost (Sacrifice's or Heedless Resurrection's kill,
  Meditation's exhaust) is now paid after the rune prompts (F6).
- **New chain items and priority windows:** Equip is an activated ability on the chain (D15), and [Quick-Draw] is a
  triggered ability on the chain (D14).
- **Options that appear or go:**
  - Counters are offered against a spell under an ability.
  - No pair spell, finalize-pass spell or split spell is offered without a complete payable choice.
  - Moonfall's "may" is offered only with a choosable unit.
  - Effect plays offer only spells with a legal target.
  - Defender of Tomorrow and Swift Scout are offered only with something to choose.
- **Turn flow (J2):** a showdown or combat staged in the Beginning Phase, the Draw Phase or the Ending Step begins
  there. So combat decisions (showdown priority, damage) can occur with `TurnState::phase` set to those phases.
- **Every game changes:** the smoke games move (section 4), and so do the fixed-seed ISMCTS games (section 1).

## 9. What another stream needs

- **Batch 5 (T1, prompt descriptions):** these prompts are new:
  - `Card::pickChainItem`: the option's object is the item's source, and its value is the item id;
  - the "[Add] for this payment" activation prompt;
  - the effect-play target prompt.

  They need descriptions like the rest.
- **I4 (throughput):** add the 10% of section 5. The play-gate probes are the place to look.
- **M1-M5** (section 10) are in `plans/remaining/10-testing-phase.md`.
- **The judges:** question 13 (J3, establishing control at 468.5 with both players present) is in
  `docs/judge-questions-2026-10.md`.

## 10. New defects found (not fixed), and one convention

All are written in `plans/remaining/10-testing-phase.md`, section "Found by batch 3":

- **M1** — 107 card files use a choice helper and do not opt into a finalize pass. Each needs reading: a target,
  mode, leading "may" or cost belongs at finalization, while a choice made by the effect belongs at resolution.
  This is the wider K3 kind.
- **M2** — The largest X a player can pay (Hextech Anomaly, Ancient Henge, Bullet Time) does not count [Add] abilities.
- **M3** — When an effect plays one of these spells, they get no targets: Singularity, Deathgrip, Smoke and Mirrors,
  Piercing Light. Each names two or more targets in its play intent and has no pass.
- **M4** — Thrill of the Hunt, with a unit its controller does not own: should the owner, who plays it, choose the battlefield?
- **M5** — A convention, not a defect. An ability's effect play settles its token's location at finalization (Zed
  follows it after B5). CR 185.2.a could be read the other way; it is one question for the whole convention, if the
  owner wants it settled.
