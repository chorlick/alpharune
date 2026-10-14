# batch-4b — final report: generators, payments, hidden information, turn structure, questions

## 1. Result

- Branch `fix/batch-4b`, from master `a92407a`. Master was merged three times:
  - after batch 2 landed (`d125a2c`, at `64c3b86`);
  - a plans-only commit (`bada264`, at `9da08ca`);
  - after batch 1b landed (at `03d1a64`).

  The gate below ran on `03d1a64`, the merged tree. A first full gate on `0ce3a50`, before batch 1b, is not
  reused. Its one finding (Experimental Hexplate, section 10) was fixed before the merge. The commit after the gate
  adds only this report.
- **Items: 31 handled.** 26 FIXED, 2 QUESTION written for the judges with the behaviour pinned (D28, D80), 1
  QUESTION for the project (G4), 1 NOT A DEFECT (G25), and the batch-4b part of H3 (G21 and G23 verified against
  batch 4a's fixes and pinned). I3 came in mid-batch and is FIXED. B7 was done after batch 2 merged, as the brief
  required.
- Clean rebuild (`cmake --build build --target clean`, then `all` plus `riftbound_tests riftbound
  riftbound_clone_equiv_test riftbound_openspiel_tests deck_fuzz`), lane A, `-j3`: **0 warnings in our code**. The log
  has 8 warnings, all in OpenSpiel's own sources (the `all` target builds them).
- **`riftbound_tests`: 3,392 passed, 0 failed** (1 disabled, as on master). `riftbound_openspiel_tests`: 27 passed.
  `riftbound_clone_equiv_test`: 10/10.
- Checked build (`-DRIFTBOUND_CHECK_BOARD_INDEX=ON`, `build-check`, incremental, 0 warnings in our code): suite 3,392 passed.
- Smoke (fiora vs yi_rebuild, 200 games, seed 20261010, 3 threads): rc 0, **P1 135 / P2 65 / 0 draws / 17 turns / 282
  decisions**. Master after batch 1b has 121 / 79 / 0 / 17 / 272. Every step of the movement is in section 4.
- MCTS batch, 4 games, seed 7: rc 0 without the fault (3-1). With `RIFTBOUND_STEP_FAULT_AT=40`: rc 1, "a game failed:
  game 1 [seed=7]: RIFTBOUND_STEP_FAULT_AT: injected engine fault at decision 40", 0 draws.
- ISMCTS, ten runs of each command, decks yi_rebuild vs fiora:
  - `ismcts:sims=50 --agent2 random --games 2 --threads 1 --seed 20261010`: rc 0 ten times, the same two games every
    time (P2 5-8 in 354 decisions, P2 7-8 in 397).
  - `ismcts:sims=20 --games 8 --seed 1`: rc 0 ten times, 2-6 every time, the same eight games.
  - No "did not reach the snapshotted decision" in any run.
- Pairing run, random vs random, 7 games for every ordered pairing of the 39 lists in `decks/` (seed 20261901): **10,647
  games on each build, 0 failures, 0 games without a winner, 0 flagged**. The two builds played the same games (P1 5,338 /
  P2 5,309 on both).
- Fuzz: section 6. **No new failure group** in 300,000 optimised and 60,000 checked games. Only F5 and F6 occur,
  both known and both batch 3's. F5 occurs more often than in batch 2's run: D9's rune choices expose it (section 6).

## 2. What was not done as asked, said plainly

- **Two copies of me worked in the tree for a few minutes.** The orchestrator restarted at about 04:45, but the
  previous session's process kept running until about 04:52. In that window it edited the worktree beside me:
  - the G1 test rewrite in `test_burn_out_and_scoring.cpp`;
  - the B8 test in `test_fixture_contract.cpp`;
  - changes to `tests/cards/test_batch_4b.cpp`;
  - `advanceTurnQueue`, `testHook_nextTurn` and `testHook_setupBattlefields` in `game_engine.{h,cpp}`.

  I stopped editing and reported it. The coordinator confirmed the other process had exited and that its edits were
  the same plan carried on. I then read the whole uncommitted diff and kept one consistent version: its edits and
  mine matched. I rebuilt under the lane-A lock before going on. Nothing had been committed by either copy before
  that point.
- **Red first, and how it was shown.** Every new test that pins a defect was shown failing on a tree without its fix
  before the fix was committed. The way differed by mechanism:
  - For the first set, the test hooks went in with the old behaviour, then the tests ran: 18 of 23 red, and the 5
    green are pins and positive halves.
  - For Accelerate, B6, H1, D46, B7 and D11, the tests were written before the fix and ran red on the built tree.
  - For Frigid Jewel, D9, D50, I3 and D71, I had already written the fix. I saved the source diff, set `src/` back
    to the last commit, built, ran the new tests (red), and re-applied the diff.
  - The D50 test I wrote first used Rocket Barrage and passed on the old tree: its [B] took the spell Power, so the
    board did not show the defect. I replaced it with a Stupefy board, which was red.
  - D71's red is on the chain item ("TRIGGER: Nocturne"). On the old tree Nocturne also ends up at base, because
    he was banished from the bottom of the deck after the recycle.
- **D9 asks less than the base cost does.** The base cost asks for every rune, even when only one can pay. The new
  payment prompts ask only when the runes on offer differ in kind (card, Domains, ready or exhausted). Runes of one
  kind are one option, represented by the lowest id. I said so in the interim note, and nobody corrected it.
- **B6 is wider than its list.** Frigid Jewel ("your second card each turn") had the same root and is fixed too.
  Shard of Undoing and Jayce, Brilliant Inventor were not in B6's list but have the same pattern; they are fixed.
  Blade Twirler was checked: it already counts the object's own moves (`moves_this_turn`), so it was not changed.
- **D71's reach.** The replacement applies at every `CardRevealedEvent` from a Main Deck and at the four looks that
  announce none: Void Hatchling, Abandon's Predict, Baited Hook, Lightning Rush. A look added later must call
  `seenOnTopOfDeck` itself (CLAUDE.md says so). Revealing cards are protected centrally rather than one by one:
  `deck_ops`, the executor's moves and the prompts skip a card that left. I did not rewrite the ~25 revealing cards.
- **The first gate was on a tree that was not final.** Batch 1b merged to master while it ran, so everything was
  measured again on the merged tree. On that first gate, the checked random fuzz found one new group: "a Cleanup
  that would not have been run changed what a player observes". The cause: Breakneck Mech's "Your Mechs have
  [Deflect] and [Ganking]" saw Experimental Hexplate's Mech tag one aura pass late, when its hook ran first. Batch 1b
  found the same thing as J1 and fixed it from the other side: its Cleanup change check now sees tags. I fixed it at
  the source: `Card::applyToBearer` applies the tag with the equipment effects, before any card hook. **Both fixes
  are kept**, and both regression tests pass together (`Batch4bTest.Gate_AnEquipmentsMechTagIsSeenByMechAurasInTheSamePass`,
  `DeckFuzz.B1_ATagGivenInTheAuraPassIsAChangeTheCleanupSees`):
  - mine makes the hooks that depend on a Mech see the tag in the same pass, so the game's result does not depend
    on hook order;
  - 1b's makes the Cleanup notice any tag change, so no other card that writes a tag in a hook can hit the skip.
- **Batch 1b's area.** I edited `TriggerManager` in three places:
  - `fireTrigger`: the first-time stamp for B6;
  - `onCardRevealed`: D71;
  - `subscribe`: clearing `left_while_seen`.

  I did not touch any `triggerCondition` of the G9 cards. Five cards' own conditions changed for B6 (Wraith of Echoes,
  Shard of Undoing, Jayce, Lucian, Draven); none is on G9's list.
