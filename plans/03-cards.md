# 3. Card bugs

Paths are under `src/cards/` unless noted.

Two engine facts drive several findings:

- **One shared `resume_point`.** All choice helpers share a single integer. A helper called after a higher-slot helper has finished sees a value past its own range and returns a padded default without asking.
- **Triggers get no targets** unless the card opts into `kFinalizeTrigger` (`engine/game_engine.cpp:1192-1198`; `trigger_manager.cpp:372` says "triggers receive empty targets").

## A. Choice helpers only compose in ascending slot order

- [ ] **3.1 Helper ordering bug in `Card`** [verified]
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

## B. Triggers that act only on `targets`

- [ ] **3.2 Triggered abilities whose printed effect never happens** — **superseded by [07 §A](07-cards-second-pass.md)**, which read every file: 26 BROKEN, 3 PARTIAL (Spirit's Refuge's aura works). Original entry kept for reference.
  - Tests pass because the fixture calls `onTrigger(ctx, targets)` directly.
  - `units/0092_riptide_rex.cpp:18` — "When you play me, deal 6" does nothing.
  - `units/0016_dangerous_duo.cpp:19`, `units/0238_leona_determined.cpp:18`, `units/0539_ahri_inquisitive.cpp:18`, `units/0199_tideturner.cpp:35`.
  - Full list: Navori Fighting Pit 278, Veiled Temple 535, Amateur Recital 763, Spirit's Refuge 63, Forge of the Future 212, and units 16, 51, 76, 82, 92, 97, 130, 132, 188, 191, 199, 234, 238, 330, 361, 384, 453, 479, 539, 589, 618, 738.
  - Fix direction: either convert each to `pickTarget` + `kFinalizeTrigger`, or have the engine choose targets at finalization for any trigger with `getTargetRequirements().count > 0`. Add an engine-level test that plays the card rather than calling `onTrigger`.
  - c0: 278 Navori Fighting Pit, 535 Veiled Temple, 763 Amateur Recital, 63 Spirit's Refuge, 212 Forge of the Future — FIXED: see 07 item 7.1 / 7.2
  - c4: 16, 51, 76, 82, 92, 97, 130, 132, 188, 191, 199 — FIXED: see 07 item 7.1.

## C. Non-idempotent work before a suspending helper

The callback re-runs from the top after a choice, so anything before the helper happens twice.

- [ ] **3.3** `units/0200_twisted_fate_gambler.cpp:45-46` — pops and recycles the top rune before `pickTarget` (`:64`, `:92`); a second rune is revealed on re-entry and the payoff re-derived. [confirmed]
  - c4: 200 Twisted Fate, Gambler — FIXED: the reveal-and-recycle runs once; the rune's domains are kept on the resolving chain item (resume_data slot 7) for the re-entry after `pickTarget`; test: C4Test.TwistedFateGambler_RevealsOneRuneHoweverOftenItIsReEntered
