[Back to the full card review](../08-cards-full-review.md)

# Batch 00 - battlefields/0270_altar_to_unity.cpp .. gear/0387_cloth_armor.cpp

Files read in full: 83 of 83.

## Findings
- [x] **527 Ornn's Forge** - `battlefields/0527_ornn_s_forge.cpp:27` [confirmed] high — ALREADY FIXED: `1ae4c88` (aura cost modifiers are tagged `from_aura` and rebuilt every pass; test: AurasCostsTest.OrnnsForge_OneModifierHoweverManyCleanups). c0 on top: each Forge gives its own controller one modifier when both players bring it; test: C0Test.SameBattlefieldTwice_ControllerScopedDiscounts
  - Text: "While you control this battlefield, the first friendly non-token gear played each turn costs [1] less."
  - Code: `applyPassiveAura` pushes a new `CostModifier{energy_reduction=1, gear_only, first_gear_per_turn, this_turn_only=false}` onto the controller's `cost_modifiers` on EVERY `recalculateAuras` pass. Nothing ever removes it: `recalculateAuras` (game_engine.cpp:4389-4455) does not clear `cost_modifiers`, `resetTurnTracking` (game_state.h:295) erases only `this_turn_only` entries, and `payCardCost` (game_engine.cpp:5877) erases only next_spell/next_unit/next_card entries.
  - Scenario: P1 controls Ornn's Forge; cleanup runs 10 times during the turn -> 10 stacked modifiers -> P1's first gear each turn costs 10 less (free). P1 then loses the battlefield -> all the modifiers stay forever, and P2 starts accumulating their own.

- [x] **17 Iron Ballista** - `gear/0017_iron_ballista.cpp:20` [confirmed] high — FIXED: overrides `entersExhausted()`; the `onPlay` exhaust is gone; test: C0Test.IronBallista_EntersExhausted
  - Text: "This enters exhausted."
  - Code: exhausts itself in `onPlay`. `executePlayCard` calls `onPlay` (game_engine.cpp:1716) BEFORE the permanent resolves; `resolvePermanent` then runs `card.is_exhausted = gear_def && gear_def->entersExhausted();` (game_engine.cpp:2183-2186), which resets it to ready because the card does not override `Card::entersExhausted()` (only 849 and 862 do).
  - Scenario: play Iron Ballista for [3] -> it is ready -> "[E]: Deal 2 to a unit at a battlefield" is usable the same turn.

- [x] **375 Heart of Dark Ice** - `gear/0375_heart_of_dark_ice.cpp:18` [confirmed] high — FIXED: `enumerateLegalTargets(state, controller, ability_index)` returns every chooseable unit, so the ability is generated and its target is chosen at finalization; test: C0Test.HeartOfDarkIce_IsOfferedAndGivesTheChosenUnitMight
  - Text: "[E]: Give a unit +3 [M] this turn."
  - Code: declares `targets.count = 1` on the ability but overrides neither `getTargetRequirements()` nor any `enumerateLegalTargets` overload. The generator (game_engine.cpp:3552-3556) calls `enumerateLegalTargets(state, player, ai, id)`, which falls through to the 2-arg default; that returns `{}` when `getTargetRequirements().count == 0` (card.cpp:632-633) -> `legal.empty()` -> `continue`. Same empty list is what `onActivate` would hand to `pickTarget`.
  - Scenario: Heart of Dark Ice is ready with units on board -> no ActivateAbility intent is ever generated; the card does nothing after being played.

- [x] **186 Treasure Trove** - `gear/0186_treasure_trove.cpp:20` [confirmed] high (the bounce gap is documented; the total failure is not) — FIXED: registered on the new `TriggerType::WhenILeaveTheBoard`, fired from `LeftBoardEvent` for a kill, a bounce, a recycle or a banish (`shared:` 776a84b); its own "Kill this" checks the Trove is on the board; test: C0Test.TreasureTrove_TriggersHoweverItLeavesTheBoard
  - Text: "When this leaves the board, draw 1 and channel 1 rune exhausted. / [P], [E]: Kill this."
  - Code: registered on `TriggerType::WhenIDie`. `WhenIDie` is dispatched only from `TriggerManager::onUnitDied` (trigger_manager.cpp:640), and `UnitDiedEvent` is emitted only for units (effect_executor.cpp:261 inside `if (obj.isUnit())`, game_engine.cpp:6419 in `killUnit`). The gear branch of `killObject` (effect_executor.cpp:273-289) emits only `LeftBoardEvent`, which `TriggerManager` does not subscribe to.
  - Scenario: pay [P], exhaust, "Kill this" -> Treasure Trove goes to trash, no draw, no rune channeled. Same for any other way it is killed or bounced.

