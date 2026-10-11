[Back to the full card review](../08-cards-full-review.md)

# Batch 01 - gear/0396_experimental_hexplate.cpp .. legends/0928_butcher_of_the_sands.cpp

Files read in full: 90 of 90.

Legend trigger dispatch check (src/engine/trigger_manager.cpp): AtStartOfBeginning (Loose Cannon), AtEndOfTurn (Dark Child), WhenIConquer (The Boss, Might of Demacia), WhenIConquerOrHold (Keeper of the Hammer), WhenIWinCombat (Glorious Executioner), WhenYouStun (Radiant Dawn) and WhenYouPlayASpell (Lady of Luminosity) all reach the LegendZone. WhenAnEnemyUnitDies (Battle Mistress) does NOT: the sweep at trigger_manager.cpp:665-679 requires `location.has_value()`.

## Findings

- [x] **490 Altar of Memories** - `gear/0490_altar_of_memories.cpp:35` [confirmed] high — FIXED: the once-guard lives on the resolving chain item, so every trigger exhausts and draws; exhaust is paid at finalization (`kFinalizeTrigger`), on-board check added; test: ReviewFixC1Test.AltarOfMemories_WorksOnEveryDeathNotOnlyTheFirst, ReviewFixC1Test.AltarOfMemories_AnExhaustedAltarIsNotOffered
  - Text: "you may exhaust me to draw 1, then put a card from your hand on the top or bottom of your Main Deck."
  - Code: the exhaust + draw is guarded by `card_counters["__altar_drew"]`, which is set on first use and never erased.
  - Scenario: second friendly death in the game (Altar ready again) -> player says yes -> Altar is NOT exhausted, NO card is drawn, but a hand card is still put on the deck. Because it never exhausts again it also re-triggers on every later death the same turn, each time costing a hand card.

- [x] **249 Loose Cannon** - `legends/0249_loose_cannon.cpp:17` [confirmed] high — FIXED: draws only with one or fewer cards in hand, checked as it resolves; test: ReviewFixC1Test.LooseCannon_DrawsOnlyWithOneOrFewerCardsInHand
  - Text: "At start of your Beginning Phase, draw 1 if you have one or fewer cards in your hand."
  - Code: `drawCards(ctx.controller, 1)` unconditionally; no hand-size check and no triggerCondition.
  - Scenario: Jinx player with 5 cards in hand draws an extra card every turn.

- [x] **514 Prodigal Explorer** - `legends/0514_prodigal_explorer.cpp:17` [confirmed] high — FIXED: the gate is `PlayerState::enemy_choices_this_turn >= 2`: every site that locks in a choice calls `GameState::noteChosen`, which counts enemy units and gear chosen by a spell or a unit's ability; test: SweepFlowTest.ProdigalExplorer_NeedsTwoEnemyChoicesBySpellsOrUnitAbilities
  - Text: "[E]: [Reaction] - Draw 1. Use only if you've chosen enemy units and/or gear twice this turn with spells or unit abilities."
  - Code: no `canActivateAbility` gate at all; also `isActionAbility()` = true and `isReactionAbility()` not overridden, so it is Action-timed, not Reaction-timed.
  - Scenario: Ezreal player exhausts the legend for a free card every turn having chosen nothing; conversely it can never be used in a closed state as a Reaction.
  - c1: timing FIXED (`isReactionAbility`, no longer an Action ability) and an interim gate added (`chose_enemy_unit_this_turn`); test: ReviewFixC1Test.ProdigalExplorer_IsAReactionWithAGate

- [x] **296 Hand of Noxus** - `legends/0296_hand_of_noxus.cpp:21` [confirmed] high — FIXED: `onActivate` does nothing unless a card was played this turn; test: ReviewFixC1Test.HandOfNoxus_NoEnergyWithoutLegion
  - Text: "[E]: [Reaction], [Legion] - [Add] [1]. (Get the effect if you've played a card this turn.)"
  - Code: relies on `requiresLegion()`, which nothing in src/engine or card.cpp ever calls; onActivate adds the Energy unconditionally.
  - Scenario: first action of the turn, no card played yet -> legend still adds [1], so the first card each turn is 1 cheaper.