- **Smoke per mechanism** was measured by building each mechanism commit's `src/` in the worktree (section 4).
  **Throughput was not measured** (section 4). Nor did I replay an F5 game to show its rise (section 6). Both were
  queued behind batch 5's long run on the run lock, and I withdrew them so as not to hold the lock queue.

## 3. Verdicts

Per item, the outcome and test names are in `plans/completed/10-testing-phase.md`, section "Batch 4b". In short:

| item | verdict | rule | what changed |
|---|---|---|---|
| G1 | FIXED | 487.7 / 488.7 (only the extra rune); 489.7-491.7 are multiplayer | the first player draws in their first Draw Phase |
| G2 | FIXED | 735, 737, 738 | one Additional Turn queue, newest first; the regular order resumes after it |
| G4 | QUESTION (project) | 487.5 / 488.5 | pinned: each player chooses (section 5) |
| G18 | FIXED | 355.9 | Defender of Tomorrow offers any gear |
| G21 | closed by 4a, pinned | 811.1.d.3 | — |
| G22 | FIXED | 811.1.d.1, 054.1 | no hidden unit where units can't be played |
| G23 | closed by 4a, pinned | 187.8 (J3 stays open) | — |
| G24 | FIXED | 439.6, 439.7.b.1 | a Brush can be replaced; the original card stays the swap-back |
| G25 | NOT A DEFECT | 420.4.a.1, 420.4.b, 420.5 | existing pin `SweepTriggersTest.AstralHeron_ACounteredFirstCardStillCountsAsPlayed` |
| H1 | FIXED | — (a "this combat" effect outlives its source) | `GameState::combat_keywords` |
| H2 | FIXED | 124.1 | `healObject` only on an object in play |
| H3 (4b part) | done | — | G21, G23 pinned |
| B3 | FIXED | 383.2.c.1 | Immortal Phoenix only from the trash |
| B6 | FIXED | 383.1, 383.1.b | turn counts for player events; `Card::firstTimeEachTurn` for the object's own |
| B7 | FIXED | 460.2.c.6 | step-by-step assignment past 64 outcomes |
| B8 | FIXED (tooling) | — | `withEngine` throws on a turn player it would ignore |
| D9 | FIXED | 164.2, 357 | the payer names the runes of every payment |
| D11 | FIXED | log audience | `hiddenFrom` / `audienceNaming`; CHOSE lines in two copies |
| D25 | FIXED | 811.6, 813.2 | facedown cards in the Main Phase |
| D28 | QUESTION (judge q. 11) | 355.8 vs "If you do" | pinned |
| D37 | FIXED | 822.1.b-d | [Ambush] location at every timing |
| D43 | FIXED | 805.1, 356.4.c, 356.4.f | Accelerate takes the optional-cost discount |
| D46 | FIXED | 355.1, 763, 763.1 | `Card::asYouPlayChoices`; any listed tag |
| D48 | FIXED | 805.1.b, 356.1.b.3 | Accelerate on plays ignoring the cost |
| D50 | FIXED | 356.2.a.2 | a spell's Deflect is `SpendFor::Spell` |
| D58 | FIXED | 805.1.b | Rek'Sai's grant on effect plays |
| D63 | FIXED | 822.1.b, 420.1.a | Ambush from the Champion Zone |
| D71 | FIXED | 370.1.c.1 | Nocturne as a replacement inside the looking effect |
| D74 | FIXED | 811.1.d, 811.1.d.2 | pair spells from facedown: one play, bound to the battlefield |
| D80 | QUESTION (judge q. 12) | 323.4, 359.3.e.13, 383.2.c.3 | pinned; the two printings made to agree |
| I3 | FIXED | — | pool Power counts as spent |