- [x] **101 Mushroom Pouch** - `gear/0101_mushroom_pouch.cpp:23` [confirmed] high — FIXED: looks for `is_hidden && hidden_at != kInvalidId` (a hidden card has no board location); the "if" is also the trigger condition; test: C0Test.MushroomPouch_SeesACardHiddenAtABattlefield
  - Text: "At the start of your Beginning Phase, if you control a facedown card at a battlefield, draw 1."
  - Code: requires `obj.is_hidden && obj.isAtBattlefield()`. A hidden card is stored with `zone = FacedownZone`, `location = std::nullopt`, `hidden_at = bf` (game_engine.cpp:2520-2523), and `is_hidden` is cleared whenever it leaves that zone (2542, 5115), so `isAtBattlefield()` (needs a BattlefieldLocation) is never true for a hidden card.
  - Scenario: controller has a card hidden at a battlefield at the start of their Beginning Phase -> no draw, ever.

- [x] **332 Serrated Dirk / 387 Cloth Armor** - `gear/0332_serrated_dirk.cpp:19`, `gear/0387_cloth_armor.cpp:19` [confirmed] high — ALREADY FIXED: `73e121e` (the aura fold adds an aura-granted [Assault N] / [Shield N]); test: AurasCostsTest.EquippedAssaultAndShield_AddMightInCombat
  - Text: "[Assault 2] (+2 [M] while I'm an attacker.)" / "[Shield 2] (+2 [M] while I'm a defender.)"
  - Code: `equippedAssault()/equippedShield()` become an `AuraEffect{keyword, keyword_value}` with `might_bonus = 0` (game_engine.cpp:4818-4833). The aura fold (4843-4850) only sums `might_bonus` and sets the keyword bit; `recomputeMight` (game_object.h:325-331) adds the object's own `assault_value` / `shield_value`, which the aura never touches. `keyword_value` is read nowhere for Assault/Shield (only Deflect, effect_executor.cpp:370).
  - Scenario: a 2 [M] unit with Serrated Dirk attacks -> Might stays 2 instead of 4. A unit with Cloth Armor defends -> no +2. Both cards have no other effect.

- [x] **764 Black Flame Altar** - `battlefields/0764_black_flame_altar.cpp:37` [confirmed] high — ALREADY FIXED: `73e121e` (engine fold; test: AurasCostsTest.AuraAssaultShield_PurifierAndBlackFlameAltar). c0 on top: goes through `auraKeyword`, sees [Temporary] granted earlier in the pass (`hasKeywordInAuraPass`), one [Shield] per Altar when both players bring it; test: C0Test.SameBattlefieldTwice_VoidGateWasteAltarAndGardens
  - Text: "Units here with [Temporary] have [Shield]. (+1 [M] while they're defenders.)"
  - Code: pushes a raw `AuraEffect{keyword=Shield, keyword_value=1}` with no `might_bonus` (it does not use `auraKeyword`, which adds the conditional Might). Same fold as above: the unit gets the keyword bit and no Might.
  - Scenario: a [Temporary] unit defends at Black Flame Altar -> no +1 [M].

- [x] **345 Long Sword / 379 Sterak's Gage / 387 Cloth Armor** - `gear/0345_long_sword.cpp:38`, `gear/0379_sterak_s_gage.cpp:38`, `gear/0387_cloth_armor.cpp:37` [confirmed] high — ALREADY FIXED: `a19e96a` (08 item 8.12: `resolvePermanent` attaches [Quick-Draw] gear whenever it is played; showdowns offer it); test: PlayCombatFixTest.QuickDraw_PlayedInTheMainPhase_AttachesToAUnitYouChoose, .QuickDraw_AtReactionTiming_IsOneActionAndAttaches
  - Text: "[Quick-Draw] (This has [Reaction]. When you play it, attach it to a unit you control.)"
  - Code: the attach happens only when the play intent carries a target (`resolvePermanent`, game_engine.cpp:2195), and only `generateClosedStateActions` emits such intents (3052-3068). The main-phase gear generator (2789-2802) emits a plain `PlayCard` to base with no target, and `generateShowdownActions` has no Quick-Draw branch at all.
  - Scenario: play Long Sword in your main phase -> it lands in base unattached, +2 [M] goes to nobody, and Equip [R] must be paid separately. During a showdown (open state) the gear cannot be played at all.

