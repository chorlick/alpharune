# 3. Card bugs

Paths are under `src/cards/` unless noted.

Two engine facts drive several findings:

- **One shared `resume_point`.** All choice helpers share a single integer. A helper called after a higher-slot helper has finished sees a value past its own range and returns a padded default without asking.
- **Triggers get no targets** unless the card opts into `kFinalizeTrigger` (`engine/game_engine.cpp:1192-1198`; `trigger_manager.cpp:372` says "triggers receive empty targets").

## A. Choice helpers only compose in ascending slot order

- [x] **3.1 Helper ordering bug in `Card`** [verified] — FIXED: helper flags and all seven cards verified in the code by sweep-verify-cards (745, 258, 490, 695, 263, 760, 607: no state change between two helper calls); test: ChoiceHelperTest.EveryOrderOfTheHelpersAsksEachOnceAndKeepsEachAnswer, EnginePathTest.ThrillOfTheHuntBanishesItsUnitOnce, EnginePathTest.ZenithBladeStunsOnce, ReviewFixC1Test.BlastCone_TheControllerChoosesTheUnitAndWhereItGoes, ReviewFixC1Test.ForgottenSignpost_ExhaustsOneUnitAndMovesADifferentOne, C3Test.KeepersVerdict_TheOwnerChoosesTopOrBottom
  - Where: `card.cpp:124-136` (`confirmOptional`), `:200-202` (`pickXAmount`), `:292-294` (`pickMode`), `:447-462` (`pickTarget`). `card.h:338-340` and `:378-380` say the helpers coexist.
  - Affected cards:
    - `spells/0745_thrill_of_the_hunt.cpp:24,59` — `pickMode` after `pickTarget` returns `resume_data[1] == 0`; the unit always goes to `battlefields[0]`.
    - `spells/0258_zenith_blade.cpp:37,60` — `confirmOptional` after `pickTarget` returns 1; "you may move" is forced and moves the first unit in map order.
    - `gear/0490_altar_of_memories.cpp:45,48` — always top of deck, never bottom.
    - `gear/0695_blast_cone.cpp:64` — second `confirmOptional` reuses slot 0-2 and returns 1; always exhausts to stun.
  - Latent: `spells/0263_bullet_time.cpp`, `spells/0760_keeper_s_verdict.cpp`, `gear/0607_forgotten_signpost.cpp` have the same ordering but take the first target from `targets[0]`. Bullet Time's comment at lines 22-25 ("they don't collide") is wrong.
  - Latent: `pickXAmount` stores X with `push_back` (`card.cpp:157,195`) but reads `resume_data[0]`; the engine pre-sizes `resume_data` to 6 for play-time targets (`game_engine.cpp:1256,1935`).
  - Fix direction: per-helper completion flags instead of one ordered integer, so helpers compose in any order.
  - HELPER FIXED (stream helpers): `Card::confirmOptional` / `pickXAmount` / `pickMode` / `pickTarget` / `pickTargetPair` keep per-helper waiting/answered flags (`resume_data[Card::kChoiceStateSlot]`; `confirmOptional` uses `may_answer`) and no longer read each other's progress from `resume_point`; `pickXAmount` stores X in `resume_data[0]`. `resume_point` still gets the documented values (1/2, 4/5, 7/8, 10/12/13). Tests: `ChoiceHelperTest.EveryOrderOfTheHelpersAsksEachOnceAndKeepsEachAnswer`, `.ConfirmOptionalAfterPickTargetCanBeDeclined`, `.PickXAmountAfterAPlayTimeTargetKeepsX`, `.HelpersLeaveTheDocumentedResumePoints`.
  - Per card, with no card edit: Altar of Memories — bottom is choosable (`EnginePathTest.DeathTriggerWithMayTargetAndModeResolvesThroughTheEngine`); Zenith Blade — the move can be declined (`EnginePathTest.SpellMayAfterTargetCanBeDeclinedThroughTheEngine`); Bullet Time — X is asked (`EnginePathTest.SpellTargetThenXIsAskedThroughTheEngine`); Keeper's Verdict — top/bottom is asked (`EnginePathTest.SpellTargetThenModeIsAskedThroughTheEngine`); Forgotten Signpost unchanged (see below).
  - Fixed by stream helpers on the orchestrator's instruction (the helper change made both run a state change once per pass): `spells/0745_thrill_of_the_hunt.cpp` asks the battlefield before banishing (test: `EnginePathTest.ThrillOfTheHuntBanishesItsUnitOnce`); `spells/0258_zenith_blade.cpp` stuns after the "may" is answered (test: `EnginePathTest.ZenithBladeStunsOnce`). Both tests were run red against the un-fixed card first. Blast radius checked: the only callbacks where a helper follows a higher-slot helper are 490, 258, 263, 745, 760; 490, 263 and 760 change no state between the two.
  - STILL OPEN, card streams (cards are not the helpers stream's files):
    - `gear/0695_blast_cone.cpp` — one `confirmOptional` per callback: the second call returns the first answer by design. "When you move an enemy unit, you may exhaust this to stun it" is its own sentence: make it a second trigger, or ask it with a two-mode `pickMode`.
    - `gear/0607_forgotten_signpost.cpp` — two `pickTarget` calls in one callback: the second returns the first pick. Use `pickTargetPair`.
    - `spells/0263_bullet_time.cpp` — delete the "they don't collide" comment; pay with `ctx.executor.payAnyPower` (03 item 3.24).
    - Rule for every card: a helper that follows another helper now suspends, so nothing between two helper calls may change game state unless it is guarded to run once.
  - c2: 258 Zenith Blade — ALREADY FIXED by stream helpers (`f892d67`: the stun runs after the "may" is answered); re-read and kept while fixing 7.24 (the stun now runs after the unit prompt too); tests still green: EnginePathTest.ZenithBladeStunsOnce, EnginePathTest.SpellMayAfterTargetCanBeDeclinedThroughTheEngine. 263 Bullet Time — the wrong "they don't collide" comment is deleted.
  - c1: 490 Altar of Memories — ALREADY FIXED by the helper change (bottom is choosable), run-once draw guard fixed under 3.7; 695 Blast Cone — FIXED: the stun is its own triggered ability (`WhenYouMoveAnEnemyUnit`) with its own `confirmOptional`; 607 Forgotten Signpost — FIXED: `pickTargetPair` in the finalize pass (`kFinalizeActivate`); test: ReviewFixC1Test.BlastCone_TheControllerChoosesTheUnitAndWhereItGoes, ReviewFixC1Test.BlastCone_StunsAUnitMovedByAnotherOfYourAbilities, ReviewFixC1Test.ForgottenSignpost_ExhaustsOneUnitAndMovesADifferentOne
  - c3: 745 Thrill of the Hunt — ALREADY FIXED (2e0f181); 760 Keeper's Verdict — FIXED: top/bottom is now asked of the unit's owner by a hand-built prompt, no state change before it; test: C3Test.KeepersVerdict_TheOwnerChoosesTopOrBottom

## B. Triggers that act only on `targets`

- [x] **3.2 Triggered abilities whose printed effect never happens** — **superseded by [07 §A](07-cards-second-pass.md)**, which read every file: 26 BROKEN, 3 PARTIAL (Spirit's Refuge's aura works). Original entry kept for reference. — FIXED: all 26 cards plus 63 / 695 / 744 verified in the code by sweep-verify-cards (see 07 items 7.1 and 7.2 for the tests)
  - Tests pass because the fixture calls `onTrigger(ctx, targets)` directly.
  - `units/0092_riptide_rex.cpp:18` — "When you play me, deal 6" does nothing.
  - `units/0016_dangerous_duo.cpp:19`, `units/0238_leona_determined.cpp:18`, `units/0539_ahri_inquisitive.cpp:18`, `units/0199_tideturner.cpp:35`.
  - Full list: Navori Fighting Pit 278, Veiled Temple 535, Amateur Recital 763, Spirit's Refuge 63, Forge of the Future 212, and units 16, 51, 76, 82, 92, 97, 130, 132, 188, 191, 199, 234, 238, 330, 361, 384, 453, 479, 539, 589, 618, 738.
  - Fix direction: either convert each to `pickTarget` + `kFinalizeTrigger`, or have the engine choose targets at finalization for any trigger with `getTargetRequirements().count > 0`. Add an engine-level test that plays the card rather than calling `onTrigger`.
  - c0: 278 Navori Fighting Pit, 535 Veiled Temple, 763 Amateur Recital, 63 Spirit's Refuge, 212 Forge of the Future — FIXED: see 07 item 7.1 / 7.2
  - c4: 16, 51, 76, 82, 92, 97, 130, 132, 188, 191, 199 — FIXED: see 07 item 7.1.
  - c5: 238 Leona, Determined — FIXED under 07 item 7.1.
  - c6: 539 Ahri, Inquisitive — FIXED: see 07 item 7.1

## C. Non-idempotent work before a suspending helper

The callback re-runs from the top after a choice, so anything before the helper happens twice.

- [x] **3.3** `units/0200_twisted_fate_gambler.cpp:45-46` — pops and recycles the top rune before `pickTarget` (`:64`, `:92`); a second rune is revealed on re-entry and the payoff re-derived. [confirmed] — FIXED: verified in the code by sweep-verify-cards; test: C4Test.TwistedFateGambler_RevealsOneRuneHoweverOftenItIsReEntered
  - c4: 200 Twisted Fate, Gambler — FIXED: the reveal-and-recycle runs once; the rune's domains are kept on the resolving chain item (resume_data slot 7) for the re-entry after `pickTarget`; test: C4Test.TwistedFateGambler_RevealsOneRuneHoweverOftenItIsReEntered
- [x] **3.4** `spells/0515_arcane_shift.cpp:25-31` — banishes and replays `targets[0]` before `pickTarget` (`:44`); play triggers fire twice. [confirmed] — FIXED: verified in the code by sweep-verify-cards; test: C3Test.ArcaneShift_ReplaysTheUnitOnceAndBanishesItself
  - c3: 515 Arcane Shift — FIXED: the enemy target and the replay location are chosen before the banish-and-replay, so it happens once; "Banish this" sets `banish_on_leave_chain`; test: C3Test.ArcaneShift_ReplaysTheUnitOnceAndBanishesItself
- [x] **3.5** `units/0406_jayce_man_of_progress.cpp:50-52` — `killObject(gear)` before `pickTarget` (`:59`); with two gears both die, with one the picked gear is never played. [confirmed] — FIXED: verified in the code by sweep-verify-cards; test: C5Test.JayceKillsTheChosenGearAndGrantsAPlayThisTurn
  - c5: 406 Jayce, Man of Progress — FIXED: nothing is killed before the choices; the gear is chosen with `pickTarget` at finalization and killed after the marker; test: C5Test.JayceKillsTheChosenGearAndGrantsAPlayThisTurn
- [x] **3.6** `gear/0242_baited_hook.cpp:33-36` — kills `targets[0]` then suspends (`:59`); on re-entry returns early, the chosen unit is never played. [confirmed] — FIXED: verified in the code by sweep-verify-cards; test: C0Test.BaitedHook_PaysPowerStaysOnBoardAndPlaysThePickedUnit
  - c0: 242 Baited Hook — FIXED: the kill is recorded on the chain item (`resume_data[7]` = 1 + the killed unit's Might) so the re-entry after the pick neither kills again nor returns early; the picked unit is played; test: C0Test.BaitedHook_PaysPowerStaysOnBoardAndPlaysThePickedUnit
- [x] **3.7** `gear/0490_altar_of_memories.cpp:35-38` — once-guard `__altar_drew` is never erased; from the second friendly death "yes" neither exhausts nor draws but still forces a hand card onto the deck. `spells/0475_guards.cpp:25-33` has the same permanent guard (`__guards_token`) [plausible]. [confirmed] — FIXED: 490 and 475 verified in the code by sweep-verify-cards; the stage 2 Guards! test never reached the "may" (no Order Power), so the re-entry after the token exists was untested — added; test: ReviewFixC1Test.AltarOfMemories_WorksOnEveryDeathNotOnlyTheFirst, VerifyCardsTest.Guards_AcceptingPaysOrderPowerAndReadiesTheOneSandSoldier, VerifyCardsTest.Guards_WithoutOrderPowerTheMayIsNotOffered
  - c1: 490 Altar of Memories — FIXED: the permanent `__altar_drew` counter is gone; the exhaust is a cost paid at finalization and the draw is guarded per resolving chain item (`resume_data` slot 7); test: ReviewFixC1Test.AltarOfMemories_WorksOnEveryDeathNotOnlyTheFirst (475 Guards! is not in c1's range)
  - c3: 475 Guards! — FIXED: the token id is kept on the resolving chain item, not on the card; test: C3Test.Guards_ALaterCastOfTheSameCardMakesANewToken

## D. Choices at finalization

- [ ] **3.8** `legends/0556_chem_baroness.cpp:23-27` — `still_legal` is `return true` and the exhaust cost has no ready check; two holds make two Gold. [confirmed]
  - c1: 556 Chem-Baroness — FIXED: `still_legal` requires a ready legend and the exhaust goes through `exhaustObject` under `costDueNow`; test: ReviewFixC1Test.ChemBaroness_OnlyAReadyLegendCanPay
  - verify-cards: needs sweep-verify: Chem-Baroness 556 is being edited by sweep-flow (effective Victory Score); not read. Verify `still_legal` and the exhaust cost after it merges, then tick.
- [x] **3.9** `units/0147_wildclaw_shaman.cpp:24-33` — "spend a buff" never spends one; free buff and ready. [confirmed] — FIXED: verified in the code by sweep-verify-cards; test: C4Test.WildclawShaman_SpendsABuffToBuffAndReadyItself, C4Test.WildclawShaman_WithNoBuffToSpendIsNotOffered
  - c4: 147 Wildclaw Shaman — FIXED: offered only with a buffed friendly unit; the controller picks whose buff and it is spent at finalization (`costDueNow`); test: C4Test.WildclawShaman_SpendsABuffToBuffAndReadyItself, C4Test.WildclawShaman_WithNoBuffToSpendIsNotOffered
- [x] **3.10** `units/0449_overzealous_fan.cpp:40,51-52` — "kill me" cost paid after the marker and never verified; if the Fan was killed in response, the effect still happens. The effect itself is also wrong: the code bounces the attacker to hand, the text says move it to its base (08 §D). [confirmed] — FIXED: verified in the code by sweep-verify-cards; test: C5Test.OverzealousFanMovesTheAttackerToItsBase, VerifyCardsTest.OverzealousFan_IsDeadBeforeTheOpponentCanRespond
  - c5: 449 Overzealous Fan — FIXED: the kill is paid under `costDueNow` by a Fan still on the board (no effect otherwise); the attacker is moved to its base; test: C5Test.OverzealousFanMovesTheAttackerToItsBase
- [x] **3.11** `units/0230_albus_ferros.cpp:28-44` — X fixed at finalization, buffs spent at resolution, payout uses X. [confirmed] — FIXED: verified in the code by sweep-verify-cards; test: C5Test.AlbusFerrosSpendsTheBuffsThePlayerChooses
  - c5: 230 Albus Ferros — FIXED: no X at finalization; buffs are chosen and spent at resolution and the runes follow the count actually spent; test: C5Test.AlbusFerrosSpendsTheBuffsThePlayerChooses
- [x] **3.12 Costs paid after the marker without `costDueNow`: 14 cards** [confirmed pattern] — **complete named list in [07 §C](07-cards-second-pass.md)**. — FIXED: all six cards verified in the code by sweep-verify-cards, and no card that opts into finalization (212 files grepped) pays a cost after its marker without `costDueNow`; the stage 2 tests of 446, 349 and 449 asserted only the end state, so a probe at the opponent's first priority was added for each; test: C6Test.FioraWorthy_PaysTheOrderPowerBeforeAnyoneCanRespond, C7Test.BaccaiReaper_PaysFuryByRecyclingAnExhaustedRuneUpFront, C6Test.KhaZix_SpendsTheXpBeforeAnyoneCanRespond, VerifyCardsTest.FaePorter_ThePowerIsGoneBeforeTheOpponentCanRespond, VerifyCardsTest.RumbleHotheaded_TheUnitIsRecycledBeforeTheOpponentCanRespond, VerifyCardsTest.OverzealousFan_IsDeadBeforeTheOpponentCanRespond
  - Payment happens at resolution, so a player can say yes then spend the resource on a Reaction (CR 383.3).
  - Clearest: `units/0500_fiora_worthy.cpp:41`, `units/0446_fae_porter.cpp:49`, `units/0796_baccai_reaper.cpp:57`.
  - Rest: in each opted-in file, grep after the `finalizingChoices(ctx)` line for `payOnePower|payEnergy|xp -= |buff_count -= 1|burn\(|killObject\(ctx.source\)`.
  - c5: 446 Fae Porter, 349 Rumble, 449 Overzealous Fan — FIXED (see 07 item 7.5); 230 Albus Ferros is not a cost (03 item 3.11).
  - c6: 500 Fiora, Worthy, 681 Kha'Zix, Evolving Hunter — FIXED: see 07 item 7.5
  - c7: 796 Baccai Reaper — FIXED, see 07 item 7.5
- [ ] **3.13 Counter writes before the marker: 4 cards** [confirmed violation, plausible harm]
  - `units/0162_miss_fortune_captain.cpp:34`, `units/0789_blade_twirler.cpp:31`, `units/0855_jayce_brilliant_inventor.cpp:42,47`, `units/0372_aphelios_exalted.cpp:33-44`. After `makeNewChoicesFor` resets `resume_point`, the "first time this turn" flag is already set.
  - c4: 162 Miss Fortune, Captain — FIXED: no counter at all; "the first time I move each turn" is `triggerCondition` (`moves_this_turn == 1`, bumped before every move event); test: C4Test.MissFortuneCaptain_FirstMoveOnly_AndLegendsCount
  - c5: 372 Aphelios, Exalted — FIXED: the mode is still claimed when chosen (a second trigger must not take it), but the claim remembers its chain item, so the same item asked again (re-entry, `makeNewChoicesFor`) is not blocked by its own answer; test: C5Test.ApheliosModeCannotBeChosenTwiceInATurn
  - c7: 789 Blade Twirler — FIXED: no counter; `triggerCondition` (`moves_this_turn == 1`); 855 Jayce, Brilliant Inventor — FIXED: "first non-token gear" is `triggerCondition`; the stamp is written idempotently and no path returns on it, so a re-chosen ability is not lost; test: C7Test.BladeTwirler_OnlyTheFirstMoveOfTheTurnBurns, C7Test.JayceBrilliantInventor_OnlyTheFirstNonTokenGearOfTheTurn
  - verify-cards: 162, 789, 855 verified in the code by sweep-verify-cards (no counter write before the marker that a path returns on). needs sweep-verify: (a) Aphelios 372 is with sweep-costs — verify after it merges; (b) 855 Jayce has no test that re-enters `onTrigger` after `makeNewChoicesFor` (the original defect) — add one. Then tick.

Clean: opt-in bits match overridden callbacks; every effect branch of the 10 multi-trigger cards has a marker; no pre-check is missing `costAlreadyPaid`.

## E. Objects that left play

- [x] **3.14 Self-move on `ctx.source` guarded only by `objectExists`: 9 sites** [confirmed] — FIXED: verified in the code by sweep-verify-cards — `EffectExecutor::moveToBase` / `moveToBattlefield` / `bounceToHand` refuse an object with no board location, and every grep hit (404, 67, 373, 35, 537, 199, 177, 447, 706, 617, 899) is covered by that or by its own check; 404, 67 and 373 had no test of the source leaving the board — added; test: ExecutorReviewFixTest.OffBoard_DeadUnitIsNotMovedBouncedBuffedOrStunned, VerifyCardsTest.EzrealDashing_ActivatedThenKilledIsNotMovedToBase, VerifyCardsTest.Blitzcrank_HoldReturnsHimToHandOnlyFromTheBoard, VerifyCardsTest.AzirAscendant_KilledInResponseSwapsNobody
  - `objectExists` is true for every id ever created, so a source killed in response is moved back from trash.
  - `units/0404_ezreal_dashing.cpp:65-66`, `units/0067_blitzcrank_impassive.cpp:31` (card ends up in hand and trash), `units/0373_azir_ascendant.cpp:102-112`.
  - Rest: `grep -nE "(moveToBattlefield|moveToBase|bounceToHand|moveToLocation)\((ctx\.executor, *)?(ctx\.source|source)\b" src/cards/*/*.cpp`
  - c4: 67 Blitzcrank, Impassive — FIXED: the hold bounce checks `location.has_value()` (and `bounceToHand` now refuses an off-board unit, 02 item 2.28).
  - c5: 404 Ezreal, Dashing — ALREADY FIXED by the executor (`99d179e`: `moveToBase` is a no-op off the board). 373 Azir, Ascendant — FIXED: both units must be on the board to trade places.
- [ ] **3.15 Trigger subject used after only `objectExists`** [plausible, low impact]
  - `units/0712_vex_apathetic.cpp:30`, `gear/0143_pirate_s_haven.cpp:26`, `battlefields/0272_back_alley_bar.cpp:26`, `gear/0060_mask_of_foresight.cpp:27`.
  - c0: 143 Pirate's Haven, 272 Back-Alley Bar, 60 Mask of Foresight — FIXED: the subject must still be a unit on the board; test: C0Test.PiratesHaven_ReadyingARuneDoesNotTrigger, .BackAlleyBar_OnlyAUnitStillOnTheBoardGetsTheMight, .MaskOfForesight_IgnoresAUnitThatLeftTheBoard
  - c7: 712 Vex, Apathetic — FIXED: the played unit must still be an enemy unit on the board; test: C7Test.VexApathetic_StunsTheUnitAnOpponentPlays
  - verify-cards: 143 and 60 verified in the code by sweep-verify-cards; 143 had no test of the subject leaving the board — added (VerifyCardsTest.PiratesHaven_AUnitThatLeftTheBoardGetsNoMight). needs sweep-verify: Vex 712 (sweep-flow) and Back-Alley Bar 272 (sweep-triggers) after they merge, then tick.
- [x] **3.16** `units/0416_direwing.cpp:21` — "Another Dragon" skips every Direwing by def id; a second Direwing does not enter ready. Use the three-argument `entersReadyOnPlay(..., self)`. [confirmed] — FIXED: 416 and 394 verified in the code by sweep-verify-cards; the stage 2 tests called `entersReadyOnPlay` directly — engine-path tests added; test: VerifyCardsTest.Direwing_ThroughTheEngine_EntersReadyOnlyNextToAnotherDragon, VerifyCardsTest.BreakneckMech_ThroughTheEngine_EntersReadyOnlyNextToAnotherMech
  - c5: 416 Direwing — FIXED: three-argument `entersReadyOnPlay`, skips only this instance (same in 394 Breakneck Mech); test: C5Test.DirewingEntersReadyNextToAnotherDirewing

## F. Data and reprint drift

- [x] **3.17** `gear/0542_seal_of_strength.cpp:25` — adds `Domain::Order`; reprint 163 adds `Domain::Body`. [verified] — FIXED: 542 verified in the code by sweep-verify-cards; 163 pinned; test: ReviewFixC1Test.SealOfStrength_AddsBodyPower, VerifyCardsTest.Pin_3_17_SealOfStrength163_AddsBodyPower
  - c0: 163 Seal of Strength — NOT A DEFECT: `gear/0163_seal_of_strength.cpp` adds `Domain::Body`, matching its text "[Add] [O]" and its Body domain; the wrong one is 542 (c1); pinned by VerifyCardsTest.Pin_3_17_SealOfStrength163_AddsBodyPower; clarified in gear/0163_seal_of_strength.cpp
  - c1: 542 Seal of Strength — FIXED: adds `Domain::Body`, as reprint 163 does; test: ReviewFixC1Test.SealOfStrength_AddsBodyPower
- [x] **3.18** `units/0778_plundering_poro.cpp:18` — `createGoldToken` (ready); text says exhausted, reprint 392 uses `createGoldExhausted`. [confirmed] — FIXED: 778 verified in the code by sweep-verify-cards; 392 pinned; test: C7Test.PlunderingPoro_TheGoldEntersExhausted, VerifyCardsTest.Pin_3_18_PlunderingPoro392_TheGoldEntersExhausted
  - c5: 392 Plundering Poro — NOT A DEFECT: 392 already calls `createGoldExhausted` (`units/0392_plundering_poro.cpp`, `onTrigger`); the defect is in reprint 778 (stream c7).; pinned by VerifyCardsTest.Pin_3_18_PlunderingPoro392_TheGoldEntersExhausted; clarified in units/0392_plundering_poro.cpp
  - c7: 778 Plundering Poro — FIXED: `createGoldExhausted`; test: C7Test.PlunderingPoro_TheGoldEntersExhausted
- [x] **3.19** `units/0548_karma_channeler.cpp:15-30` — only a `WhenYouPlayMe` Vision (the engine already does Vision, `trigger_manager.cpp:317`, so it runs twice) and no recycle trigger. `units/0235_karma_channeler.cpp:57` has tags `{"Vi", "Ionia"}`. [confirmed] — FIXED: 548 and 235 verified in the code by sweep-verify-cards (235's `Karma, Ionia` deliberately departs from the gallery data, which says `Vi`: printed card still to be confirmed by the user); test: C6Test.Karma548_BuffsAChosenUnitWhenACardIsRecycled, C5Test.KarmaChanneler235IsAKarma, VerifyCardsTest.Pin_3_21_ReprintsCarryTheTagsOfTheirOwnPrinting
  - c5: 235 Karma, Channeler — FIXED: tags `{Karma, Ionia}`; test: C5Test.KarmaChanneler235IsAKarma. (The gallery data in `cards/raw/gallery_raw.json` carries "Vi" for OGN-235, which is where the error came from; the SFD reprint says Karma.) 548 is stream c6's.
  - c6: 548 Karma, Channeler — FIXED: `WhenYouRecycle` + `pickTarget` + `kFinalizeTrigger` as 235, hand-rolled Vision removed; 548's tag `Karma` matches the printed SFD-237/221 (235's tags are cards-c5's; `cards/raw/gallery_raw.json` lists OGN-235/298 as `Vi, Ionia` too); test: C6Test.Karma548_BuffsAChosenUnitWhenACardIsRecycled
- [x] **3.20** `units/0543_sett_brawler.cpp` lacks the `canActivateAbility` buff gate that `units/0164_sett_brawler.cpp:27` has. [confirmed] — FIXED: verified in the code by sweep-verify-cards (543 is line for line 164); test: C6Test.Sett543_NeedsItsOwnBuffToActivate, C6Test.Sett543_SpendsTheBuffAsTheAbilityIsActivated
  - c6: 543 Sett, Brawler — FIXED: same code as 164 (per-instance gate, buff spent at activation); test: C6Test.Sett543_NeedsItsOwnBuffToActivate, .Sett543_SpendsTheBuffAsTheAbilityIsActivated
- [ ] **3.21** Region tags differ in 5 reprint pairs (Ahri 539/119, Darius 547/243, Sett 164/543, Yasuo 205/546, Vayne 35/537). `units/0421_veteran_poro.cpp` sets `Keyword::Equip`, 779 does not. [confirmed]
  - c4: 119, 164, 205, 35 — NOT A DEFECT on these ids: they are the copies that carry the region tag (Ionia / Demacia, as `cards/json/ogn_*.json`); the reprints missing it (539, 543, 546, 537) are outside this range.; pinned by VerifyCardsTest.Pin_3_21_ReprintsCarryTheTagsOfTheirOwnPrinting; clarified in the reprint files (units/0539, 0543, 0546, 0537: a comment at `d.tags` naming both printings)
  - c5: 421 Veteran Poro — FIXED: `Keyword::Equip` removed (a Weaponmaster unit has no Equip ability; 779 was right), and from the other Weaponmaster units in range (324, 331, 407, 414, 435, 440, 448); test: C5Test.KeywordsTheTextOnlyMentionsAreNotOnTheDef. 243 Darius's region tags are unchanged (reprint 547 is c6's).
  - c6: 539 Ahri, 543 Sett, 546 Yasuo, 547 Darius, 537 Vayne — NOT A DEFECT: each def matches its own printing in `cards/raw/gallery_raw.json` (SFD-227 `Ahri`; SFD-232 `Sett`; SFD-235 `Yasuo`; SFD-236 `Trifarian, Darius`; SFD-223 `Vayne, Demacia, Sentinel`) — the Spiritforged reprints print fewer region tags than the Origins cards; pinned by VerifyCardsTest.Pin_3_21_ReprintsCarryTheTagsOfTheirOwnPrinting; clarified in units/0539_ahri_inquisitive.cpp, 0543_sett_brawler.cpp, 0546_yasuo_windrider.cpp, 0547_darius_executioner.cpp, 0537_vayne_hunter.cpp
  - c7: 779 Veteran Poro — NOT A DEFECT: [Weaponmaster] is its only keyword (def text "[Weaponmaster]"); 421 no longer sets `Keyword::Equip` either, so the two match; pinned by VerifyCardsTest.Pin_3_21_VeteranPoro779_HasWeaponmasterAndNoEquip; clarified in units/0779_veteran_poro.cpp
  - verify-cards: every def checked against `cards/raw/gallery_raw.json` (and `cards/json/ogn_*.json` for Origins): all ten match their own printing; 779 has no `Keyword::Equip`. Pinned by VerifyCardsTest.Pin_3_21_ReprintsCarryTheTagsOfTheirOwnPrinting and VerifyCardsTest.Pin_3_21_VeteranPoro779_HasWeaponmasterAndNoEquip; clarified in the six reprint files and in units/0779_veteran_poro.cpp. needs sweep-verify: Veteran Poro 421 is with sweep-costs (Weaponmaster) — confirm it still carries no `Keyword::Equip` after the merge, then tick.
- [x] **3.22** `spells/0251_noxian_guillotine.cpp:35` — without Legion writes `__kill_on_next_damage_this_turn`, which nothing reads; the spell does nothing. [confirmed] — FIXED: verified in the code by sweep-verify-cards; test: ReviewFixC2Test.NoxianGuillotine_WithoutLegion_KillsTheUnitTheNextTimeItIsDamaged, ReviewFixC2Test.NoxianGuillotine_WithLegion_KillsNow
  - c2: 251 Noxian Guillotine — FIXED: a one-shot delayed ability (`TriggerType::WhenAUnitTakesDamage`, `shared:` commit) kills the chosen unit the next time it is dealt damage this turn; test: ReviewFixC2Test.NoxianGuillotine_WithoutLegion_KillsTheUnitTheNextTimeItIsDamaged

## G. Shared helpers

- [x] **3.23 `payOnePower` bypasses the engine's power accounting** [confirmed] — FIXED: `payOnePower` recycles through new `recycleRuneForPower` (rune ready, `power_spent_this_turn`, `LeftBoardEvent`) and uses `EffectExecutor::payAnyPower` for [A]; `standardEquip` (both overloads), `SimpleEquipGear` and `UniversalEquipGear` pay through `payEnergy` + `payOnePower`, all-or-nothing, both ends on board; test: `ChoiceHelperTest.PayOnePowerRecyclesARuneWithTheEnginesBookkeeping`, `ChoiceHelperTest.EquipCostsCountAsPowerSpentAndAreAllOrNothing`
  - Where: `card_helpers.h:45-73`, `gear/equip_base.h:57-64`. No `power_spent_this_turn`, no `LeftBoardEvent`, no `is_exhausted` clear (engine path: `effect_executor.cpp:413-423`). Sivir, Mercenary (464) still reads 0 after two Equip costs.
- [x] **3.24 Copy-pasted helpers** [confirmed] — FIXED: verified in the code by sweep-verify-cards — no local `splitDamage` / `kSplit*` / `discardThenAct` left, and no `rune_deck.insert` outside `card_helpers.h` (263 Bullet Time pays with `payAnyPower`); 751 and 41 had only direct-call tests — engine-path tests added; test: VerifyCardsTest.AlphaStrike_ThroughTheEngine_AllTheDamageWhereTheControllerPutsIt, VerifyCardsTest.VolibearFurious_ThroughTheEngine_AllTheDamageWhereTheControllerPutsIt, C5Test.CorruptEnforcerDiscardsTheChosenCard, ReviewFixC2Test.BulletTime_XCountsReadyRunesAndPaysThroughTheEngine
  - `splitDamage` + four `kSplit*` constants duplicated in `spells/0751_alpha_strike.cpp:43-52` and `units/0041_volibear_furious.cpp:43-52` (compile only because the directories are separate unity targets).
  - `units/0444_corrupt_enforcer.cpp:35` re-implements `discardThenAct`.
  - 5 files hand-roll rune recycling instead of `payOnePower`.
  - HELPER DONE (stream helpers): `card_helpers.h` now has `splitDamageAmong` + `kSplitDamage*` (the two copies, verbatim, under a new name so it cannot collide with them) and `recycleRuneForPower`; test: `ChoiceHelperTest.SplitDamageAmongLetsTheControllerDivideTheDamage`.
  - STILL OPEN, card streams — recipe:
    - `spells/0751_alpha_strike.cpp`, `units/0041_volibear_furious.cpp`: delete the local `splitDamage` and the four `kSplit*` constants; call `splitDamageAmong(ctx, label, total, source, legal, damaged)` (same signature and behaviour).
    - `units/0444_corrupt_enforcer.cpp`: delete the private `discardThenAct`; call `discardThenAct(ctx, 1, label, [](CardContext&) {})` from `card_helpers.h`.
    - `gear/0405_hextech_anomaly.cpp`, `gear/0412_the_zero_drive.cpp`, `gear/0498_blade_of_the_ruined_king.cpp`, `gear/0748_hextech_gauntlets.cpp`, `spells/0263_bullet_time.cpp`: replace every hand-written `rune_deck.insert(...)` recycle with `payOnePower(ctx, player, domain)` (one Power of a Domain; `std::nullopt` = [A]), `ctx.executor.payAnyPower(player, n)` (n [A]), or — where the card picks the rune itself — `recycleRuneForPower(ctx, player, rune)`. Check affordability first with `payableOnePower` / `ctx.executor.payableAnyPower`. An Equip that is "[N] + one Power" is `standardEquip(ctx, ctx.source, unit, N, domain)`.
  - c4: 41 Volibear, Furious — FIXED: local `splitDamage` and `kSplit*` deleted, calls `splitDamageAmong`; test: existing Volibear tests and ChoiceHelperTest.SplitDamageAmongLetsTheControllerDivideTheDamage
  - c1: 405 Hextech Anomaly, 412 The Zero Drive, 498 Blade of the Ruined King, 748 Hextech Gauntlets — FIXED: no hand-written rune recycle left (`payAnyPower` / `standardEquip` / `payOnePower`); test: ReviewFixC1Test.HextechAnomaly_IsAReactionAndReadyRunesCanPayX, AuditFix0Test.ZeroDrive_EquipPaysAndAttaches, ReviewFixC1Test.BladeOfTheRuinedKing_TheControllerChoosesWhichUnitDies, ReviewFixC1Test.HextechGauntlets_EquipIsPaidWithFloatingResources
  - c3: 751 Alpha Strike — FIXED: local `splitDamage` and `kSplit*` deleted, calls `splitDamageAmong`; test: existing Alpha Strike tests (full suite)
  - c5: 444 Corrupt Enforcer — FIXED: private copy deleted, calls the shared `discardThenAct`; test: C5Test.CorruptEnforcerDiscardsTheChosenCard. (405 and 412 are gear files, stream c1.)

## See also

[08-cards-full-review.md](08-cards-full-review.md) is the full review of all 955 card files; start there for card work.

[07-cards-second-pass.md](07-cards-second-pass.md) holds the second-pass findings: hand-rolled resume cards, more text mismatches, off-board source handling, bookkeeping gaps.

## Clean

- Ids 1..955 with no gaps or duplicates; filename, `d.id`, `registerCard` and `register_card_N` agree in all 955 files; `card_type` matches the directory.
- No unity-build name collisions; all 20 reprint pairs carry the id suffix.
- No kill inside a damage loop, no loop mutating its own container.
- No `cardDefId()` passed as a `GameObjectId`, no `ctx.source` used as a def id.
- `kInvalidId` handling after `pickTarget` / `pickTargetPair`; every `targets[n]` index is guarded.
- No `ability_text` matching in any card file.
- `__died_empowered` is written at `engine/effect_executor.cpp:164` and never read (dead write).
