# Batch 6: effect-play locations (M5), batch M1's findings (R1-R14), R15

Branch `fix/batch-6`, from master `28b0af9`. Master `511c7d6` (batch 5d merged) is merged in. The gate ran on the
final tree.

## 1. Result

Every item is closed:
- **FIXED** (each with a test shown red first): M5, R1-R13, R15.
- **QUESTION**: R14 is judge question 17. Both readings the engine uses are pinned, with code notes.
- **New finding**: S1 (Thermo Beam), recorded in `plans/remaining/10-testing-phase.md`.

**Gate**: all green. Our code has 0 warnings. 3,582 + 34 + 10/10 tests pass. Fuzz: 0 failures in 400,000 optimised
games, 60,000 checked games and 2 × 10,647 pairing games.

## 2. M5 — effect-play locations (DEFECT, CR 420.3.b.1-420.3.b.2)

> 420.3.b.2 ... that information is checked on finalization of the resulting **Game Object**, unless it is specified
> as being checked earlier.

Read with Rek'Sai's example and CR 355.2, this means the location of a played card or token is a choice of that play.
It is made when the played object is finalized, which happens while the effect that plays it resolves.

**Seven abilities chose the location in their own finalize pass.** They now ask after their marker:
- Flame Chompers
- Immortal Phoenix
- Soulgorger
- Kharox (its reflexive)
- Herald of the Arcane
- Zed, From the Shadows
- Undying Loyalty

Herald and Zed have nothing else to choose, so they no longer opt into a finalize pass. A scan of every card with a
finalize pass or a reflexive trigger found no other case. The roughly 40 other effect-play location prompts were
already asked at resolution.

**Earlier decisions:**
- **B5 (batch 3) is reversed.** Zed was asked at resolution, which was correct. The convention B5 followed is what
  420.3.b.2 rules out. Its test now expects the prompt after the opponent's priority.
- **D66 (batch 4a) stays fixed.** The Recruit's place is still asked; only the moment changes.
- **9.GG2's three pins are reversed** (`SweepCostsTest.EffectPlayLocation_*`). A battlefield gained in response is
  now offered.

**Same root, fixed with it.** The lists of places for an effect play offered only the base and the battlefields the
player controls. But an effect play is a play ("Treat all steps of Play as normal", 420.3.b). So the played card's own
permissions, the player's grants and [Ambush] all apply (355.2.b, 822.1.c).
- `GameEngine::unitPlayableAt`, `ambushLocation` and `enemyUnitAloneAt` are now free functions in
  `engine/play_permissions.{h,cpp}`. The engine's generators and the card helpers share them.
- New card helpers: `effectPlayAllowedAt`, `playLocationsFor`, and a `unit` argument on `pickEffectPlayLocation`.
- Cards that now use them: Void Burrower, The Zero Drive, Rek'Sai, Promising Future, Void Rush, Heedless Resurrection,
  Last Rites, Baited Hook, Blind Fury, The Harrowing, Spectral Matron, Glasc Mixologist, Rumble, Dazzling Aurora,
  Wild Claw, Reinforce and Arcane Shift.
- In the smoke run this showed up as Dazzling Aurora offering Rengar, Trophy Hunter his [Ambush] battlefield (see
  section 5).

## 3. Verdicts