- [ ] **382 Svellsongur** - `gear/0382_svellsongur.cpp:19` [confirmed] high
  - Text: "As this is attached to a unit, copy that unit's text to this Equipment's effect text for as long as this is attached to it."
  - Code: only `hasEquipAbility` / `onEquip` (standardEquip [1][G]). No copy logic in the card and nothing in src/engine or src/core references it; no comment marks it as unimplemented.
  - Scenario: attach Svellsongur to any unit with abilities -> nothing is copied; the 3-energy + [G] Equipment (no Might bonus) has no effect.
  - BLOCKED: needs engine support. An Equipment's effect text is asked of the gear's `Card` through fixed, state-free descriptors (`equippedTriggerType()`, `equippedKeywords()`, `equippedAssault()` ...) and its bearer's own triggers, auras and activated abilities are dispatched by the bearer's card id; "copy that unit's text" needs the trigger / aura / ability dispatch to run the bearer's card a second time on behalf of the gear. The card now says so in a comment; nothing else changed.
  - sweep-flow: NOT DONE, still BLOCKED. The flow stream's functions cannot carry it alone: "the bearer has each of its abilities twice" needs (1) `TriggerManager` to queue the bearer's own triggered abilities once more per attached text-copying Equipment (trigger_manager.cpp, sweep-triggers — every `fireTrigger` site for a unit's own trigger), and (2) a per-INSTANCE aura hook, because `Card::applyPassiveAura` is called once per (card, controller) and covers every instance, so "apply this one unit's passive twice" cannot be expressed today. Proposed shape: `Card::copiesBearerText()` on the gear; `recalculateAuras` counts such gear into a new `GameObject::text_copies` before the hooks; `forEachSelfOnce` (card_helpers.h) runs its body `1 + text_copies` times for that instance, hand-rolled hooks are converted to it; `TriggerManager::fireTrigger` repeats the queue `1 + text_copies` times. No vocabulary or feature-layout change is needed.

- [x] **775 Vaults of Helia** - `battlefields/0775_vaults_of_helia.cpp:21` [confirmed] high — ALREADY FIXED: `1ae4c88` (`CostModifier::units_only`); test: AurasCostsTest.VaultsOfHelia_ModifierIsUnitsOnly
  - Text: "When you hold here, your non-token units cost [1] more to play this turn."
  - Code: `CostModifier{energy_increase=1, affects_non_token_only, this_turn_only}` with no card-type restriction. `canAfford` / `payCardCost` (game_engine.cpp:5638-5648, 5817-5826) apply it to every card the player plays; the only unit filter available, `next_unit_only`, is not set (and would expire after one unit).
  - Scenario: hold Vaults of Helia, then play a spell or gear that turn -> it costs [1] more.

- [x] **771 Star Spring** - `battlefields/0771_star_spring.cpp:29` [confirmed] medium — FIXED: the played unit is the trigger's subject (`shared:` 776a84b), tokens do not trigger it, "first time each turn" is a trigger condition per player, and the player picks which other unit moves (`pickTarget` + `kFinalizeTrigger`); test: C0Test.StarSpring_MovesAnotherUnitNotTheOnePlayed, .StarSpring_DecliningStillUsesUpTheFirstTime, AuditFix7Test.StarSpring_* (corrected to the engine path)
  - Text: "The first time a player plays a non-token unit here each turn, they may move another unit they control here to its base."
  - Code: the battlefield trigger carries no subject (`fireBattlefieldTriggers`, trigger_manager.cpp:163-171, 365), so the card guesses the played unit as the highest GameObjectId among the player's non-token units here. Hand cards get their ids at setup, so the just-played unit is often not the highest. `movable` is then "everything except the guess", and the move takes the first entry in map order (lines 65-70) with no player choice.
  - Scenario: unit A (id 80) is at Star Spring; P plays unit B (id 35) there and says yes -> guess = A, movable = {B} -> the unit just played is sent to base instead of A.

- [x] **182 Scrapheap** - `gear/0182_scrapheap.cpp:23` [confirmed, discard half documented] medium — FIXED: also registers `WhenIAmDiscarded`; the killed branch works since 8.5 (`e8a0f70`); test: C0Test.Scrapheap_DrawsWhenDiscardedAndWhenKilled
  - Text: "When this is played, discarded, or killed, draw 1."
  - Code: triggers are `WhenYouPlayThis` and `WhenIDie`. `WhenIDie` never fires for gear (see Treasure Trove). The comment says the discard branch cannot be wired, but `TriggerType::WhenIAmDiscarded` exists and is dispatched from the "discarded" state event (trigger_manager.cpp:1134-1139); the card does not register it.
  - Scenario: Scrapheap is killed, or discarded from hand -> no draw. Only the "played" third works.

- [x] **124 Arena Bar** - `gear/0124_arena_bar.cpp:22` [confirmed] medium — FIXED: `enumerateLegalTargets` keeps only exhausted friendly units on the board; the engine re-checks the target against it at resolution; test: C0Test.ArenaBar_OnlyAnExhaustedFriendlyUnit
  - Text: "[E]: Buff an exhausted friendly unit."
  - Code: `TargetRequirements{count=1, must_be_unit, must_be_friendly}`; the struct has no exhausted filter and the card does not override `enumerateLegalTargets` or check in `onActivate`.
  - Scenario: Arena Bar buffs a ready friendly unit (e.g. one about to attack).

- [x] **369 Poro Snax** - `gear/0369_poro_snax.cpp:24` [documented] medium — FIXED: cost is `{exhaust, energy 1, power 1 Calm}`; "Kill this" is paid at finalization (`kFinalizeActivate` + `costDueNow`) with `killObject`, so it goes to the trash; test: C0Test.PoroSnax_CostsCalmPowerAndIsKilledNotRecycled
  - Text: "[1][G], [E], Kill this: Draw 1."
  - Code: cost is `{exhaust, energy=1, recycle_self}`. The [G] power is never charged (`ActivationCost::power`/`power_domain` are available and paid by the engine), and `recycle_self` puts the card on the bottom of the Main Deck (game_engine.cpp:1463-1468) instead of killing it to trash.
  - Scenario: activate with no Calm power available -> allowed, and Poro Snax returns to the deck to be drawn again instead of going to trash.

- [x] **271 Aspirant's Climb** - `battlefields/0271_aspirant_s_climb.cpp:18` [confirmed] medium — ALREADY FIXED: `a19e96a` (02 item 2.19: `isWinningPointAttempt` uses `effectiveVictoryScore()`); test: ScoringFixTest.FinalPointCheck_UsesTheEffectiveVictoryScore
  - Text: "Increase the points needed to win the game by 1."
  - Code: `victoryScoreModifier()` feeds `effectiveVictoryScore()` (used by `checkWinCondition` and `scorePoints`), but the conquer path's `isWinningPointAttempt` still tests `score >= state_.mode.victory_score - 1` (game_engine.cpp:4376).
  - Scenario: with Aspirant's Climb in play (9 to win) a player on 7 conquers one battlefield -> treated as the winning point, blocked unless every battlefield was scored this turn, and they draw 1 instead of going to 8.

- [ ] **290 Vilemaw's Lair** - `battlefields/0290_vilemaw_s_lair.cpp:14` [confirmed] medium
  - Text: "Units can't move from here to base."
  - Code: `blocks_move_to_base` is read only by the standard-move generator (game_engine.cpp:2642). `EffectExecutor::moveToBase` (effect_executor.cpp:735-752) does not consult it.
  - Scenario: The Syren (184, same batch) "[1], [E]: Move a friendly unit at a battlefield to its base" targets a unit at Vilemaw's Lair -> the unit moves to base.
  - BLOCKED: `EffectExecutor::moveToBase` is also the engine's recall (a recall cannot be blocked, CR 458.3), so it cannot refuse a blocked move until there is a separate recall primitive (STATUS carry-over, executor row). Done card-side by c0: `moveToBaseBlocked()` in card_helpers.h, used by `moveDests()` and by this stream's move-to-base cards — The Syren 184, Amateur Recital 763, Star Spring 771; test: C0Test.TheSyren_NotFromABattlefieldUnitsCantLeaveForBase, .AmateurRecital_MovesTheChosenUnitToItsBase. Other streams' cards that call `moveToBase` for a real move are not covered.

- [x] **762 Altar of Blood** - `battlefields/0762_altar_of_blood.cpp:23` [confirmed] medium — ALREADY FIXED: `09e104c` (02 item 2.26: the card's `applyReplacement` pays `[A][A][A]` through `payAnyPower`, floating Power and exhausted runes count; AltarOfBloodTest.*). c0 closes the second half: `EffectExecutor::killObject` asks the dying unit's battlefield card too (`shared:` 7422fc9); test: C0Test.AltarOfBlood_IsAskedForASpellKillDuringCombatToo
  - Text: "If a unit here would die during combat, its controller may pay [A][A][A] to heal it, exhaust it, and recall it instead."
  - Code: the flag is read only in `GameEngine::killUnit` (game_engine.cpp:6314-6360), which offers the payment only when the controller has 3 READY runes in base and pays by recycling exactly those; floating power and exhausted runes are ignored. `EffectExecutor::killObject` (spell/ability kills during the combat) never checks the flag.
  - Scenario: defender has 4 runes, 3 exhausted, plus [A] floating -> no offer. A unit killed by a spell during the showdown at Altar of Blood -> no offer.

- [x] **772 The Academy** - `battlefields/0772_the_academy.cpp:23` [confirmed] medium — FIXED (`shared:` 7422fc9, `executePlaySpell`): the granted Repeat cost is the spell's base Energy and Power, and the grant is used up by the next spell even when that spell prints its own [Repeat] (a second, granted instance on such a spell is still not modeled); test: C0Test.TheAcademy_GrantedRepeatCostsTheWholeBaseCost
  - Text: "When you hold here, give your next spell this turn [Repeat] equal to its base cost."
  - Code: the engine consumer (game_engine.cpp:1843-1850) builds the Repeat cost from `energy_cost` only (`repeat_cost.power = 0`), and reads/consumes the flag only when the spell has no printed Repeat.
  - Scenario: next spell costs [2][R] -> its granted Repeat costs [2], not [2][R]. If the next spell already has [Repeat], the grant is not consumed and passes to the spell after it.

- [ ] **Equip cost (332, 339, 345, 353, 356, 365, 374, 379, 382, 387)** - `gear/equip_base.h:30` [confirmed] medium
  - Text: "[Equip] [R]" / "[Equip] [1][G]" etc.
  - Code: the energy+Domain `standardEquip` only counts ready runes in base for Energy and requires a rune of the domain on board for Power; it never looks at `rune_pool` (floating Energy/Power). The equip generator (game_engine.cpp:2822-2854) offers the intent without any affordability check, and `onEquip` returning false changes nothing.
  - Scenario: Seal of Rage adds [R], controller has no Fury rune in base -> Equip [R] on Serrated Dirk is offered, chosen, and silently does nothing; it can be chosen again indefinitely.
  - c0: the payment half is ALREADY FIXED (`f892d67`: `standardEquip` checks and pays through `payableEnergy` / `payableOnePower` / `payEnergy` / `payOnePower`, so floating Energy and Power count — the Seal of Rage scenario works). BLOCKED for the rest: the equip generator in `generateMainPhaseActions` still offers Equip with no affordability check and a failed `onEquip` is a free no-op that can be repeated; it needs the equip-cost descriptor on `Card` that STATUS carry-over assigns to the sweep (7.7 row).

- [x] **287 The Dreaming Tree** - `battlefields/0287_the_dreaming_tree.cpp:23` [confirmed] medium — FIXED: `triggerCondition` looks at the item that made the choice (the one resolving / being finalized, else the chain item listing the unit) and triggers only for a spell; stale "inert" comment removed; test: C0Test.DreamingTree_OnlyASpellsChoiceDraws
  - Text: "When a player chooses a friendly unit here with a spell for the first time each turn, they draw 1."
  - Code: comment says the trigger is inert, but `WhenAFriendlyUnitChosenHere` is now fired from the "chosen_by_controller" event (trigger_manager.cpp:1121-1122), which is emitted for activated abilities as well as spells (chain_manager.cpp:138, card.cpp:335). The card does not check that the chooser was a spell.
  - Scenario: a player targets their own unit at The Dreaming Tree with a gear ability (Orb of Regret, The Syren) -> they draw 1.

- [x] **276 Hallowed Tomb** - `battlefields/0276_hallowed_tomb.cpp:28` [confirmed] medium — FIXED: `confirmOptional` + `kFinalizeTrigger`; returns `PlayerState::chosen_champion` (new field set at setup, `shared:` 7422fc9), not the first champion unit in the trash; test: C0Test.HallowedTomb_ReturnsTheChosenChampionAndCanBeDeclined
  - Text: "When you hold here, you may return your Chosen Champion from your trash to your Champion Zone if it is empty."
  - Code: no `confirmOptional` (always returns it), and it takes the first `isUnit() && isChampion()` card in the trash; nothing identifies the Chosen Champion.
  - Scenario: trash holds a main-deck copy of the champion unit and the Chosen Champion -> the wrong card can go to the Champion Zone, and the player cannot decline.

- [x] **23 Unlicensed Armory** - `gear/0023_unlicensed_armory.cpp:51` [documented] medium — FIXED: the ability marks the unit for the turn; the card's own `applyReplacement` asks the "may" (`confirmNow`), takes [C] = one Fury Power (CR 135.2.e.6) and heals / exhausts / recalls; no Power or a "no" lets the unit die; the engine's free `death_replacement_recall_pending` is no longer used. Approximation kept in a comment: the rescue is only consulted while an Unlicensed Armory is still on that player's board; test: C0Test.UnlicensedArmory_SavingTheUnitCostsPowerAndIsOptional, .UnlicensedArmory_CanBeDeclined
  - Text: "The next time it would die this turn, you may pay [C] to heal it, exhaust it, and recall it instead."
  - Code: sets `death_replacement_recall_pending`; the recall then happens automatically with no [C] payment and no choice.
  - Scenario: the chosen unit would die while its controller has no Power to pay -> it is saved anyway.

- [x] **946 Kinkou Temple** - `battlefields/0946_kinkou_temple.cpp:29` [documented] medium — FIXED: `hasKeywordInAuraPass` (printed, "this turn" and granted-this-pass Tank); `recalculateAuras` now grants Equipment keywords before the card hooks (`shared:` 7422fc9); one +1 per Temple; test: C0Test.KinkouTemple_TankFromAnEquipmentCounts, .SameBattlefieldTwice_EachCopyAppliesOnce
  - Text: "Units here with [Tank] have +1 [M]."
  - Code: checks `u.keywords.has(Keyword::Tank)` (printed / this-turn only). Tank granted through auras is not seen; Equipment keywords are added later in the same pass (step 3c).
  - Scenario: a unit wearing Doran's Shield (356, same batch) at Kinkou Temple has [Tank] but gets no +1 [M].

- [x] **365 Brutalizer** - `gear/0365_brutalizer.cpp:22` [confirmed] low — FIXED: the aura pass notices a new bearer and stamps the turn, so a free attach (`attachFree`) counts; `onEquip` no longer stamps; test: C0Test.Brutalizer_BonusAppliesHoweverItWasAttached, AuditFix2Test.Brutalizer_* (corrected)
  - Text: "If this was attached to me this turn, I have an additional +2 [M]."
  - Code: the attach turn is stamped only in the card's own `onEquip`. `attachFree` (card_helpers.h:114; used by Jax 440, Rell 347, Forge of the Fluft 522) attaches without calling it.
  - Scenario: Rell attaches Brutalizer for free -> no +2 [M] that turn.

- [x] **181 Pack of Wonders** - `gear/0181_pack_of_wonders.cpp:29` [documented in part] low — FIXED: the 4-argument `enumerateLegalTargets` excludes the Pack itself and includes the controller's facedown cards; a facedown pick is taken out of the battlefield's facedown zone into its owner's hand; test: C0Test.PackOfWonders_NeverItselfAndCanTakeAFacedownCard
  - Text: "[E]: Return another friendly gear, unit, or facedown card to its owner's hand."
  - Code: facedown cards are never legal (they have no location; line 51 notes this). The generator-side `enumerateLegalTargets` (29-40) does not exclude the Pack itself, while `onActivate` does.
  - Scenario: Pack of Wonders is the only friendly permanent -> the ability is offered, exhausts the Pack and does nothing. A hidden card can never be returned.

- [ ] **272 Back-Alley Bar** - `battlefields/0272_back_alley_bar.cpp:23` [confirmed] low
  - Text: "When a unit moves from here, give it +1 [M] this turn."
  - Code: fires on every `UnitMovedEvent` leaving the battlefield. Recalls emit the same event (combat-tie recall game_engine.cpp:4192-4201, death-replacement recalls, `moveToBase`), and a recall is not a move.
  - Scenario: attackers are recalled from Back-Alley Bar after a tied combat -> each gets +1 [M] this turn.
  - c0: the S2 half is FIXED (the bonus goes only to a subject that is still a unit on the board; test: C0Test.BackAlleyBar_OnlyAUnitStillOnTheBoardGetsTheMight). BLOCKED for the recall half: recalls emit the same `UnitMovedEvent` as moves (combat-tie recall, death-replacement recalls, `moveToBase`); the card cannot tell them apart until the engine has a recall primitive / flag (STATUS carry-over, executor row "moveToBase is used for recalls too").

- [x] **143 Pirate's Haven** - `gear/0143_pirate_s_haven.cpp:23` [confirmed] low — FIXED: `triggerCondition` requires the readied object to be a unit; the effect checks it is still a unit on the board; test: C0Test.PiratesHaven_ReadyingARuneDoesNotTrigger
  - Text: "When you ready a friendly unit, give it +1 [M] this turn."
  - Code: `readyObject` emits `UnitReadiedEvent` for any object type (effect_executor.cpp:704-706) and `onUnitReadied` does not filter the subject to units; the card does not check either.
  - Scenario: Targon's Peak readies two runes -> two Pirate's Haven abilities go on the chain and "buff" the runes.

- [ ] **948 Piltovan Forge** - `battlefields/0948_piltovan_forge.cpp:22` [confirmed] low
  - Text: "…the first friendly gear activated ability played each turn costs [1] less."
  - Code: the discount lives in `activationEnergyReduction`, which the Equip path bypasses (`payAndQueueAbility` returns from the equip branch at game_engine.cpp:1407-1423 before costs or the `gear_abilities_activated_this_turn` counter).
  - Scenario: Equip [1][R] on Skyfall of Areion as the first gear ability of the turn -> full price, and the next gear ability still gets the discount.
  - BLOCKED: engine. The Equip branch of `payAndQueueAbility` returns before `activationEnergyReduction` and before `gear_abilities_activated_this_turn` is counted, and an Equip cost is paid inside the gear's `onEquip`, out of the engine's sight. Same root as the Equip-cost item above: needs the equip-cost descriptor (sweep).

- [ ] **950 Risen Altar** - `battlefields/0950_risen_altar.cpp:26` [documented] low
  - Text: "[Empower] costs of your units here cost [1] or [A] less."
  - Code: only the [1] Energy reduction is modeled.
  - Scenario: an Empower cost of [A] only (no Energy) gets no reduction.
  - c0: the per-controller mix-up is FIXED (each Altar discounts only its own controller's units; test: C0Test.SameBattlefieldTwice_ControllerScopedDiscounts). BLOCKED for "or [A] less": `empower_cost_reduction` feeds `Card::activationCostReductionFor`, which reduces Energy only; an [Empower] cost with no Energy (Gangplank, Naval 873: [O][O]; Glowstone 920) needs a Power-side reduction in `canPayActivationResources` / `payActivationResources`. Documented in the card.

- [x] **286 The Candlelit Sanctum** - `battlefields/0286_the_candlelit_sanctum.cpp:22` [confirmed] low — FIXED (`shared:` 7422fc9, `EffectExecutor::predict`): kept cards go back in the order the player names (one prompt per card beyond the first); the default is the order they were in; test: C0Test.CandlelitSanctum_KeptCardsGoBackInTheOrderThePlayerChooses
  - Text: "Put those you don't back in any order."
  - Code: `predict(2)` asks keep/recycle per card and pushes kept cards back in peek order (effect_executor.cpp:1422-1445), so two kept cards always come back swapped with no order choice.
  - Scenario: keep both cards -> the original second card is now on top.

- [x] **99 Garbage Grabber / 342 Assembly Rig** - `gear/0099_garbage_grabber.cpp:31`, `gear/0342_assembly_rig.cpp:36` [confirmed] low — FIXED: both opt into `kFinalizeActivate` and pay the recycle under `costDueNow`, with the cards the player picks (`selectObject`); test: C0Test.GarbageGrabber_RecyclesThePickedCardsAsACost, .GarbageGrabber_TheRecycleHappensAtActivation, .AssemblyRig_RecyclesThePickedUnitAsACost
  - Text: "Recycle 3 from your trash, [1], [E]: Draw 1." / "[1][R], Recycle a unit from your trash, [E]: …"
  - Code: the recycle cost is paid inside `onActivate` at resolution, not at activation with the rest of the cost.
  - Scenario: the opponent responds by emptying the trash -> the ability resolves with [1]/[R] and the exhaust spent and no effect; the recycled cards also stay in trash while the ability is on the chain.

## Systemic
- S2: 272 Back-Alley Bar, 60 Mask of Foresight, 143 Pirate's Haven (subject guarded only by objectExists), 186 Treasure Trove (kills ctx.source guarded only by objectExists)
  - c0: 272, 60 Mask of Foresight, 143 FIXED (subject must still be a unit on the board); 186 FIXED (source must be on the board); test: C0Test.BackAlleyBar_OnlyAUnitStillOnTheBoardGetsTheMight, .MaskOfForesight_IgnoresAUnitThatLeftTheBoard, .PiratesHaven_ReadyingARuneDoesNotTrigger, .TreasureTrove_TriggersHoweverItLeavesTheBoard
- S3: 289 Trifarian War Camp, 291 Void Gate, 946 Kinkou Temple, 766 Forbidding Waste, 764 Black Flame Altar, 292 Windswept Hillock (harmless), 527 Ornn's Forge, 769 Gardens of Becoming (both grants land on the first copy's battlefield, none on the second), 950 Risen Altar (each copy's controller gets the discount at both) - all when both players bring the same battlefield, since the hook runs once per BattlefieldState and each call loops every battlefield with that def; 365 Brutalizer (two copies on board -> +4 each)
  - c0: all nine battlefields FIXED card-side (`hasAuraFrom` per-slot guard; Ornn's Forge by modifier source; Gardens by existing grant; Risen Altar by each slot's controller; Windswept Hillock guarded too); 365 Brutalizer is fixed by the engine's once-per-(card, controller) call (`1ae4c88`) and the card visits each instance; test: C0Test.SameBattlefieldTwice_EachCopyAppliesOnce, .SameBattlefieldTwice_VoidGateWasteAltarAndGardens, .SameBattlefieldTwice_ControllerScopedDiscounts
- S4: 124 Arena Bar
  - c0: 124 Arena Bar — ALREADY FIXED by the executor (`99d179e`: `buffUnit` gives a Buff only to a unit without one)
- S5: 282 Sigil of the Storm (rune to recycle), 276 Hallowed Tomb, 771 Star Spring (unit to move), 339 Recurve Bow (enemy unit here), 99 Garbage Grabber (which 3 trash cards), 342 Assembly Rig (which unit in trash), 284 Targon's Peak (which runes to ready)
  - c0: 282 Sigil of the Storm, 284 Targon's Peak, 99 Garbage Grabber, 342 Assembly Rig FIXED with `selectObject` (the pick is not a rules "choice"); 276, 771 and 339 Recurve Bow FIXED with `pickTarget` + `kFinalizeTrigger`; test: C0Test.SigilOfTheStorm_ThePlayerPicksTheRune, .TargonsPeak_ThePlayerPicksWhichTwoRunesReady, .RecurveBow_ItsControllerPicksTheEnemyUnitHere and the tests named on the items above

## Not read
None
