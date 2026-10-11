[Back to the full card review](../08-cards-full-review.md)

# Batch 10 - units/0372_aphelios_exalted.cpp .. units/0955_shadow_clone.cpp

Files read in full: 79 of 79.

Only findings not already in `03-cards.md` / `07-cards-second-pass.md` are listed.

## Findings
- [x] **449 Overzealous Fan** - `units/0449_overzealous_fan.cpp:52` [confirmed] high — FIXED: `moveToBase(attacker)` (its base; it keeps buffs, damage and gear); the controller chooses among the attacking units; "kill me" is paid under `costDueNow` at finalization and only by a Fan on the board; test: C5Test.OverzealousFanMovesTheAttackerToItsBase
  - Text: "When I defend, you may kill me to move an attacking unit to its base."
  - Code: `ctx.executor.bounceToHand(attacker)` - the attacker is returned to its owner's hand, not moved to its base.
  - Scenario: opponent attacks with a 6-cost unit carrying a buff and an Equipment; Fan is sacrificed -> the unit goes to hand (buff, damage and attachment lost, must be re-paid) where the card only removes it from the combat to base. (03 item 3.10 describes the bounce as if it were the printed effect.)
- [x] **459 Windsinger** - `units/0459_windsinger.cpp:58-62` [confirmed] high — FIXED: `Keyword::Hidden` on the def, text carries "[Hidden]"; test: C5Test.KeywordsTheTextOnlyMentionsAreNotOnTheDef
  - Text: "Hidden (Hide now for [A] to react with later for .)"
  - Code: `d.keywords` never sets `Keyword::Hidden` (the text is stored without brackets). The hide generator requires the bit (`engine/game_engine.cpp:2860`).
  - Scenario: Windsinger in hand with a controlled battlefield and a spare rune -> no Hide action is ever offered; the card can only be played face up at normal timing. Compare 562, 703, 886, 922, which set the keyword.