- [ ] **3.4** `spells/0515_arcane_shift.cpp:25-31` — banishes and replays `targets[0]` before `pickTarget` (`:44`); play triggers fire twice. [confirmed]
- [ ] **3.5** `units/0406_jayce_man_of_progress.cpp:50-52` — `killObject(gear)` before `pickTarget` (`:59`); with two gears both die, with one the picked gear is never played. [confirmed]
- [ ] **3.6** `gear/0242_baited_hook.cpp:33-36` — kills `targets[0]` then suspends (`:59`); on re-entry returns early, the chosen unit is never played. [confirmed]
  - c0: 242 Baited Hook — FIXED: the kill is recorded on the chain item (`resume_data[7]` = 1 + the killed unit's Might) so the re-entry after the pick neither kills again nor returns early; the picked unit is played; test: C0Test.BaitedHook_PaysPowerStaysOnBoardAndPlaysThePickedUnit
- [ ] **3.7** `gear/0490_altar_of_memories.cpp:35-38` — once-guard `__altar_drew` is never erased; from the second friendly death "yes" neither exhausts nor draws but still forces a hand card onto the deck. `spells/0475_guards.cpp:25-33` has the same permanent guard (`__guards_token`) [plausible]. [confirmed]

## D. Choices at finalization

- [ ] **3.8** `legends/0556_chem_baroness.cpp:23-27` — `still_legal` is `return true` and the exhaust cost has no ready check; two holds make two Gold. [confirmed]
- [ ] **3.9** `units/0147_wildclaw_shaman.cpp:24-33` — "spend a buff" never spends one; free buff and ready. [confirmed]
  - c4: 147 Wildclaw Shaman — FIXED: offered only with a buffed friendly unit; the controller picks whose buff and it is spent at finalization (`costDueNow`); test: C4Test.WildclawShaman_SpendsABuffToBuffAndReadyItself, C4Test.WildclawShaman_WithNoBuffToSpendIsNotOffered
- [ ] **3.10** `units/0449_overzealous_fan.cpp:40,51-52` — "kill me" cost paid after the marker and never verified; if the Fan was killed in response, the effect still happens. The effect itself is also wrong: the code bounces the attacker to hand, the text says move it to its base (08 §D). [confirmed]
- [ ] **3.11** `units/0230_albus_ferros.cpp:28-44` — X fixed at finalization, buffs spent at resolution, payout uses X. [confirmed]
- [ ] **3.12 Costs paid after the marker without `costDueNow`: 14 cards** [confirmed pattern] — **complete named list in [07 §C](07-cards-second-pass.md)**.
  - Payment happens at resolution, so a player can say yes then spend the resource on a Reaction (CR 383.3).
  - Clearest: `units/0500_fiora_worthy.cpp:41`, `units/0446_fae_porter.cpp:49`, `units/0796_baccai_reaper.cpp:57`.
  - Rest: in each opted-in file, grep after the `finalizingChoices(ctx)` line for `payOnePower|payEnergy|xp -= |buff_count -= 1|burn\(|killObject\(ctx.source\)`.
- [ ] **3.13 Counter writes before the marker: 4 cards** [confirmed violation, plausible harm]
  - `units/0162_miss_fortune_captain.cpp:34`, `units/0789_blade_twirler.cpp:31`, `units/0855_jayce_brilliant_inventor.cpp:42,47`, `units/0372_aphelios_exalted.cpp:33-44`. After `makeNewChoicesFor` resets `resume_point`, the "first time this turn" flag is already set.
  - c4: 162 Miss Fortune, Captain — FIXED: no counter at all; "the first time I move each turn" is `triggerCondition` (`moves_this_turn == 1`, bumped before every move event); test: C4Test.MissFortuneCaptain_FirstMoveOnly_AndLegendsCount

Clean: opt-in bits match overridden callbacks; every effect branch of the 10 multi-trigger cards has a marker; no pre-check is missing `costAlreadyPaid`.

## E. Objects that left play

- [ ] **3.14 Self-move on `ctx.source` guarded only by `objectExists`: 9 sites** [confirmed]
  - `objectExists` is true for every id ever created, so a source killed in response is moved back from trash.
  - `units/0404_ezreal_dashing.cpp:65-66`, `units/0067_blitzcrank_impassive.cpp:31` (card ends up in hand and trash), `units/0373_azir_ascendant.cpp:102-112`.
  - Rest: `grep -nE "(moveToBattlefield|moveToBase|bounceToHand|moveToLocation)\((ctx\.executor, *)?(ctx\.source|source)\b" src/cards/*/*.cpp`
  - c4: 67 Blitzcrank, Impassive — FIXED: the hold bounce checks `location.has_value()` (and `bounceToHand` now refuses an off-board unit, 02 item 2.28).
- [ ] **3.15 Trigger subject used after only `objectExists`** [plausible, low impact]
  - `units/0712_vex_apathetic.cpp:30`, `gear/0143_pirate_s_haven.cpp:26`, `battlefields/0272_back_alley_bar.cpp:26`, `gear/0060_mask_of_foresight.cpp:27`.
  - c0: 143 Pirate's Haven, 272 Back-Alley Bar, 60 Mask of Foresight — FIXED: the subject must still be a unit on the board; test: C0Test.PiratesHaven_ReadyingARuneDoesNotTrigger, .BackAlleyBar_OnlyAUnitStillOnTheBoardGetsTheMight, .MaskOfForesight_IgnoresAUnitThatLeftTheBoard
- [ ] **3.16** `units/0416_direwing.cpp:21` — "Another Dragon" skips every Direwing by def id; a second Direwing does not enter ready. Use the three-argument `entersReadyOnPlay(..., self)`. [confirmed]

## F. Data and reprint drift

- [ ] **3.17** `gear/0542_seal_of_strength.cpp:25` — adds `Domain::Order`; reprint 163 adds `Domain::Body`. [verified]
  - c0: 163 Seal of Strength — NOT A DEFECT: `gear/0163_seal_of_strength.cpp` adds `Domain::Body`, matching its text "[Add] [O]" and its Body domain; the wrong one is 542 (c1)
- [ ] **3.18** `units/0778_plundering_poro.cpp:18` — `createGoldToken` (ready); text says exhausted, reprint 392 uses `createGoldExhausted`. [confirmed]
- [ ] **3.19** `units/0548_karma_channeler.cpp:15-30` — only a `WhenYouPlayMe` Vision (the engine already does Vision, `trigger_manager.cpp:317`, so it runs twice) and no recycle trigger. `units/0235_karma_channeler.cpp:57` has tags `{"Vi", "Ionia"}`. [confirmed]
- [ ] **3.20** `units/0543_sett_brawler.cpp` lacks the `canActivateAbility` buff gate that `units/0164_sett_brawler.cpp:27` has. [confirmed]
- [ ] **3.21** Region tags differ in 5 reprint pairs (Ahri 539/119, Darius 547/243, Sett 164/543, Yasuo 205/546, Vayne 35/537). `units/0421_veteran_poro.cpp` sets `Keyword::Equip`, 779 does not. [confirmed]
  - c4: 119, 164, 205, 35 — NOT A DEFECT on these ids: they are the copies that carry the region tag (Ionia / Demacia, as `cards/json/ogn_*.json`); the reprints missing it (539, 543, 546, 537) are outside this range.
- [ ] **3.22** `spells/0251_noxian_guillotine.cpp:35` — without Legion writes `__kill_on_next_damage_this_turn`, which nothing reads; the spell does nothing. [confirmed]
  - c2: 251 Noxian Guillotine — FIXED: a one-shot delayed ability (`TriggerType::WhenAUnitTakesDamage`, `shared:` commit) kills the chosen unit the next time it is dealt damage this turn; test: ReviewFixC2Test.NoxianGuillotine_WithoutLegion_KillsTheUnitTheNextTimeItIsDamaged

## G. Shared helpers

- [x] **3.23 `payOnePower` bypasses the engine's power accounting** [confirmed] — FIXED: `payOnePower` recycles through new `recycleRuneForPower` (rune ready, `power_spent_this_turn`, `LeftBoardEvent`) and uses `EffectExecutor::payAnyPower` for [A]; `standardEquip` (both overloads), `SimpleEquipGear` and `UniversalEquipGear` pay through `payEnergy` + `payOnePower`, all-or-nothing, both ends on board; test: `ChoiceHelperTest.PayOnePowerRecyclesARuneWithTheEnginesBookkeeping`, `ChoiceHelperTest.EquipCostsCountAsPowerSpentAndAreAllOrNothing`
  - Where: `card_helpers.h:45-73`, `gear/equip_base.h:57-64`. No `power_spent_this_turn`, no `LeftBoardEvent`, no `is_exhausted` clear (engine path: `effect_executor.cpp:413-423`). Sivir, Mercenary (464) still reads 0 after two Equip costs.
- [ ] **3.24 Copy-pasted helpers** [confirmed]
  - `splitDamage` + four `kSplit*` constants duplicated in `spells/0751_alpha_strike.cpp:43-52` and `units/0041_volibear_furious.cpp:43-52` (compile only because the directories are separate unity targets).
  - `units/0444_corrupt_enforcer.cpp:35` re-implements `discardThenAct`.
  - 5 files hand-roll rune recycling instead of `payOnePower`.
  - HELPER DONE (stream helpers): `card_helpers.h` now has `splitDamageAmong` + `kSplitDamage*` (the two copies, verbatim, under a new name so it cannot collide with them) and `recycleRuneForPower`; test: `ChoiceHelperTest.SplitDamageAmongLetsTheControllerDivideTheDamage`.
  - STILL OPEN, card streams — recipe:
    - `spells/0751_alpha_strike.cpp`, `units/0041_volibear_furious.cpp`: delete the local `splitDamage` and the four `kSplit*` constants; call `splitDamageAmong(ctx, label, total, source, legal, damaged)` (same signature and behaviour).
    - `units/0444_corrupt_enforcer.cpp`: delete the private `discardThenAct`; call `discardThenAct(ctx, 1, label, [](CardContext&) {})` from `card_helpers.h`.
    - `gear/0405_hextech_anomaly.cpp`, `gear/0412_the_zero_drive.cpp`, `gear/0498_blade_of_the_ruined_king.cpp`, `gear/0748_hextech_gauntlets.cpp`, `spells/0263_bullet_time.cpp`: replace every hand-written `rune_deck.insert(...)` recycle with `payOnePower(ctx, player, domain)` (one Power of a Domain; `std::nullopt` = [A]), `ctx.executor.payAnyPower(player, n)` (n [A]), or — where the card picks the rune itself — `recycleRuneForPower(ctx, player, rune)`. Check affordability first with `payableOnePower` / `ctx.executor.payableAnyPower`. An Equip that is "[N] + one Power" is `standardEquip(ctx, ctx.source, unit, N, domain)`.
  - c4: 41 Volibear, Furious — FIXED: local `splitDamage` and `kSplit*` deleted, calls `splitDamageAmong`; test: existing Volibear tests and ChoiceHelperTest.SplitDamageAmongLetsTheControllerDivideTheDamage

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
