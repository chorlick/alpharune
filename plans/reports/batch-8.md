# Batch 8: round 2's findings (U1-U21), the setup mode (V1-G4), S1, Q3, coverage for the stopping rule (C1)

Branch `fix/batch-8`, from master `2804411`. Master was merged in twice: batch 7-perf (`86ad293`) and `6e33543`
(`59c7079`). Master has not moved since. The gate ran on the final tree.

## 1. Result

Every item is closed:
- **FIXED** (each with a test shown red first): U1-U16, U18, U19, S1, Q3, and the description half of U17.
- **QUESTION** (judge questions 18-21, the engine's current reading pinned and noted at the code): U17 (order prompts
  for copies of one ability), U20 (two questions), U21.
- **DONE**: V1-G4 (Duel by default, Match selectable), C1 (every one of the six cards above the minimum, which was
  not lowered).
- **New defects found and fixed in this batch**: X1, X2, X3 (section 7).

**Gate**: all green. Our code has 0 warnings (8 in `_deps`); checked build 0. 3,638 + 35 + 10/10 tests pass, the
checked suite 3,638. Fuzz: 0 failures in 300,000 optimised, 60,000 checked, 1,500,000 focused coverage games and
2 × 11,767 pairing games.

## 2. Verdicts

| item | verdict | mechanism | tests (red first) |
|---|---|---|---|
| U1 | FIXED (regression of D4) | `WeaponmasterUnit::onTrigger` set "no" without `confirmOptional`, so `may_answer` stayed -1 and CR 403 removal never applied. Now always asked through `confirmOptional` with its `still_legal`. Jax, Unrelenting's same shape simplified. | `Batch8Test.U1_*` (12 units red); sweep `SweepCostsTest.Removal_NoRegisteredTriggerWithNothingToDecideIsFinalizedForNothing` (13 cards red: every Weaponmaster unit and Azir; 9 cards listed by their text) |
| U2 | FIXED (regression of D4) | Azir, Sovereign asks a leading "may" (still_legal: a token can be chosen), then the "any number" picks | `Batch8Test.U2_*` |
| U3 | FIXED | Curtain Call / Rocket Barrage built their mode mask from the units the text allows; the target prompt offers only `offerableChoices` (a [Deflect] unit only with the [A] payable on top). With Rengar, Unseen or Ornn, Forge God the only candidate and no Power left, the generator's probe found a payable sequence through another mode, the player took the dead mode and `executePlaySpell` threw. Masks now from `offerableChoices`. | sweep `U3_Sweep_EveryOptionOfASpellsFirstChoiceCanBeCompleted` (every choosing spell, three fundings, gear and [Deflect] units; red for exactly these two cards) |
| U4 | FIXED | `announcePermanentPlayed` and `EffectExecutor::playIgnoringCostDecided` announce `CardPlayedEvent` / "played_not_from_hand" / `EnteredBoardEvent` inside one `SimultaneousTriggers`. The second shape (play trigger + "becomes Mighty" on entry) is judge question 18 | `U4_*` |
| U5 | FIXED | `BattlefieldState::same_object_as` records every object a slot held before a Replace or swap back (CR 439.1.a, 439.7.a); `battlefieldOfCard` follows it; 11 battlefield cards that compared `card_object_id == ctx.source` converted; Green Father's "that battlefield" follows a swap back | `U5_*` (3) |
| U6 | FIXED | Jhin, Meticulous Killer's alternative cost generated for every source zone and location; the Dragon Roost combination excluded | `U6_*` (3, one of them `U6_NoCardWithAnAlternativeCostIsADragon`) |
| U7 | FIXED | `executePlayFromHidden` and effect plays (`EffectExecutor::setOfferOptionalCost`) offer the optional additional cost | `U7_*` (2) |
| U8, U9 | FIXED | Piercing Light: `needsPlayTimeTarget` + `kFinalizeResolve`, the "up to one other" through `selectTargetOrNone`; each [Repeat] execution chooses in the card's pass, an effect play no longer adds "No target" for the first | `U8_*`, `U9_*` |
| U10, U11 | FIXED (CR 355.1.a, 820.2) | `GameEngine::planRepeats` decides each tranche before any cost (offered only if the whole cost step stays payable, tried on a copy) and makes the extra executions' choices then; the tranches are paid in the card's own payment (`TrancheCost`, `cost_step_tranches_`). Effect plays (Fizz) and trash plays plan them too | `U10_*`, `U11_*` (3) |
| U12 | FIXED | `createToken` offers [Accelerate]; `accelerateAvailable` accepts tokens | `U12_*` |
| U13 | FIXED (CR 829.1.c.3) | `TrashReplayGrant::alternative`: a spell that prints [Flow] and is given [Flow] has both trash plays | `U13_*` |
| U14 | FIXED | `Card::pickCostObject` (ChooseObject, no Deflect): Sacrifice, Heedless Resurrection, Forgotten Signpost | `U14_*` |
| U15 | FIXED | Nami, Headstrong's condition in `triggerCondition` | `U15_*` |
| U16 | FIXED | prompt descriptions: a spell an effect plays, an activated ability's target, a reflexive trigger; a battlefield named by its controller and slot | `U16_*` |
| U17 | QUESTION + FIXED | judge question 20 (order prompts for copies of one ability), pinned `Q20_Pin_*`; descriptions now say " token" and " (exhausted)" | `U17_ATokenCopyAndTheCardAreDescribedApart` |
| U18 | FIXED | trash-play costs paid like a card cost (`payLikeACardCost`, one `askRunesFor` call site): every rune asked, forced picks included | `U18_*` |
| U19 | FIXED | Fizz's log line, a RECYCLE line in the chain manager, `giveTemporaryMight` no "-> N", target-not-offered lines say finalized / resolves | `U19_*` |
| U20 | QUESTION | judge question 18 (a unit entering with 5+ Might "becomes Mighty"), 19 (Prodigal Explorer's "chosen twice"); pinned `Q18_Pin_*`, `Q19_Pin_*`, code notes | — |
| U21 | QUESTION | judge question 21 (Gold 326 vs 564), pinned `Q21_Pin_*`, note at `createGoldToken` | — |
| S1 | FIXED | Thermo Beam kills the gear in one `DeathBatch` | `S1_*` |
| Q3 | FIXED (CR 383.4.g.1, 383.3.d) | Reckoner's Arena: the conquer abilities it activates are one trigger batch | `Q3_*` |
| V1-G4 | DONE | `ModeOfPlay::format` defaults to BestOf1 (Duel: each setup battlefield drawn from the engine RNG, no decision, a public SETUP line); Match asks each player. `GameEngine::setMatchFormat`, `GameConfig::match_format`, `--game-mode duel|match` on every CLI, OpenSpiel `game_mode`, trailing `MatchFormat` on `MctsAgent` / `IsMctsAgent` | `StepApiTest.V1G4_*` (3), `RiftboundStateTest.TheGameModeParameterDecidesWhetherTheSetupBattlefieldIsAsked` |
| C1 | DONE | `deck_fuzz --focus ID,... [--focus-boost B]`; deck lists name a printing with `<name> #<id>` (and `toDeckList` writes it); new lists `decks/yasuo_546.txt`, `decks/rell.txt` | `C1_*` (2); coverage table in section 5 |

**Red first.** Every FIXED item's test failed on the unchanged code. One exception was caught at the end: the first
U11 regression test (Bellows Breath, `U11_TheCardsRunePicksKeepTheRepeatPayable`) passed on the two-step payment too,
so it did not cover the coverage run's failure. It was replaced by `U11_TheCardsCostAndTheRepeatArePaidAsOneCost`,
built from the fuzz replay (Upstage Comedy, Dragonsoul Sage, Butcher of the Sands' [Add] recycling the runes the
tranche needed), which fails on the two-step payment with the fuzz game's exact message and passes now.

## 3. Tests changed by design (old -> new)

| test | change | reason |
|---|---|---|
| `Batch2Combat.D23_*`, `C6Test.AzirSovereign_*`, `Batch3Test.D24_*` | answer Azir's new "may" | U2 |
| `ReviewFixC2Test.PiercingLight_FirstAtABattlefield_SecondAnywhere_OrNone`, `VerifyCardsTest.Pin_PiercingLight_NeedsTheFirstUnit_TheOtherIsOptional` | one Play intent; the units are prompts | U8, U9 |
| `ChainTriggerFixTest.Repeat_RepeatedEffectGetsItsOwnChoiceAndHappens` | object prompts 2 -> 4 | U11: the Repeat's runes are named |
| `ChainTriggerFixTest.Repeat_HandRolledChoiceCardFinishesItsRepeat` | 2 -> 3 | U11 |
| `Batch4bTest.G4_Pin_EachPlayerChoosesTheirBattlefield` | renamed `G4_Match_EachPlayerChoosesTheirBattlefield`, runs a Match | V1-G4 |
| `StepApiTest.TheSetupBattlefieldChoiceIsNotABattlefieldSlot` | runs a Match | V1-G4 |
| `RiftboundStateTest.AnEditThatChangesTheDecidersOptionsIsRefused` | walks to a decision with a hand play offered (the Duel's games differ) | V1-G4 |
| `PerfEngineDeathBatch.EveryCardBatchSaysWhoDies` | 9 -> 10 sites | S1 |
| `Batch8Test.U11_TheCardsRunePicksKeepTheRepeatPayable` (this batch's own) | replaced by `U11_TheCardsCostAndTheRepeatArePaidAsOneCost` | did not cover the failure (section 2) |

## 4. Gate (final tree, master `6e33543` merged)

**Build:** clean rebuild of `all` + `deck_fuzz prompt_trace search_strength balance collision_census`, lane A, -j3:
1,007 steps, 8 warnings, all in `_deps`. Checked build: 0 warnings.

**Suites:** `riftbound_tests` 3,638 passed (1 disabled); `riftbound_openspiel_tests` 35; `clone_equiv` 10/10; checked
3,638. After the U11 test replacement: the suite again 3,638, and `Batch8Test.*` on the checked build.

**Leak long run** (`RIFTBOUND_LEAK_GAMES=300`): 6 passed. Relabelled 300/300 to the end (82,653 decisions, 674,666 log
lines); variants 84,672 decisions; passive 300 pairs. The relabelled run found X1 before it passed.

**Smoke** (fiora v yi_rebuild, 200, seed 20261010): Duel 124/76/0 (avg 283 decisions); Match 136/64/0 (289) =
master 136/64/0 (289). 44 of the 200 Match games differ from master; all 44 first diverge at an Onslaught Flow play
now paid like a card cost (U18: each rune asked); other differences are log words (U14, U19). Same winner in all 44.
Every Duel game differs (the setup is drawn from the engine RNG, two decisions fewer).

**MCTS** (seed 7, `mcts:sims=8`, 4 games): rc 0, P1 2 / P2 2. With `RIFTBOUND_STEP_FAULT_AT=40`: rc 1, "injected
engine fault at decision 40", 0 draws.

**ISMCTS**, ten runs of each command, identical output: sims=50, 2 games, seed 20261010 (P1 8-4, P1 8-1); sims=20,
8 games, seed 1 (7-1).

**Pairing** (41 lists, 7 per ordered pairing, 11,767 games, seed 8401): 0 failures on both builds, identical rows,
P1 5,846 / P2 5,921.

**Fuzz, optimised, 3 × 100,000** (seeds 8160001 random, 8160002 random-player, 8160003 pass=0): 0 failures. One
chain of 200+ in pass=0: the Faefolk / Challenger loop ended by TR 505.7 (replayed, as round 2 and M1 found).

**Fuzz, checked, 3 × 20,000** (seeds 8161001-3): 0 failures.

## 5. C1: coverage of the six cards

Three focused runs of 500,000 games (`--focus 205,546,803,852,347,806`, seeds 8170001 random, 8170002 random-player,
8170003 pass=0): 0 failures (one TR 505 Faefolk loop at pass=0, replayed). Resolutions per million (minimum 200; 50 at
pass=0):

| card | random | random-player | pass=0 |
|---|---|---|---|
| Yasuo, Windrider 205 | 444 | 552 | 1,104 |
| Yasuo, Windrider 546 | 478 | 514 | 1,086 |
| Rell, Magnetic | 2,576 | 3,506 | 3,914 |
| Eclipse Dragon | 1,164 | 1,178 | 1,990 |
| Renekton, Rage Fueled | 1,624 | 1,148 | 1,762 |
| Swain, Visionary | 966 | 822 | 698 |

The first focused coverage run found the first U11 version's hole (56 engine errors, "payRepeats ... could not be
paid"); the runs above are on the fixed tree.

## 6. Throughput

fiora v yi_rebuild, 2,000 games, 1 thread, seed 20261010, under the run lock; master = batch 7-perf's binary. Wall
time ~3.0-3.3 s for every binary (noisy). `perf stat -e instructions:u`, two runs each:
- master 20.274G / 20.269G;
- batch 8, Match 20.299G / 20.312G (+0.15%: 287 decisions per game, the rune prompts U18 added);
- batch 8, Duel 20.272G / 20.279G (284 decisions per game).

Cycles within noise (12.1-12.3G).

## 7. New defects found (all fixed here)

- **X1** `TriggerManager::fireTrigger`: a unit dying in a death batch still triggered on another member's death when
  it was the batch's pending member (CR 383.2.c.2; The Ruination). Found by the relabelled leak test under Duel games.
  `HiddenOrderLeak` now prints both logs under `RIFTBOUND_LEAK_VERBOSE`.
- **X2** Altar of Memories chose the hand card as a target: `pickUnseenCard`.
- **X3** `disposeCounteredCard` left `__recycle_on_leave_chain` on a countered card.

## 8. Not done as asked

- **C1 by focus, not only by lists.** Lists were made for Yasuo 546 and Rell; lists for Swain and Renekton were tried
  and dropped (low yield: Butcher of the Sands starves the runes; kept in the scratchpad). The six cards are met by the
  new `--focus` deck building, which is how the stopping rule's coverage runs should be made from now on. The minimum
  was not lowered.
- **The first U11 regression test did not cover the failure** (section 2); found and replaced before this report.
- **Tests are committed with their mechanisms**, except the questions' pins (one commit) and the U11 replacement.
- **One interim note** (`.worktrees/reports/batch-8-1.md`), written after U1-U5.

## 9. Trainer notes

`docs/trainer-notes-2026-10.md`, "Batch 8": an engine game is now a **Duel** by default (setup battlefields drawn, no
decision); **the trainer must set Match explicitly** (`game_mode=match`, `--game-mode match`,
`GameEngine::setMatchFormat`, the agents' last constructor argument). No observation, vocabulary or `kNumActions`
change. Prompts that changed: [Repeat] before any cost and paid with the card, trash plays paid like a card, two Flow
plays, Jhin's alternative cost everywhere, Piercing Light's prompts, Azir's "may", cost picks as ChooseObject, tokens
with [Accelerate], optional additional costs on facedown and effect plays, order prompts for one play's abilities and
Reckoner's Arena, `describePrompt` words.

## 10. Open, not ours

L1 / G28, K2 / L3, P1 stay in `plans/remaining/10-testing-phase.md` as they were.