- [x] **554 Grandmaster at Arms** - `legends/0554_grandmaster_at_arms.cpp:45` [confirmed] high — FIXED: only Equipment on the board; Equipment and unit chosen with `pickTargetPair` at activation (`kFinalizeActivate`); test: ReviewFixC1Test.GrandmasterAtArms_OnlyEquipmentOnTheBoardAndTheControllerChooses, ReviewFixC1Test.GrandmasterAtArms_MovesAnAttachedEquipmentToAnotherUnit
  - Text: "[1], [E]: Attach a detached Equipment you control to a unit you control."
  - Code: the Equipment search loops over every object with `controller == me && isEquipment` and no `location.has_value()` check; cards in hand / deck / trash have controller = owner (game_engine.cpp:5242) and `attached_to` empty, so they count as "detached". It also takes the first match in unordered_map order, with no player choice of which Equipment.
  - Scenario: Jax player with a B.F. Sword still in the Main Deck activates ability 0 -> the deck copy is attached to a unit (and stays listed in main_deck), giving +3 Might from a card that was never played.

- [x] **517 Battle Mistress** - `legends/0517_battle_mistress.cpp:17` [confirmed] high — FIXED: the enemy-death half was fixed by 02 item 2.13; the recycle-a-rune half is a second trigger (`WhenYouRecycleARune`, new dispatch in `TriggerManager::onLeftBoard`, `shared:` commit) with a real may and exhaust cost; test: ReviewFixC1Test.BattleMistress_RecyclingARuneOffersAGold, ReviewFixC1Test.BattleMistress_AnExhaustedLegendIsNotOffered, ChainTriggerFixTest.Legend_EnemyUnitDeathReachesTheLegend
  - Text: "When you recycle a rune, you may exhaust me to play a Gold gear token exhausted. When one or more enemy units die, ready me."
  - Code: declares WhenAnEnemyUnitDies, which is only dispatched to objects with a board location (trigger_manager.cpp:665-679), so it never fires on a legend. The recycle-a-rune clause is not wired at all (documented in the file).
  - Scenario: any game - the Sivir legend has no effect whatsoever.