| item | verdict | what changed | tests (shown red first) |
|---|---|---|---|
| M5 | FIXED | section 2 | `Batch6Test.M5_*` (9) |
| R1 | FIXED | Lunar Boon's "drew" lines are private, and its prompt header no longer lists the hand. Stacked Deck's header named the 3 cards looked at; it was found by the same scan and fixed with it. | `HiddenOrderLeakCards.LunarBoon_*`, `.StackedDeck_*` |
| R2 | FIXED | "Base Might becomes 5 this turn" is a trait-altering effect (CR 143.2.a, which uses this card as its example; 479.2). New field `GameObject::base_might_this_turn`, read through `baseMight()`. It clears at the Expiration Step and when the object leaves the board. | `Batch6Test.R2_*` (3) |
| R3 | FIXED | Safety Inspector's two kills are one `DeathBatch` (CR 370.1.a.2). | `R3_SafetyInspector_TheTwoUnitsDieTogether`: Nasus no longer sees the other unit die |
| R4 | FIXED | Acceptable Losses: both players choose (`selectObjectAs` over `boardObjects`), then one `DeathBatch`. | `R4_*` |
| R5 | FIXED | Iterative Design, Vanguard Armory, Recruit the Vanguard and Guards! use `tokenPlayDests` / `pickTokenDest`, and pass `location_choices` (Zilean's copy). | `R5_*` (6) |
| R6 | FIXED | Wild Claw, Reinforce and Dazzling Aurora banish and play first, then recycle. Lightning Rush draws with `drawHeldCard`, then trashes the rest. The location lists are unit-aware, and an option's value is its `locationCode`. | `R6_*` (7) |
| R7 | FIXED | Cursed Sarcophagus checks only the activating copy (`canActivateAbilityAt`). `onPlay` forgets the old banished list (CR 124.1); The Zero Drive does the same. | `R7_*` (2) |
| R8 | FIXED | Siphon Power and Firestorm target the battlefield (CR 355.10.b, 355.10.d), chosen with `pickBattlefield` in the finalize pass. | `R8_*` (3) |
| R9 | FIXED | Whirlwind's picks are not targets (CR 355.10.e). "Can't be chosen" means untargetable (Vendetta patch notes), so it no longer filters them, and no Deflect is charged (809.1.c). | `R9_*` |
| R10 | FIXED | Back Off reads `Card::kPlayedNotFromHand`. | `R10_*` (2) |
| R11 | FIXED | Arcane Shift: the owner chooses where the unit is played (CR 355.2), using 5d's `pickValueAs`. | `R11_*` |
| R12 | FIXED | List items were run together in 12 cards' text (2 were listed). `Keyword::Predict` was on 6 defs (1 was listed); Predict is a game action (CR 437). | `R12_*` (2 registry-wide tests) |
| R13 | FIXED | New `Card::pickUnseenCard`: the same mechanics as pickTarget, but `PromptKind::ChooseObject`, no Deflect and no `commitChoice`. Used in all 10 listed cards. | `R13_*` (2; red with pickTarget) |
| R14 | QUESTION | Judge question 17: what is an "enemy battlefield"? The engine reads it two ways — Maduli: a battlefield with enemy units; Dauntless Vanguard and Deadbloom Predator: one the opponent controls. | `R14_*` (2 pins) |
| R15 | FIXED | `[[gnu::noinline]]` on clone_breakdown's replaced operator new and delete. The warning was a false report caused by inlining. | gate: 0 warnings |

**Red-first counts:**
- First run: 32 of the 40 new tests failed on the unchanged cards. The other 8 pin behaviour that stays.
- The Lightning Rush test as first written passed by mistake: it read the fixture's copy of the state. It was
  rewritten and shown red.
- The tests added later (Stacked Deck, R11, R13) were each shown red against the old code.

## 4. Tests changed by design (old -> new)

| test | change | reason |
|---|---|---|
| `SweepCostsTest.EffectPlayLocation_{FlameChompers,ImmortalPhoenix,Soulgorger}DoesNotAskAtResolution` | renamed `...AsksAtResolution`; the battlefield gained in response is now offered | M5 |
| `Batch3Test.B5_ZedChoosesWhereTheShadowCloneGoesBeforeAnyoneResponds` | renamed `...AsTheAbilityResolves` | M5 |
| `SweepCostsTest.UndyingLoyalty_*`, `C3Test.UndyingLoyalty_OnlyABirdCatDog...` | prompt order is now: the unit, then where | M5 |
| `ReviewFixC2Test.HandBuiltUnitChoicesSkipAnUnchoosableEnemyUnit` | Whirlwind part: the caster is now offered the unit | R9 |
| `ReviewFixC2Test.Whirlwind_ChoosingAnOpponentsDeflectUnitCostsPower` | renamed `...CostsNothing` | R9 |
| `VendettaE.Vendetta_DragonForm_BaseMightBecomesFiveThisTurn` | its simulated end of turn now clears `base_might_this_turn` | R2 |
| `CardTestFixture.BackOff_StunsAndDraws1` | the direct call now states that the card was played from a hand | R10 |
| `SweepTriggersTest.PromptLines_NamingHiddenCardsArePrivate` | looks for `PICK_PROMPT` | R13 |
| `StrongIds` audit, `PerfEngineSources` list, `PerfEngineDeathBatch` count (7 -> 9) | updated for the new code | — |

## 5. Gate (final tree, master `511c7d6` merged)

**Build:**
- Clean rebuild of `all` plus `deck_fuzz`, lane A, -j3. Our code has 0 warnings: R15 is fixed, and a GCC
  `-Warray-bounds` false report on a `resize` in the new Wild Claw code was removed with `erase`. The 8 remaining
  warnings are all in `_deps`.
- Checked build: 0 warnings.

**Suites:**
- `riftbound_tests`: 3,582 passed (1 disabled).
- `riftbound_openspiel_tests`: 34 passed.
- `clone_equiv`: 10/10.
- Checked build: 3,582 passed.
- Long leak run (`RIFTBOUND_LEAK_GAMES=300`): 5/5 pass.
  - Variants: 84,261 decisions.
  - Relabelled: 85,207 decisions, 689,857 log lines. 1 pair became different games by the table's order, which the
    test allows.
  - Passive opponent: 39,082 decisions.

**Smoke** (fiora vs yi_rebuild, 200 games, seed 20261010): 136 / 64 / 0 against master's 137 / 63 / 0.
- I traced both runs and found one divergence apart from the renamed prompt lines (TGT_ -> PICK_ for Rift Herald and
  Decree of Strength).
- In that game, Dazzling Aurora reveals Rengar, Trophy Hunter. Rengar's [Ambush] now offers the battlefield with
  enemy units (M5's permissions), and the random agent took it.

**MCTS** (seed 7, `mcts:sims=8`, 4 games):
- rc 0, P1 3 / P2 1.
- With `RIFTBOUND_STEP_FAULT_AT=40`: rc 1, "injected engine fault at decision 40", 0 draws.

**ISMCTS:** both fixed-seed commands, ten runs each. rc 0 every time, and each command's output was identical across
its ten runs.

**Pairing** (39 lists, 7 games per ordered pairing, 10,647 games): 0 failures on both builds. The two builds gave
identical results (P1 5,373 / P2 5,274).

**Fuzz, optimised, 3 × 100,000 with `--known`:** 0 failures.
- Seed 6150821, random agents.
- Seed 6150822, random agents.
- Seed 6150823, `random-player`.
- An extra 100,000 at seed 6150824 with `random-player:pass=0`: 0 failures, but one game hit the 200-turn cap.
  - I replayed it. It diverges from master because Firestorm can now be played with no enemy unit on the board (R8).
  - It then becomes the stall batch M1 already explained: Endless Riches skips every draw, both boards are empty, and
    the score sits at 7-2.

**Fuzz, checked, 3 × 20,000** (random, random-player, pass=0; seeds 6150831-3): 0 failures.

## 6. Throughput

1,000 games, 1 thread, fiora vs yi_rebuild, seed 20261010, under the run lock. Six alternating pairs against master
(5d's binary):

| | runs (games/s) | mean |
|---|---|---|
| master | 681.5, 671.2, 611.4, 665.3, 651.7, 633.1 | 652.4 |
| batch 6 | 668.6, 612.9, 594.0, 657.8, 652.1, 602.7 | 631.4 |

- The machine's load average was 4-5 during these runs (another lane was building), and one binary alone spread from
  611 to 681.
- So I measured instruction counts, which load does not affect (`perf stat -e instructions:u`, two runs each):
  - master: 10.373G and 10.333G;
  - batch 6: 10.363G and 10.384G.
- The two builds do the same work. The wall-clock gap is noise.
- `GameObject` gained one int (`base_might_this_turn`).

## 7. Not done as asked

Nothing was left out. Three points of method:
- The tests are committed in one commit after the source commits, not with each mechanism. Each source commit names
  its tests.
- R13 and R11 were finished just before and with the 5d merge, so they use its `PromptKind` and `pickValueAs`.
- The master binary used for the smoke and throughput comparisons is batch 5d's build. It was built after 5d's last
  source commit, and master's merge of 5d changed no source.

## 8. Trainer notes

- **Observation:** no layout change. For a unit under Dragon Form, `temp_might_bonus` now reads 0 where it read
  `5 - base`. The "Might minus base" input is unchanged.
- **Prompt kinds:**
  - The ten hidden-zone picks are now `ChooseObject`, labelled "pick:" (they were `Target`).
  - Effect-play locations are now asked at resolution, as `Location` prompts.
  - Siphon Power and Firestorm ask a battlefield prompt (`Target` kind, one option per battlefield) instead of a unit
    target.
  - Arcane Shift's location is now asked of the unit's owner.
- **What is offered:** effect plays can now offer battlefields the card's own permissions allow. Siphon Power and
  Firestorm are offered with no unit on the board. Whirlwind offers [Deflect] and "can't be chosen" units for free.
- **Card text:** the text of 12 cards changed (line breaks only), and `Keyword::Predict` is gone from 6 defs. This
  matters only to consumers of `--dump-registry`.

## 9. New defects found (not fixed)

- **S1** Thermo Beam ("Kill all gear") kills the gear one after another, with no `DeathBatch`. They do not die
  together (CR 370.1.a.2, 383.2.c.2). Recorded in `plans/remaining/10-testing-phase.md`.