## 4. Smoke and decision flow

Fiora vs yi_rebuild, 200 games, seed 20261010, 3 threads. Each mechanism commit's `src/` was built in the worktree
and played; the binaries are the same as the gate's except for `src/`.

| tree | P1 / P2 | decisions per game | what changed in these games |
|---|---|---|---|
| `a92407a` master before batch 2 | 117 / 83 | 278 | — |
| `a996ece` G1, G2, generators, the first cards | **130 / 70** | **286** | G1: the first player draws. Every game has one more card for whoever goes first. The other changes in this commit (D25, D37, D63, G22, D74, G18, B3, G24, D80) were not reached, or not decisive, in these lists |
| `00339bc` Accelerate | 130 / 70 | 286 | neither list has an effect play of an [Accelerate] unit or Ezreal; no change |
| `1142599` B6, `dcee62b` H1, `55d2540` D46 | 130 / 70 | 286 | none of these cards are in the lists |
| `d125a2c` master after batch 2 | 122 / 78 | 271 | (batch 2's own movement) |
| `64c3b86` merge of batch 2 | 132 / 68 | 279 | batch 2 plus the above: the G1 offset holds (+10 P1, +8 decisions) |
| `8010594` B7, `cdec83d` D11 | 132 / 68 | 279 | no board here has more than 64 assignments; logs do not change play |
| `ce2770d` D9, D50, I3 | **136 / 64** | **282** | D9: rune picks on non-base payments (Accelerate, Equip, activations, Hide in these lists), about +3 decisions a game. The random agent picks runes differently from the engine's order, which moves 4 games |
| `9642ce8` D71 | 136 / 64 | 282 | no Nocturne in the lists |
| `16b24cc` master after batch 1b | 121 / 79 | 272 | (batch 1b's own movement) |
| `03d1a64` this branch merged | **135 / 65** | **282** | against master after 1b: +14 P1 wins, +10 decisions — G1 (about +13 / +8, as above) and D9 (about +4 / +3); the rest of the branch moves nothing here |

So G1 is the large movement: the player going first is no longer a card short. D9 is the rest. The fuzz shows the
same direction: mean decisions per game are 270.4 / 261.5 / 297.2 (random / random-player / pass=0), against batch
4a's 266.0 / 258.9 / 291.9.

**Throughput** was not measured on the merged tree. The run lock was held by batch 5's search-strength run when I
wanted it, so I withdrew the queued measurement rather than hold the lock queue. The fuzz rates are batch 2's again:
1,783 / 1,810 / 1,716 games/s on 3 threads, against 595 per thread. The per-commit 1,000-game timings I took
before the merge waited on the run lock inside the timed window, so they are not usable.

## 5. Questions for the user

**G4: which mode is one engine game?** At setup each player chooses one of their three battlefields; the engine asks
the agent, and a random agent's uniform pick is what a Duel would do.

- **Duel (CR 487.5, best of one): "randomly selects".** The decision would go. That is two fewer decisions a game,
  and the battlefield would be drawn from the engine's RNG. Search agents and a trained policy would no longer choose
  a battlefield. The deck list's three battlefields would still matter, as the pool of the draw.
- **Match (CR 488.5): "selects".** This is what the engine does now. A single engine game then reads as game one of
  a match. Games two and three would need to exclude the battlefields already used (488.5, 488.6). That needs series
  support in `GameRunner` / `BatchRunner`, which the engine does not have: every engine game is a first game.

Nothing was changed. `Batch4bTest.G4_Pin_EachPlayerChoosesTheirBattlefield` pins the present behaviour and the code
comment cites both rules.

## 6. Fuzz, final tree

`deck_fuzz --known docs/testing-known-failures.txt`. Optimised: `--decks 5000 --games-per-pair 20`, seeds
20261701-3. Checked: `--decks 2500 --games-per-pair 8`, seeds 20261801-3. Batch 2's numbers (its report, section 6)
are in brackets.

| build | agents | games | failures | groups | no winner / long chains / hung | decisions per game |
|---|---|---|---|---|---|---|
| optimised | random | 100,000 | 5 [1] | F5 4, F6 1 [F6 1] | 0 / 0 / 0 | 270.4 |
| optimised | random-player | 100,000 | 14 [3] | F5 12, F6 2 [F6 2, F5 1] | 0 / 0 / 0 | 261.5 |
| optimised | random-player:pass=0 | 100,000 | 25 [2] | F5 20, F6 5 [F5 2] | 0 / 0 / 0 | 297.2 |
| checked | random | 20,000 | 2 [0] | F5 2 | 0 / 0 / 0 | 271.4 |
| checked | random-player | 20,000 | 1 [0] | F5 1 | 0 / 0 / 0 | 262.1 |
| checked | random-player:pass=0 | 20,000 | 3 [1] | F5 2, F6 1 [F6 1] | 0 / 0 / 0 | 297.3 |
| optimised | pairing, 39 lists x7 | 10,647 | 0 [0] | — | 0 / 0 / 0 | 314.2 |
| checked | pairing, 39 lists x7 | 10,647 | 0 [0] | — | 0 / 0 / 0 | 314.2 |

- **No new failure group.** The first gate, on the tree before batch 1b, had one new group in the checked random run.
  It was the Experimental Hexplate / Breakneck Mech aura order; it is fixed (section 2) and does not occur here.
- **F5 rose from 0-2 to 4-20 per 100,000.** F5 is batch 3's item, "you may pay [N] to <choose a unit>". The chosen
  unit's Deflect is paid first, and then the [N] cannot be paid; the card returns before its marker. Kennen, Keeper
  of Balance, Sinister Poro and Icevale Archer are the cards. Before D9 the engine paid that Deflect in its own
  order, which takes exhausted runes first and keeps ready ones. (This is read from the code: `payAnyPower` in a trigger's
  finalize pass now asks `runeChooser`. The one-game trace I queued to show it was withdrawn with the throughput
  measurement, so it is not shown.) Now the payer chooses, and a random agent often
  recycles a ready rune whose Energy the [N] needed. That is a legal choice, so the card's defect, judging the
  Deflect and the [N] apart, is met more often. D9 adds no new cause. The fix is F5's: judge the two together and
  offer only runes that keep the [N] payable, the way `setAheadOfCostCheck` does for spells. It is listed for batch 3
  in section 9.
- F6 (Sacrifice, Heedless Resurrection) is at batch 2's rate.

## 7. Tests changed by design

Each one pinned the old behaviour. For each: old -> new, and the rule. No test was deleted without a replacement,
and no assertion was weakened to make it pass.

- `BurnOutScoringTest.FirstPlayerFirstTurn_DrawPhaseSkipped` -> `_DrawsInOneVersusOne`: no draw -> draws 1
  (CR 487.7 / 488.7; G1).
- `JhinDeckTest.TimeWarp_QueuesExtraTurn`, `ReviewFixC2Test.TimeWarp_IsBanishedAndNotAlsoFiledInTheTrash`,
  `FeaturePerspective.PairedBlocksAreSelfThenOpponent` and the seat mirror: the queue is now
  `TurnState::additional_turn_queue` (read with `GameState::additionalTurnsOf`). The assertions are unchanged; the
  mirror flips the queue and `regular_turn_player` (G2).
- B8: five tests named P2's turn but ran on P1's, because the turn player they passed was ignored. They now set the
  turn they meant, and each still passes:
  - `PerfEngineLookBack` (its play helper passes the caster only as a default);
  - `SweepTriggersTest.DelayedAbility_WatchedPlayerIsNotItsController`;
  - `SweepTriggersTest.PlayIgnoringCost_HonoursEntersReadyEffects`;
  - `KnowledgeReveals.ARevealedHandIsKnownWhileItIsChosenFromAndAfter`;
  - `C7Test.AsheFocused_TheBanishedCardReturnsWhenTheyHold`.
- B6: three tests fired Wraith / Jayce triggers by hand, without the event, so they now set the turn count the event
  would have set: `Batch1TriggerConditions.WraithOfEchoes_OnlyTheFirstDeathOfTheTurnTriggers`,
  `VendettaB.Vendetta_JayceInventor_FirstNonTokenGearEachTurn`, and
  `VerifyCardsTest.JayceBrilliantInventor_IsAskedAgainAfterNewChoicesAndStillResolves` (its stamp assertion is gone
  with the stamp).
- H1: `PerfEngineSources.CardCodeWalksEveryObjectOnlyWhereItIsListed`: Fortified Position removed from the list (it
  no longer walks every object).
- D46: these drove the old AsYouPlayMe trigger or pinned The List's public-tags list.
  - `CardTestFixture.TheList_OnPlayNamesMostCommonEnemyTag` / `_OnPlayClearWinnerNamed` /
    `_OnPlayNoEnemyUnitsNamesNothing` -> `TheList_AsItIsPlayedThePlayerNamesAnyTagOfTheList` /
    `_TheMostCommonEnemyTagIsNamedIfChosen` / `_ATagIsNamedWithNoTaggedUnitInView` (137 options; CR 355.1, 763.1).
  - `ReviewFixC1Test.TheList_TheControllerNamesTheTag`: the agent names the tag by label, not by index 1 of the
    public list.
  - `CardTestFixture.IvernFriend_GainsChosenTagOnAsYouPlayMe`, `AuditFix5Test.IvernFriend_DefaultTagBird` /
    `_AgentChoosesTag`: played through the engine (or `asYouPlayChoices` with a bare executor) instead of the
    trigger driven by hand.
- B7: `RulesComplianceTest.CombatDamage_ManyDefendersEnumeratesQuicklyAndBounded`: a capped list of 64 of the 560
  outcomes -> `Kind::StepByStep` with no list (CR 460.2.c.6).
- D9 / I3:
  - `SweepCostsTest.FinalizePassSpell_AUnitIsOfferedOnlyIfItsDeflectCanBePaidWithTheCost`: one more prompt (the
    Deflect's floating Power, value 0).
  - `SweepCostsTest.PayEnergy_ExhaustsTheLowestIdBaseRuneAndNoRuneOutsideTheBase`: the payer names the rune; both
    base runes are offered and the stray one is not.
  - `SweepCostsTest.PayAnyPower_SpendsFloatingPowerThenRecyclesExhaustedRunesFirst`: `power_spent_this_turn` 1 -> 2.
  - `AurasCostsTest.Repeat_PowerComesFromRecyclingTheRuneToTheRuneDeck`: 1 -> 2.
  - `ChoiceHelperTest.PayOnePowerRecyclesARuneWithTheEnginesBookkeeping`: 2 -> 3.
- D71:
  - `TriggerEventTest.RevealedFromTop_FiresNocturne` -> `_PutsNothingOnTheChainForNocturne`.
  - `VerifyCardsTest.Nocturne_OffTheDeckAtFinalization_NothingAskedAtResolution` -> `Nocturne_SeenOutsideTheDeck_NothingAsked`.
  - `C4Test.Nocturne_PlayingCostsOnePower_AndBanishingAloneIsAllowed` and
    `VerifyCardsTest.Nocturne_CanBePlayedToAControlledBattlefieldButNotWhereUnitsCannotBePlayed`: driven through
    `seenOnTopOfDeck`. With only "don't play me" open, that choice of one is not asked.
  - `StrongIds.EveryObjectIdReadFromAnIntegerSlotIsOnTheAuditedList`: Lightning Rush's slot-reader count 3 -> 4.

New tests: 53 in `tests/cards/test_batch_4b.cpp`, plus `FixtureContractTest.WithEngineRefusesATurnPlayerItWouldIgnore`.

## 8. Trainer notes

The user's instruction stands: no trainer work until every engine batch has landed. This is a list only.

- **Unchanged:** `kStateFeatureDim`, `kCardVocabSize`, the feature layout and values, the action-id scheme and the
  history token. The additional-turns pair of features now counts `TurnState::additional_turn_queue` per player:
  same values, new source.
- **Every game changes.**
  - G1: the first player has 5 cards in their first Main Phase.
  - G2: a Time Warp on the other player's turn now adds a turn.
- **New decisions (no new slot kinds):**
  - D9: rune picks on every non-base payment, `MakeChoice` with the rune as `chosen_objects`, one option per kind. A
    Deflect paid ahead of a spell's cost may also offer "floating Power" (value 0).
  - D43 / D48 / D58: Accelerate offered on effect plays and from facedown with Rek'Sai, and the "[1] or [A] less"
    half prompt with Ezreal.
  - D46: The List names one of 137 tags as it is played (values 16 and up share the overflow slot and are told apart
    by twin number). Ivern's tag is chosen during the play, not on the chain.
  - B7: past 64 outcomes, combat damage is one prompt per unit ("lethal to X next") instead of one
    `AssignCombatDamage` list.
  - D71: Nocturne's banish and play prompts come inside the revealing effect.
- **Options that appear or go:**
  - Facedown cards in the Main Phase (D25).
  - [Ambush] locations in the Main Phase (D37) and from the Champion Zone (D63).
  - No hidden unit at Rockfall Path (G22).
  - Facebreaker / Switcheroo from facedown: one intent, not one per pair (D74).
  - Defender of Tomorrow: ready gear offered (G18).
  - Green Father on a Brush (G24).
- **Logs (D11):** a seat-filtered history now gets the redacted CHOSE copy and no hidden card names in TGT_PICKED,
  TARGET (finalize), PREDICT or the Hide-cost header.

## 9. What another stream needs

- **Batch 1b (merged before this branch):** the contact points are resolved in `03d1a64`.
  - The B6 stamp now sits after `triggerConditionOn` in `TriggerManager::fireTrigger`.
  - My judge questions are renumbered 11 (D28) and 12 (D80); 1b's are 9 (G10) and 10 (G26).
  - H1 is tested against 1b's combat end, where control and the conquer abilities come before the combat ends:
    `Batch4bTest.H1_TheThisCombatShieldLastsThroughTheConquerAbilitiesAndEndsWithTheCombat`.
  - Accelerate on effect plays sits beside 1b's "played not from hand" in `playIgnoringCost`.
  - B8's check passes against 1b's tests.
  - Experimental Hexplate: both fixes are kept (section 2).
- **Batch 3:**
  - D64 (Undertitan, "As I'm revealed from your deck, [Add] [2]") is the same kind of replacement as Nocturne. The
    hook is now there (`Card::replacesBeingSeenOnTop` / `seenOnTopOfDeck`); Undertitan still uses the trigger.
  - Switcheroo and Facebreaker gained `hasLegalChoicesHiddenAt` and hidden-battlefield filtering (D74); their D8 /
    D79 work in batch 3 will meet these lines.
  - `commitChoice` now records the chosen object before paying its Deflect.
- **Batch 3 (F5):** D9 makes F5 more frequent (section 6). F5's fix is to judge "pay [N]" together with the chosen
  unit's Deflect, and to offer only runes that keep the [N] payable. That is the same mechanism as
  `EffectExecutor::setAheadOfCostCheck`, used here for a spell's Deflect paid ahead of its cost.
- No `shared:` commits.

## 10. New defects found (not fixed)

- **An effect play that pays a card's cost loses that play's paid flags.** `EffectExecutor::playPayingCost` pays
  through `payAdditionalPlayCosts` (which sets `kAdditionalCostPaid` or an `OptionalAdditionalCost::paid_flag`) and
  then calls the shared play path, which runs `Card::forgetPlayPayments` and erases them. So "if you paid the
  additional cost" cards played by Void Burrower or Wild Claw read it as unpaid: Zaun Punk, Bard, Mercurial, Safety
  Inspector. CODE-ONLY. The [Accelerate] flags added here are read before that call.
- **G4-adjacent:** `GameRunner` has no notion of a match series, so 488.5's exclusion of used battlefields cannot
  apply. This only matters if the answer to G4 is "Match".