- [x] **784 Wuju Master** - `legends/0784_wuju_master.cpp:27` [confirmed] high — ALREADY FIXED: `1ae4c88` removed the engine's text-parsed aura pass (02 item 2.1), the card's own xp >= 6 hook is the only source; test: ReviewFixC1Test.WujuMaster_LevelSixIsGatedOnXp
  - Text: "[Level 6][>] Your units have +1 [M]."
  - Code: the card's applyPassiveAura gates on xp >= 6, but the engine's text-matched aura pass (game_engine.cpp:4500-4607, "your units have" pattern, which includes LegendZone sources) also reads this ability_text and applies +1 [M] with no Level gate. (The file's comment that legends get no aura pass is stale.)
  - Scenario: at 0 XP every friendly unit already has +1 [M]; at 6+ XP they have +2 [M] instead of +1.

- [x] **805 Rage Amplifier** - `gear/0805_rage_amplifier.cpp:21` [confirmed] high — ALREADY FIXED: `1ae4c88` (02 item 2.1), the extra +1 from the text matcher is gone; test: ReviewFixC1Test.RageAmplifier_GivesOneMightOrTwoWhenEmpowered
  - Text: "Your units have +1 [M]. If I'm [Empowered], they have +2 [M] instead."
  - Code: applyPassiveAura adds +1 / +2, and the engine's "your units have" text matcher (game_engine.cpp:4605) independently adds another +1 from the same ability_text.
  - Scenario: one un-Empowered Rage Amplifier gives +2 [M]; Empowered gives +3 [M].

- [x] **502 Purifier** - `legends/0502_purifier.cpp:38` [confirmed] high — ALREADY FIXED: `73e121e` (08 item 8.3) — the engine fold adds the Might of an aura-granted [Assault]; test: AurasCostsTest.AuraAssaultShield_PurifierAndBlackFlameAltar
  - Text: "Your Equipment each give [Assault]. (+1 [M] while equipped unit is an attacker.)"
  - Code: pushes an AuraEffect with keyword Assault / keyword_value 1 but might_bonus 0. recomputeMight only adds `assault_value` (printed / temporary), and the aura fold only sums might_bonus (see the auraKeyword helper comment), so the keyword bit is set but no Might is gained.
  - Scenario: Lucian player attacks with an equipped 3 [M] unit -> it fights at its normal Might, not +1.

- [x] **551 Mechanized Menace** - `legends/0551_mechanized_menace.cpp:27` [confirmed] high — ALREADY FIXED: `73e121e` (08 item 8.3); test: AurasCostsTest.AuraShield_TaricAndMechanizedMenace
  - Text: "Your Mechs have [Shield]. (+1 [M] while they're defenders.)"
  - Code: same defect as Purifier - AuraEffect with keyword Shield, keyword_value 0, might_bonus 0.
  - Scenario: a defending Mech gets no +1 [M].

- [x] **512 Emperor of the Sands** - `legends/0512_emperor_of_the_sands.cpp:36` [confirmed] high — FIXED (sweep-triggers): [Weaponmaster] (and [Vision]) are keyword abilities queued per instance (`engine/keyword_abilities`), so a token or an aura-granted keyword triggers; test: SweepTriggersTest.Weaponmaster_ASandSoldierTokenMayEquipAsItIsPlayed, .Weaponmaster_IsAMay
  - Text: "Your Sand Soldiers have [Weaponmaster]."
  - Code: grants Keyword::Weaponmaster as an aura. Nothing reads that keyword (grep: only main.cpp / types.cpp name tables); Weaponmaster only exists as behaviour of WeaponmasterUnit card classes, and Sand Soldier tokens have no card class.
  - Scenario: Azir plays a Sand Soldier with an Equipment in base -> no Weaponmaster attach is ever offered.
  - BLOCKED: 08 item 8.4 remainder, assigned to the sweep stream in STATUS.md — Weaponmaster for tokens needs a keyword-level "when you play me" handler in the engine (tokens have no card class).

- [x] **256 Unforgiven** - `legends/0256_unforgiven.cpp:17` [confirmed] high — FIXED: a unit at a battlefield goes to its base, a unit in its base goes to a battlefield the controller chooses (`pickMode`); on-board and still-friendly checks; test: ReviewFixC1Test.Unforgiven_MovesAUnitFromItsBaseToAChosenBattlefield, ReviewFixC1Test.Unforgiven_MovesAUnitAtABattlefieldToItsBase
  - Text: "[2], [E]: Move a friendly unit to or from its base."
  - Code: always `moveToBase(targets[0])`; target enumeration includes units already at base.
  - Scenario: Yasuo player targets a unit in base to send it to a battlefield -> pays [2], unit stays in base, and a base->base UnitMovedEvent fires its "When I move" triggers. The "from its base" half of the ability does not exist.

- [x] **252 Nine-Tailed Fox** - `legends/0252_nine_tailed_fox.cpp:17` [documented] high — FIXED: triggers on `WhenAnEnemyUnitAttacksYourBattlefield` (new, dispatched from `TriggerManager::onCombatStarted` once per attacking unit, `shared:` commit) and gives it -1 [M] this turn, minimum 1; test: ReviewFixC1Test.NineTailedFox_AttackersOfYourBattlefieldGetMinusOne, ReviewFixC1Test.NineTailedFox_NotAtABattlefieldYouDoNotControl
  - Text: "When an enemy unit attacks a battlefield you control, give it -1 [M] this turn, to a minimum of 1 [M]."
  - Code: no trigger, no hook - the legend is a blank card.
  - Scenario: any attack into an Ahri player's battlefield is at full Might.

- [x] **542 Seal of Strength** - `gear/0542_seal_of_strength.cpp:27` [confirmed] high — FIXED: adds Body power; test: ReviewFixC1Test.SealOfStrength_AddsBodyPower
  - Text: "[E]: [Reaction] - [Add] [O]." ([O] is Body; the card's domain is Body.)
  - Code: `addFloatingPower(ctx.controller, Domain::Order, 1)`.
  - Scenario: Body deck taps the Seal to pay a [O] power cost -> gets Order power, which cannot pay it.

- [x] **508 Rabadon's Deathcrown** - `gear/0508_rabadon_s_deathcrown.cpp:19` [confirmed] high — FIXED: `applyPassiveAura` adds 3 to `bonus_damage_dealt` while attached to a unit on the board; test: ReviewFixC1Test.RabadonsDeathcrown_GivesThreeBonusDamageWhileAttached
  - Text: "Your spells and abilities deal 3 Bonus Damage (while this is attached)."
  - Code: only onEquip is implemented; nothing sets `PlayerState::bonus_damage_dealt` (the field dealDamage reads), so the effect text does nothing.
  - Scenario: equipped Deathcrown, controller casts a 2-damage spell -> deals 2, not 5.

- [ ] **809 Endless Riches** - `gear/0809_endless_riches.cpp:50` [confirmed] high
  - Text: "You may play cards from your trash. If a card would go to your trash from anywhere other than your Main Deck, banish it instead."
  - Code: sets `may_play_from_trash` and `trash_goes_to_banish`, but neither flag is read anywhere in src/ (only reset in recalculateAuras).
  - Scenario: player plays Endless Riches -> hand and trash banished, draw phase skipped, 7 cards burned to trash, and none of them can be played; the player is locked out of cards for the rest of the game.
  - BLOCKED: engine work, not card work — `may_play_from_trash` needs a play-from-trash path for every card type (today's `TrashReplayGrant` is spell-only) and `trash_goes_to_banish` needs a replacement at every site that files a card in the trash. The card already sets both flags every aura pass.

- [x] **640 Sprite Fountain** - `gear/0640_sprite_fountain.cpp:20` [confirmed] high — ALREADY FIXED: 08 item 8.5 (`TriggerManager::onLeftBoard` fires a killed gear's WhenIDie); test: ChainTriggerFixTest.GearDeath_TemporaryGearGetsItsDeathknell
  - Text: "[Deathknell][>] Repeat this gear's play effect."
  - Code: registers WhenIDie, but WhenIDie is only dispatched from onUnitDied (UnitDiedEvent); a gear dying emits only LeftBoardEvent, which TriggerManager does not subscribe to.
  - Scenario: the [Temporary] Fountain is killed at the next Beginning Phase -> no second Sprite token.

- [x] **607 Forgotten Signpost** - `gear/0607_forgotten_signpost.cpp:74` [confirmed] high — FIXED: `pickTargetPair` at activation (`kFinalizeActivate`), the exhaust is paid as a cost then; offered only with a ready unit and a different unit elsewhere; test: ReviewFixC1Test.ForgottenSignpost_ExhaustsOneUnitAndMovesADifferentOne, ReviewFixC1Test.ForgottenSignpost_NeedsAReadyUnitAndADifferentOneElsewhere
  - Text: "Exhaust a unit you control, [E]: Move a different unit you control to the location of the unit you exhausted"
  - Code: two pickTarget calls in one onActivate. After the first completes (resume_point 8) the second returns the stored first answer (resume_data[2]) without prompting, so `mover == to_exhaust`.
  - Scenario: exhaust unit A at battlefield 1 intending to move B there -> B never moves; A is "moved" to its own location, firing its When-I-move triggers.

- [x] **471 Last Rites** - `gear/0471_last_rites.cpp:64` [confirmed, cost part documented] high — FIXED: trigger opts into `kFinalizeTrigger` (may + unit chosen by the controller), the unit is played with `playPayingCost`; Equip cost recycles the two cards the controller chooses (`chooseObjectNow`), all-or-nothing, and announces `recycled_main`; test: ReviewFixC1Test.LastRites_ThePlayedUnitIsPaidFor, ReviewFixC1Test.LastRites_EquipRecyclesTheTwoCardsTheControllerChooses, EquippedTriggerChainTest.YouMayIsARealChoice_Declined
  - Text: "When I conquer or hold, you may play a unit from your trash. (You still pay its costs.)"
  - Code: onEquippedTrigger is called directly from fireEquippedTriggers, not from the chain, so `chain.resuming` is empty: confirmOptional returns yes whenever a unit is in trash and pickTarget returns the first one. The unit is then played with playIgnoringCost (file comment documents the missing payment; executor.playPayingCost exists).
  - Scenario: equipped unit holds each turn -> the first unit in trash returns to play for free every time, with no choice of unit and no option to decline. Also the Equip cost recycles the last two trash cards with no choice (line 39).

- [x] **401 Temporal Portal** - `gear/0401_temporal_portal.cpp:37` [documented] high — FIXED: cost is [A] (`power_any_domain`); the effect sets the engine's existing one-shot grant `grant_repeat_base_to_next_spell` (The Academy's). Remaining gap, noted in the file: the grant carries the spell's Energy cost only; test: ReviewFixC1Test.TemporalPortal_CostIsAnyPowerAndTheNextSpellGetsRepeat
  - Text: "[A], [E]: Give the next spell you play this turn [Repeat] equal to its cost."
  - Code: onActivate only logs. Cost is also encoded as Mind power (`power_domain = Domain::Mind`, no `power_any_domain`) although [A] is any domain.
  - Scenario: player pays a power and exhausts the gear for no effect; a non-Mind power cannot pay it.

- [x] **611 Honeyfruit** - `gear/0611_honeyfruit.cpp:19` [confirmed] medium — FIXED: (a) `entersExhausted()`; (b) ability 1 adds 1 Energy + 1 [A]; (c) ability 1 gated by `canActivateAbilityAt` on 6+ XP; test: ReviewFixC1Test.Honeyfruit_EntersExhaustedAndTheLevelAbilityIsGated, ReviewFixC1Test.Honeyfruit_PlayedFromHandItIsExhausted
  - Text: "This enters exhausted." / "[Level 6] ... [E]: [Add] [1][A]. (Use this ability only while you have 6+ XP.)"
  - Code: (a) exhausts in onPlay, but resolvePermanent then sets `is_exhausted = gear_def->entersExhausted()` (game_engine.cpp:2186), which is not overridden, so it enters ready. (b) line 54 adds 2 universal power instead of 1 Energy + 1 [A]. (c) ability 1 is offered below 6 XP; activating it exhausts the gear and returns at line 53.
  - Scenario: Honeyfruit is tapped for [A] the turn it is played; at 6 XP it yields [A][A]; at 3 XP the agent can pick ability 1 and waste the gear.

- [x] **474 Eye of the Herald** - `gear/0474_eye_of_the_herald.cpp:23` [confirmed] medium — FIXED: the Recruit enters exhausted, at the bearer's location; test: ReviewFixC1Test.EyeOfTheHerald_TheRecruitEntersExhausted
  - Text: "When I move, play a 1 [M] Recruit unit token here."
  - Code: createToken(..., loc, true) - the last argument is enter_ready.
  - Scenario: equipped unit moves to a battlefield -> the Recruit arrives ready and can act immediately (units enter exhausted, CR 143.4).

- [x] **504 Spinning Axe** - `gear/0504_spinning_axe.cpp:15` [confirmed] medium — FIXED: the Beginning Step no longer kills [Temporary] gear that is attached (CR 718.2; one line in `GameEngine::beginningStep`, `shared:` commit); the Quick-Draw half was fixed by 08 item 8.12; test: ReviewFixC1Test.SpinningAxe_TemporaryOnlyKillsItWhileUnattached
  - Text: "[Temporary] (If this is unattached, kill it at the start of its controller's Beginning Phase)" / "[Quick-Draw] (... When you play it, attach it to a unit you control.)"
  - Code: the Beginning Step kills every [Temporary] gear with no attached check (game_engine.cpp:761-771); Quick-Draw attach only happens for the closed-state PlayReaction intent that carries a target (game_engine.cpp:2195, 3056) - a main-phase play has no target and does not attach.
  - Scenario: Axe attached to Draven is destroyed at the controller's next Beginning Phase; Axe played in the main phase sits unattached in base.

- [x] **504/507/509 UniversalEquipGear (Spinning Axe, Forgefire Cape, Shurelya's Requiem)** - `gear/equip_base.h:107` [confirmed] medium — ALREADY FIXED: `f892d67` (03 item 3.23) — `UniversalEquipGear` pays through `standardEquip` (`payEnergy` + `payOnePower`, all-or-nothing); test: ChoiceHelperTest.EquipCostsCountAsPowerSpentAndAreAllOrNothing
  - Text: "[Equip] [A]"
  - Code: no affordability check - if no rune is in base the recycle loop does nothing and the gear attaches anyway. Floating power is never used; the first rune in map order is always recycled.
  - Scenario: player with zero runes on board equips for free; player holding floating [A] from a Gold token still loses a rune.

- [x] **SimpleEquipGear cards (396, 408, 417, 424, 430, 437, 439, 445, 454, 455, 474, 482, 493, 581, 658, 798, 814, 860; also 460, 471, 924, and the private copies in 412, 498, 748)** - `gear/equip_base.h:30` [confirmed] medium — ALREADY FIXED for the base and for 460 / 471 / 924 (`f892d67`, 03 item 3.23: floating Energy and Power pay); FIXED here for the private copies in 412, 498 and 748, which now pay through `standardEquip` / `payOnePower`; test: ChoiceHelperTest.EquipCostsCountAsPowerSpentAndAreAllOrNothing, ReviewFixC1Test.HextechGauntlets_EquipIsPaidWithFloatingResources, ReviewFixC1Test.BladeOfTheRuinedKing_TheControllerChoosesWhichUnitDies
  - Text: "[Equip] [domain]"
  - Code: the energy+Domain standardEquip only accepts recycling a rune of that domain in base; floating domain / universal power in the rune pool is ignored (the optional-domain overload in card_helpers.h does handle it), and Energy is paid only by exhausting runes, not from floating Energy.
  - Scenario: player taps Seal of Rage for [R], then tries Equip [R] on Pendulum Blade with no Fury rune in base -> equip refused; with a Fury rune it is recycled and the floating [R] is wasted.

- [x] **759 Keeper of the Hammer** - `legends/0759_keeper_of_the_hammer.cpp:17` [confirmed] medium — FIXED: `WhenIHold`; test: ReviewFixC1Test.KeeperOfTheHammer_GainsXpOnAHoldOnly
  - Text: "When you hold, gain 1 XP."
  - Code: trigger type is WhenIConquerOrHold.
  - Scenario: Poppy player conquers a battlefield -> gains 1 XP that the card does not give.

- [x] **412 The Zero Drive** - `gear/0412_the_zero_drive.cpp:27` [confirmed; power cost documented] medium — FIXED: (a) an equipped Deathknell (`equippedTriggerType` WhenIDie) banishes the dead bearer from the trash; (b) a token bearer is neither banished nor recorded; (c) cost is [3] + Mind power; (d) `canActivateAbilityAt` looks at the activating copy; test: AuditFix0Test.ZeroDrive_DeathknellBanishesAndRecords, ReviewFixC1Test.ZeroDrive_BearerDiesAndItsOwnDeathTriggersFire, ReviewFixC1Test.ZeroDrive_ATokenBearerIsNotRecordedOrReplayed, ReviewFixC1Test.ZeroDrive_ReplayCostsThreeEnergyAndAMindPower
  - Text: "[Deathknell] - Banish me." / "[3][B], Banish this: Play all units banished with this"
  - Code: (a) implemented as a death replacement, so the bearer never dies: no UnitDiedEvent, none of its own Deathknells, no "when a friendly unit dies" triggers. (b) a token bearer is recorded in tracked_objects and later re-played by playIgnoringCost although it ceased to exist. (c) line 54: only [3] Energy is charged, the [B] power is not. (d) hasLegalTargets (line 58) inspects the first object with this def id regardless of controller or which copy is activating.
  - Scenario: equipped Recruit token dies -> no death triggers; activating the Drive puts the dead token back on the board, for 3 Energy and no Mind power.

- [x] **405 Hextech Anomaly** - `gear/0405_hextech_anomaly.cpp:19` [confirmed] medium — FIXED: `isReactionAbility()`; X ranges over `payableAnyPower` (capped at 15) and is paid with `payAnyPower`; test: ReviewFixC1Test.HextechAnomaly_IsAReactionAndReadyRunesCanPayX
  - Text: "[E]: [Reaction] - Pay any amount of [A] to [Add] that much Energy."
  - Code: `isReactionAbility()` is not overridden (default false), so the ability is only generated in the main-phase list. Max X counts only floating power + exhausted runes (line 33), though ready runes can also be recycled; X is not capped at the 16 int choice slots.
  - Scenario: player cannot convert power to Energy in response to a spell or during a showdown; with 3 ready runes and nothing floating the only option is X = 0.

- [x] **498 Blade of the Ruined King** - `gear/0498_blade_of_the_ruined_king.cpp:51` [confirmed] medium — FIXED: the victim is a direct agent decision (`chooseObjectNow`), [Y] is paid with `payOnePower`, all-or-nothing; test: ReviewFixC1Test.BladeOfTheRuinedKing_TheControllerChoosesWhichUnitDies, ReviewFixC1Test.BladeOfTheRuinedKing_NothingIsPaidWithoutThePower
  - Text: "[Equip] - [Y], Kill a friendly unit"
  - Code: pickTarget is called from onEquip, outside the chain, so it returns the first unit in the list without asking.
  - Scenario: player with a Recruit token and a champion equips the Blade to a third unit -> whichever comes first in unordered_map order dies, possibly the champion.

- [x] **396 Experimental Hexplate** - `gear/0396_experimental_hexplate.cpp:38` [confirmed] medium — FIXED: the bearer gets the Mech tag while this is attached (on equip, re-asserted by the aura hook, removed in `onUnattached`); test: ReviewFixC1Test.ExperimentalHexplate_BearerIsAMechWhileItIsAttached, ReviewFixC1Test.ExperimentalHexplate_AttachedByAnotherEffectStillMakesAMech
  - Text: "I am a Mech."
  - Code: nothing adds the Mech tag to the bearer (no hook in the card, no engine handling of the effect_text).
  - Scenario: equipped unit is ignored by Mechanized Menace, Rumble Hotheaded, Bubble Bot, Breakneck Mech and "Your Mechs each have".

- [ ] **294 Daughter of the Void / 506 Fire Below the Mountain / 786 Scorn of the Moon** - `legends/0294_daughter_of_the_void.cpp:24`, `legends/0506_fire_below_the_mountain.cpp:24`, `legends/0786_scorn_of_the_moon.cpp:22` [confirmed; 506 and 786 documented] medium
  - Text: "Use only to play spells." / "Use only to play gear or use gear abilities." / "Spend this Energy only during showdowns."
  - Code: plain addFloatingUniversalPower / addFloatingEnergy with no spend restriction (only unit-only Energy has a restricted pool).
  - Scenario: Kai'Sa's [A] pays for a unit; Ornn's [A] pays for a spell; Diana's [1] is spent in the main phase.
  - BLOCKED: needs restricted pools in `RunePool` beside `unit_energy` (Power usable only for spells; Power usable only for gear and gear abilities; Energy usable only during showdowns), honoured by `payCardCost` / `payActivationResources` / `canAfford`. The three cards are one line each once the pools exist.

- [x] **264 The Boss** - `legends/0264_the_boss.cpp:44` [documented] medium — FIXED: the replacement asks its may (`confirmNow`), only when [C] (Body or Order power) can be paid and the legend is ready, pays it, and clears the unit's combat designation; test: ReviewFixC1Test.TheBoss_TheReplacementCanBeDeclined, ReviewFixC1Test.TheBoss_NeedsThePowerToPay, ReviewFixC1Test.TheBoss_PaysAndRecallsOutOfCombat, RulesComplianceTest.DeathReplacement_TheBoss_SavesABuffedUnit
  - Text: "you may pay [C], exhaust me, and spend its buff to heal it, exhaust it, and recall it instead."
  - Code: [C] is never charged and the "may" is never asked - the replacement is automatic whenever the legend is ready. The recalled unit also keeps its combat_designation.
  - Scenario: buffed unit dies with the Sett player holding no power -> it is saved anyway; a player who wants the unit to die (Deathknell) cannot decline.

- [x] **321 Might of Demacia - Starter** - `legends/0321_might_of_demacia_starter.cpp:29` [documented] medium — FIXED: counts units at the conquered battlefield (the trigger's subject), as a trigger condition and again at resolution; test: ReviewFixC1Test.MightOfDemacia_CountsUnitsAtTheConqueredBattlefield, CardTestFixture.MightOfDemacia_DrawsTwoOnConquerWith4Units
  - Text: "When you conquer, if you have 4+ units at that battlefield, draw 2."
  - Code: checks every battlefield in battlefields_scored_this_turn (which includes Holds) instead of the conquered one; the trigger's subject (the scored battlefield card) is now available but unused.
  - Scenario: hold battlefield A with 4 units, then conquer battlefield B with 1 unit -> draws 2.

- [x] **700 The List** - `gear/0700_the_list.cpp:28` [documented] medium — FIXED: naming the tag is an `AsYouPlayMe` choice by the controller among the tags in public view; the ability is offered and resolved against the activating copy's named tag (source-aware `enumerateLegalTargets`); test: ReviewFixC1Test.TheList_TheControllerNamesTheTag, ReviewFixC1Test.TheList_IsNotOfferedWithNoUnitOfTheNamedTag
  - Text: "As you play this, name a tag."
  - Code: the tag is the most common tag among enemy units on board; the player never chooses, and if the opponent has no units no tag is ever named.
  - Scenario: played on an empty enemy board -> the gear can never do anything; the ability is still offered (broad gate, line 60) and exhausts for nothing.

- [x] **784 Wuju Master** - `legends/0784_wuju_master.cpp:48` [documented] medium — FIXED: [Level 11] lists the legend in `PlayerState::units_enter_ready_by`, so every unit its controller plays enters ready; test: SweepFlowTest.WujuMaster_LevelElevenUnitsEnterReady
  - Text: "[Level 11][>] Your units enter ready."
  - Code: not implemented.
  - Scenario: at 11 XP units still enter exhausted.

- [x] **319 Lady of Luminosity - Starter** - `legends/0319_lady_of_luminosity_starter.cpp:23` [plausible] low — FIXED: confirmed — reads the played spell from the trigger's subject instead of the newest spell in the trash; test: ReviewFixC1Test.LadyOfLuminosity_LooksAtTheSpellThatWasPlayed
  - Text: "When you play a spell that costs [5] or more, draw 1."
  - Code: looks up the most recent spell in the controller's trash rather than the triggering spell (chain item's triggering_subject).
  - Scenario: a 6-cost spell played for Flow is banished, so the scan finds an older 2-cost spell -> no draw (or the reverse).

- [x] **736 Shard of Undoing** - `gear/0736_shard_of_undoing.cpp:50` [documented] low — FIXED: the opponent chooses which of their units dies; the Beginning-Phase gate is a trigger condition; test: WaveABatch3Test.ShardKillsOpponentUnitInBeginningPhase, WaveABatch3Test.ShardOnlyFiresOncePerTurn
  - Text: "each opponent must kill one of their units."
  - Code: kills the opponent's lowest-Might unit; the opponent is not asked.
  - Scenario: opponent would rather lose a 4 [M] exhausted unit than a 1 [M] unit holding a battlefield.

- [x] **489 Vanguard Armory** - `gear/0489_vanguard_armory.cpp:23` [confirmed] low — FIXED: each token goes to the base or a controlled battlefield the controller picks; test: ReviewFixC1Test.VanguardArmory_TokensGoWhereTheControllerPutsThem, ReviewFixC1Test.VanguardArmory_WithOnlyABaseNothingIsAsked
  - Text: "Play three 1 [M] Recruit unit tokens. (You may play them to different locations.)"
  - Code: all three always go to base.
  - Scenario: player controlling a battlefield cannot reinforce it with the tokens.

- [x] **650 Gutter Palace / 426 Petricite Monument** - `gear/0650_gutter_palace.cpp:70`, `gear/0426_petricite_monument.cpp:55` [confirmed] low — FIXED: Deflect removed from both gear defs; test: ReviewFixC1Test.GearDefs_DeflectBelongsToTheUnitsNotTheGear
  - Text: Deflect belongs to the Bird token / to friendly units, not to the gear.
  - Code: the gear's own def sets `deflect_value = 1` and Keyword::Deflect, which instantiateCard copies onto the gear object.
  - Scenario: opponent must pay [A] to choose the gear itself with a spell or ability.

- [x] **509 Shurelya's Requiem** - `gear/0509_shurelya_s_requiem.cpp:36` [plausible] low — FIXED: confirmed — "here" is now read from the bearer; the executor moves also carry gear since 02 item 2.22; test: ReviewFixC1Test.ShurelyasRequiem_HereIsWhereTheBearerIs
  - Text: "Your units here have [Ganking]."
  - Code: "here" is read from the gear's own location, which only GameEngine::moveUnit keeps in step with the bearer; executor.moveToBase / moveToBattlefield do not move attachments.
  - Scenario: bearer is moved by an effect -> the aura stays at the old battlefield.
  - verify-cards: the carry-over finding "the hook grants Ganking here but the def has no such sentence" is NOT A DEFECT: the sentence is the Equipment's `effect_text`; pinned by VerifyCardsTest.Pin_Shurelya509_UnitsWhereTheBearerIsHaveGanking; clarified in gear/0509_shurelya_s_requiem.cpp

## Systemic
- S2: 490 Altar of Memories, 256 Unforgiven, 746 Bloodharbor Ripper, 254 Blind Monk, 864 Tools of Empire
  - c1: 490, 256, 746, 864 FIXED (on-board checks in the card; tests under their entries above). 254 Blind Monk ALREADY FIXED: the engine drops a target that is no longer legal before `onActivate` and `buffUnit` is a no-op off the board (02 item 2.28).
- S3: 601 Soul Sword
  - c1: ALREADY FIXED by 02 item 2.2 (`applyPassiveAura` is called once per card and controller; Soul Sword's hook visits each of its own instances once).
- S4: 430 Warmog's Armor, 254 Blind Monk, 297 Radiant Dawn
  - c1: ALREADY FIXED by 02 item 2.14 (`buffUnit` places a Buff only on a unit without one); none of the three prints "if it was buffed this way".
- S5: 460 Edge of Night, 636 Frigid Jewel, 297 Radiant Dawn, 315 Dark Child - Starter, 471 Last Rites (equip cost), 554 Grandmaster at Arms (which Equipment)
  - c1: 460, 636, 297 FIXED (`pickTarget` + `kFinalizeTrigger`; tests: ReviewFixC1Test.EdgeOfNight_TheControllerChoosesTheUnitHere, PerTurnTest.FrigidJewel_BuffsAFriendlyUnitOnSecondDraw, ReviewFixC1Test.RadiantDawn_TheControllerChoosesTheUnitToBuff); 471 and 554 FIXED (see their entries). 315 Dark Child NOT A DEFECT as a choice: an exhausted rune is worth the same whatever its Domain (ready it gives [1]; it is recycled for Power ready or exhausted, CR 164.2.b) and readying fewer is never better — the pick was made independent of map order; test: ReviewFixC1Test.DarkChild_ReadiesTwoRunes. Pinned by VerifyCardsTest.Pin_Batch01_DarkChild315_ReadiesTwoRunesWithoutAsking; clarified in legends/0315_dark_child_starter.cpp (comment in `onTrigger`).
- S6: 490 Altar of Memories (pickMode after pickTarget -> always "top")
  - c1: ALREADY FIXED by 03 item 3.1 (helpers compose in any order); test: EnginePathTest.DeathTriggerWithMayTargetAndModeResolvesThroughTheEngine.

## Not read
None