- [ ] **470 Ezreal, Prodigy** - `units/0470_ezreal_prodigy.cpp:74-75` [confirmed] high
  - Text: "Optional additional costs you pay cost [1] or [A] less."
  - Code: not implemented. The class has only the `WhenYouPlayMe` discard/draw; nothing in `src/engine` or `src/core` reduces an optional additional cost (`maybePayOptionalAdditionalCost`, `game_engine.cpp:5520-5560`, charges `cost.energy` / `cost.power` unmodified; same for Accelerate / Repeat).
  - Scenario: Ezreal on board, play Frostcoat Cub and accept "[B] as an additional cost" -> the full [B] is charged; should be free. Same for every Accelerate / Repeat / "you may pay" card. (07 lists 0470 as clean; that pass only checked resume handling.)
  - BLOCKED: engine work. `GameEngine::maybePayOptionalAdditionalCost` and the Accelerate / Repeat payments charge the cost unmodified; it needs a per-player continuous "optional additional costs cost [1] or [A] less" value (reset in `recalculateAuras`, set by this card's `applyPassiveAura`) read at each of those sites. Reported under "Needs another stream".
- [ ] **591 Red Brambleback** - `units/0591_red_brambleback.cpp:22-25` [documented] high
  - BLOCKED: needs `TriggerManager::onScore` to queue every conquer trigger for the battlefield (units, their gear, the legend, the battlefield card) once more per such unit there, as `queueOwnDeathTrigger` does with `deathknell_double_count` — the same engine change Blue Sentinel 649 needs for holds. The card documents the gap.
  - Text: "Your conquer effects for conquering here trigger an additional time."
  - Code: not implemented (in-file "ENGINE GAP" comment; no conquer analogue of `deathknell_double_count` exists).
  - Scenario: Brambleback conquers -> its own "[Buff] a friendly unit" resolves once instead of twice; other units' / legend's conquer triggers at that battlefield also fire once.
- [ ] **875 Jayce, Hammer in Hand** - `units/0875_jayce_hammer_in_hand.cpp:34` (root cause `engine/effect_executor.cpp:667`) [confirmed] medium
  - Text: "choose one to give me this turn - ... [Deflect 2]"
  - Code: `giveTemporaryKeyword(.., Keyword::Deflect, 2)` does `deflect_value += 2`; nothing ever subtracts it (Assault/Shield have `temp_*_value`, Deflect does not; the only other writes are object init at `game_engine.cpp:5253` and token copy). `expireTemporaryKeywords` clears the keyword bit, but `deflectCostFor` (`effect_executor.cpp:362-371`) still charges when `deflect_value > 0`.
  - Scenario: Jayce picks Deflect 2 on turn 3 -> opponent must pay [A][A] to choose him for the rest of the game; pick it again on turn 5 -> [A] x4. Permanent and stacking instead of "this turn".
- [x] **618 Yuumi, Magical Cat** - `units/0618_yuumi_magical_cat.cpp:44` [confirmed] medium — FIXED: `Keyword::Tank` removed from the def; the trigger now gives +3 [M] and [Tank] to the chosen other unit here (07 item 7.1); test: C6Test.Yuumi_GivesAnotherUnitHereMightAndTank
  - Text: "give one of your other units here +3 [M] and [Tank] this turn" (Yuumi herself has no printed keyword).
  - Code: `d.keywords.set(Keyword::Tank)` on Yuumi's own def. Combat damage ordering reads `hasKeyword(Keyword::Tank)` (`game_engine.cpp:3955,4050`).
  - Scenario: Yuumi (1 Might) defends beside a 5-Might ally -> the attacker is forced to assign lethal to Yuumi first; by the card it could ignore her. (7.1 notes the grant is missing, not that Yuumi wrongly has it.)
- [ ] **726 Safety Inspector** - `units/0726_safety_inspector.cpp:63-78` [documented] medium
  - Text: "each player must kill one of their units."
  - Code: the opponent is never asked; the engine kills the opponent's lowest-Might unit (first in map order on ties). Also the "spend 3 XP as an additional cost to play me" is asked and charged in the trigger at resolution (`:32-41`), not at play.
  - Scenario: opponent has a 1-Might champion with Equipment and a 3-Might vanilla -> the champion dies; the opponent would pick the vanilla.
- [ ] **796 Baccai Reaper** - `units/0796_baccai_reaper.cpp:24-37,51` [confirmed] medium
  - Text: "When I attack, you may pay [R] to give me [Assault 2] this turn."
  - Code: `canPayFury` (the `still_legal` gate) accepts floating Fury/universal power or a READY Fury rune only. Power is paid by recycling a rune, ready or exhausted (CR 164.2; `payOnePower` at `card_helpers.h:57-66` prefers an exhausted one).
  - Scenario: all Fury runes were exhausted for Energy this turn (the usual state when attacking) -> "MAY_NOT_OFFERED", the ability can never be used, although an exhausted Fury rune could be recycled for [R].
- [ ] **741 Rift Herald** - `units/0741_rift_herald.cpp:33-60` [documented] medium
  - Text: "look at the top 3 cards of your Main Deck. You may reveal a unit from among them and draw it. Recycle the rest."
  - Code: (a) no "may" - the first unit among the three is always drawn; (b) the player does not choose which unit; (c) `CardRevealedEvent{..., revealed_to_all=true}` is emitted for all three looked-at cards, not only the drawn one. Fate Weaver 626 does the same text correctly.
  - Scenario: top 3 = cheap unit, key spell, bomb unit -> the cheap unit is drawn with no choice, and the opponent's observation shows all three cards.
- [x] **406 Jayce, Man of Progress** - `units/0406_jayce_man_of_progress.cpp:55-68` [confirmed] medium — FIXED: the gear to kill is chosen (`confirmOptional` + `pickTarget` at finalization) and nothing is killed before the choice; the reward is a permission for the turn — a one-gear, this-turn `CostModifier` for [7] Energy (Power still paid), not a forced immediate play; test: C5Test.JayceKillsTheChosenGearAndGrantsAPlayThisTurn
  - Text: "If you do, you may play a gear with Energy cost no more than [7] from hand this turn, ignoring its Energy cost."
  - Code: after the kill, `pickTarget` over eligible hand gear with no decline option, played immediately; the permission does not last "this turn". (The killed gear is also the first friendly gear in map order - listed under S5.)
  - Scenario: only eligible hand gear is one the player wants to hold, or its Power cannot be covered until later in the turn -> it is played now regardless (and for free, 7.7), or the permission is lost if no gear is in hand yet.
- [x] **373 Azir, Ascendant** - `units/0373_azir_ascendant.cpp:78-93` [documented] medium — FIXED: the "may" is asked (`confirmOptional`), then the unit alone or the unit and one of its Equipment (only gear with the Equipment tag); test: C5Test.AzirMayLeaveTheEquipmentWhereItIs
  - Text: "If it's equipped, you may attach one of its Equipment to me."
  - Code: `pickTargetPair` has no decline, so if the chosen unit has any gear one is always moved to Azir (in-file "GAP" comment). `legal_b_fn` also offers every attached gear, not only Equipment.
  - Scenario: swap with a unit whose Equipment the player wants to stay there -> it is stripped onto Azir.
- [x] **373 Azir, Ascendant** - `units/0373_azir_ascendant.cpp:39-70` [confirmed] low — FIXED: targets come from the four-argument `enumerateLegalTargets(.., source)` and once-per-turn is `canActivateAbilityAt` on the activating Azir; test: C5Test.AzirOncePerTurnIsPerAzir
  - Text: "Choose a unit you control ... Use only once per turn."
  - Code: `enumerateLegalTargets` / `hasLegalTargets` locate "Azir" as the first on-board object with this def id, not the activating instance (the 4-argument `enumerateLegalTargets(..., source)` overload is not used). With two Azirs: once the first-found one has activated, `hasLegalTargets` returns false for both; and the second Azir's target list contains itself, so picking it hits `picked == source` (`:100`) after [G] was paid.
  - Scenario: two Azirs; first uses the ability -> second can no longer activate this turn.
- [x] **373 Azir, Ascendant** - `units/0373_azir_ascendant.cpp:135-148` [confirmed] low — FIXED: the Equipment moves with `attachFree`, which announces "attached" / "equipped"; test: C5Test.AzirMayLeaveTheEquipmentWhereItIs
  - Text: "attach one of its Equipment to me"
  - Code: hand-rolled re-attach does not emit `ObjectStateChangedEvent "attached"/"equipped"` (`attachFree` in `card_helpers.h:114` does), so attach triggers on the gear never fire for this move.
  - Scenario: gear with a "when attached" trigger moved by Azir -> no trigger.
- [ ] **878 Corrupted Dragon** - `units/0878_corrupted_dragon.cpp:20-22` [confirmed] low
  - Text: "If your score is not within 3 points of the Victory Score, I enter ready."
  - Code: uses `state.mode.victory_score`, ignoring `Card::victoryScoreModifier()` battlefields (`EffectExecutor::effectiveVictoryScore`, `effect_executor.cpp:322`).
  - Scenario: a "+1 points needed to win" battlefield is in play (victory 9), score 5 -> 4 away, should enter ready; code computes 8-5=3 and it enters exhausted.
- [ ] **459 / 585 / 674 / 681 / 878 - "can't be chosen by enemy spells and abilities" ignored** - `units/0459_windsinger.cpp:23-29`, `units/0585_katarina_reckless.cpp:32-35`, `units/0674_irresistible_faefolk.cpp:29-36`, `units/0681_kha_zix_evolving_hunter.cpp:33-40`, `units/0878_corrupted_dragon.cpp:33-41` [confirmed] low
  - Text: each chooses / moves / damages an enemy unit.
  - Code: the candidate lists are built by hand without the `untargetable_by_enemy` filter that `chooseableUnits` and the neighbouring cards (404, 562, 667, 699) apply. Corrupted Dragon's and Faefolk's picks also bypass Deflect (hand-rolled `requestChoice` / first-match, as in 7.30).
  - Scenario: enemy Baron Nashor at the battlefield -> Katarina's 2 damage / Kha'Zix's Might damage / Windsinger's bounce can choose it.
  - c5: 459 Windsinger — FIXED: the list is `chooseableUnits` (honours `untargetable_by_enemy`).
  - c6: 585 Katarina, 674 Irresistible Faefolk, 681 Kha'Zix — FIXED: candidates come from `chooseableUnits`; Faefolk and Kha'Zix pick through `pickTarget` (Deflect charged); test: C6Test.Katarina_CannotChooseAnUntargetableEnemy, .Faefolk_PullsTheEnemyThePlayerPicks
- [ ] **728 Stalking Wolf** - `units/0728_stalking_wolf.cpp:21-27` [documented] low
  - Text: "You may [Ambush] me to its battlefield, even if you don't have other units there."
  - Code: not modelled (in-file comment): the sacrifice is chosen after the Wolf has landed, so the Ambush location is only the standard "battlefield where you have units"; and if the sacrificed unit was the only friendly unit there the standard Ambush was only legal because of it.
  - Scenario: lone Poro at a battlefield; Ambush the Wolf there killing the Poro -> allowed only by accident of ordering; killing a Poro at battlefield A cannot place the Wolf at A if another unit elsewhere is what made the play legal.
- [ ] **440 Jax, Unrelenting** - `units/0440_jax_unrelenting.cpp:33-83` [documented] low
  - Text: "[Equip] one of your Equipment to me for [A] less"
  - Code: the Equip cost is re-parsed from the gear's `ability_text`; Equipment whose Equip cost has more than one Power or any non-rune part is never offered to Weaponmaster (in-file ENGINE-GAP), and `attachFree` bypasses the gear's own `onEquip`.
  - Scenario: Equipment with "[Equip] [1][O][O]" on board when Jax is played -> not offered, though Weaponmaster should allow it for [1][O].
  - BLOCKED: same root as 07 item 7.7 — the Equip cost has to become data (`Card` equip-cost descriptor) before "for [A] less" can be computed for every Equipment and the gear's own `onEquip` run; sweep (design) in STATUS.md. c5 only removed the spurious `Keyword::Equip` from the def.

## Systemic
- S1: none new (384 Aspiring Engineer, 453 Beast Below, 479 Sandshifter, 539 Ahri, 589 Inviolus Vox, 618 Yuumi, 738 Vi are already in 7.1)
  - c5: 384 Aspiring Engineer, 453 Beast Below, 479 Sandshifter — FIXED under 07 item 7.1.
  - c6: 539, 589, 618 — FIXED (07 item 7.1)
- S2: 404 Ezreal Dashing (trigger reads `current_might` of ctx.source, `:38`), 500 Fiora Worthy (readies `subject` with no on-board check, `:33-42`), 585 Katarina Reckless (`readyObject(ctx.source)`, `:27`), 642 Hwei (`giveTemporaryMight(ctx.source)`, `:77`), 681 Kha'Zix (Might of ctx.source, `:51`), 955 Shadow Clone (`giveTemporaryKeyword(ctx.source)`, `:44`) - all low impact
  - c5: 404 Ezreal, Dashing — FIXED: no damage when he is no longer on the board (his Might is read only then).
  - c6: 500, 585 — ALREADY FIXED by the executor guard (`readyObject` does nothing off the board; Fiora also checks the subject); 642 Hwei, 681 Kha'Zix — FIXED: on-board check before using the source
- S3: 404 Ezreal Dashing (`applyPassiveAura` loops every instance without `forEachSelfOnce`; the effect is a boolean `suppress_combat_damage`, so duplicates are harmless)
  - c5: 404 — NOT A DEFECT today: the hook runs once per (card, controller) (`1ae4c88`) and visits each instance once.
- S4: 372 Aphelios Exalted (`:83`), 591 Red Brambleback (`:37`), 613 Ivern Nurturer (`:114`), 614 Nami Headstrong (`:88`)
  - c5: 372 Aphelios — ALREADY FIXED by `buffUnit` (`99d179e`).
  - c6: 591, 613, 614 — ALREADY FIXED by 02 item 2.14 (`buffUnit`)
- S5: 406 Jayce Man of Progress (gear to kill, `:20-28,50`), 613 Ivern Nurturer (buff recipient = first friendly unit in map order, `:110-118`), 667 Imposing Challenger (destination = first other battlefield, `:57-62`; matters only with 3+ battlefields), 741 Rift Herald (unit drawn, `:49`), 726 Safety Inspector (opponent's unit, `:63-74`), 449 Overzealous Fan ("an attacking unit" = the first attacker captured in map order by `trigger_manager.cpp:471-480`)
  - c5: 406 Jayce (gear to kill) and 449 Overzealous Fan (which attacker) — FIXED, see the findings.
  - c6: 613 Ivern — FIXED (the controller picks the unit to buff); 667 Imposing Challenger — FIXED (`pickMode` over the other battlefields when there is more than one); test: C6Test.Ivern_DrawsTheUnitThePlayerPicksAndBuffsTheChosenUnit, .ImposingChallenger_ThePlayerPicksTheDestination

## Corrections
- 07 "Read in full and clean" lists units 0440, 0470, 0626, 0642, 0726, 0728, 0729, 0919. That holds only for the resume / finalization classes that pass checked: 0470 has an unimplemented sentence, 0726 never asks the opponent, 0728 and 0440 carry documented gaps (all above). 0626, 0642, 0729, 0919 are clean against the full checklist.
- 03 item 3.10 (Overzealous Fan) says "the attacker is still bounced" as the expected effect; the printed effect is a move to base, so the bounce itself is wrong (first finding above).
- 7.1's Yuumi row ("never grants the printed [Tank]") is incomplete: the def also puts [Tank] on Yuumi herself.
- No other entry in 03 / 07 for these 79 files was found to be wrong.

## Not read
None
