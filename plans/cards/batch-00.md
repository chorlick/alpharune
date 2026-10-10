[Back to the full card review](../08-cards-full-review.md)

# Batch 00 - battlefields/0270_altar_to_unity.cpp .. gear/0387_cloth_armor.cpp

Files read in full: 83 of 83.

## Findings
- [ ] **527 Ornn's Forge** - `battlefields/0527_ornn_s_forge.cpp:27` [confirmed] high
  - Text: "While you control this battlefield, the first friendly non-token gear played each turn costs [1] less."
  - Code: `applyPassiveAura` pushes a new `CostModifier{energy_reduction=1, gear_only, first_gear_per_turn, this_turn_only=false}` onto the controller's `cost_modifiers` on EVERY `recalculateAuras` pass. Nothing ever removes it: `recalculateAuras` (game_engine.cpp:4389-4455) does not clear `cost_modifiers`, `resetTurnTracking` (game_state.h:295) erases only `this_turn_only` entries, and `payCardCost` (game_engine.cpp:5877) erases only next_spell/next_unit/next_card entries.
  - Scenario: P1 controls Ornn's Forge; cleanup runs 10 times during the turn -> 10 stacked modifiers -> P1's first gear each turn costs 10 less (free). P1 then loses the battlefield -> all the modifiers stay forever, and P2 starts accumulating their own.

- [ ] **17 Iron Ballista** - `gear/0017_iron_ballista.cpp:20` [confirmed] high
  - Text: "This enters exhausted."
  - Code: exhausts itself in `onPlay`. `executePlayCard` calls `onPlay` (game_engine.cpp:1716) BEFORE the permanent resolves; `resolvePermanent` then runs `card.is_exhausted = gear_def && gear_def->entersExhausted();` (game_engine.cpp:2183-2186), which resets it to ready because the card does not override `Card::entersExhausted()` (only 849 and 862 do).
  - Scenario: play Iron Ballista for [3] -> it is ready -> "[E]: Deal 2 to a unit at a battlefield" is usable the same turn.

- [ ] **375 Heart of Dark Ice** - `gear/0375_heart_of_dark_ice.cpp:18` [confirmed] high
  - Text: "[E]: Give a unit +3 [M] this turn."
  - Code: declares `targets.count = 1` on the ability but overrides neither `getTargetRequirements()` nor any `enumerateLegalTargets` overload. The generator (game_engine.cpp:3552-3556) calls `enumerateLegalTargets(state, player, ai, id)`, which falls through to the 2-arg default; that returns `{}` when `getTargetRequirements().count == 0` (card.cpp:632-633) -> `legal.empty()` -> `continue`. Same empty list is what `onActivate` would hand to `pickTarget`.
  - Scenario: Heart of Dark Ice is ready with units on board -> no ActivateAbility intent is ever generated; the card does nothing after being played.

- [ ] **186 Treasure Trove** - `gear/0186_treasure_trove.cpp:20` [confirmed] high (the bounce gap is documented; the total failure is not)
  - Text: "When this leaves the board, draw 1 and channel 1 rune exhausted. / [P], [E]: Kill this."
  - Code: registered on `TriggerType::WhenIDie`. `WhenIDie` is dispatched only from `TriggerManager::onUnitDied` (trigger_manager.cpp:640), and `UnitDiedEvent` is emitted only for units (effect_executor.cpp:261 inside `if (obj.isUnit())`, game_engine.cpp:6419 in `killUnit`). The gear branch of `killObject` (effect_executor.cpp:273-289) emits only `LeftBoardEvent`, which `TriggerManager` does not subscribe to.
  - Scenario: pay [P], exhaust, "Kill this" -> Treasure Trove goes to trash, no draw, no rune channeled. Same for any other way it is killed or bounced.

- [ ] **101 Mushroom Pouch** - `gear/0101_mushroom_pouch.cpp:23` [confirmed] high
  - Text: "At the start of your Beginning Phase, if you control a facedown card at a battlefield, draw 1."
  - Code: requires `obj.is_hidden && obj.isAtBattlefield()`. A hidden card is stored with `zone = FacedownZone`, `location = std::nullopt`, `hidden_at = bf` (game_engine.cpp:2520-2523), and `is_hidden` is cleared whenever it leaves that zone (2542, 5115), so `isAtBattlefield()` (needs a BattlefieldLocation) is never true for a hidden card.
  - Scenario: controller has a card hidden at a battlefield at the start of their Beginning Phase -> no draw, ever.

- [ ] **332 Serrated Dirk / 387 Cloth Armor** - `gear/0332_serrated_dirk.cpp:19`, `gear/0387_cloth_armor.cpp:19` [confirmed] high
  - Text: "[Assault 2] (+2 [M] while I'm an attacker.)" / "[Shield 2] (+2 [M] while I'm a defender.)"
  - Code: `equippedAssault()/equippedShield()` become an `AuraEffect{keyword, keyword_value}` with `might_bonus = 0` (game_engine.cpp:4818-4833). The aura fold (4843-4850) only sums `might_bonus` and sets the keyword bit; `recomputeMight` (game_object.h:325-331) adds the object's own `assault_value` / `shield_value`, which the aura never touches. `keyword_value` is read nowhere for Assault/Shield (only Deflect, effect_executor.cpp:370).
  - Scenario: a 2 [M] unit with Serrated Dirk attacks -> Might stays 2 instead of 4. A unit with Cloth Armor defends -> no +2. Both cards have no other effect.

- [ ] **764 Black Flame Altar** - `battlefields/0764_black_flame_altar.cpp:37` [confirmed] high
  - Text: "Units here with [Temporary] have [Shield]. (+1 [M] while they're defenders.)"
  - Code: pushes a raw `AuraEffect{keyword=Shield, keyword_value=1}` with no `might_bonus` (it does not use `auraKeyword`, which adds the conditional Might). Same fold as above: the unit gets the keyword bit and no Might.
  - Scenario: a [Temporary] unit defends at Black Flame Altar -> no +1 [M].

- [ ] **345 Long Sword / 379 Sterak's Gage / 387 Cloth Armor** - `gear/0345_long_sword.cpp:38`, `gear/0379_sterak_s_gage.cpp:38`, `gear/0387_cloth_armor.cpp:37` [confirmed] high
  - Text: "[Quick-Draw] (This has [Reaction]. When you play it, attach it to a unit you control.)"
  - Code: the attach happens only when the play intent carries a target (`resolvePermanent`, game_engine.cpp:2195), and only `generateClosedStateActions` emits such intents (3052-3068). The main-phase gear generator (2789-2802) emits a plain `PlayCard` to base with no target, and `generateShowdownActions` has no Quick-Draw branch at all.
  - Scenario: play Long Sword in your main phase -> it lands in base unattached, +2 [M] goes to nobody, and Equip [R] must be paid separately. During a showdown (open state) the gear cannot be played at all.

- [ ] **382 Svellsongur** - `gear/0382_svellsongur.cpp:19` [confirmed] high
  - Text: "As this is attached to a unit, copy that unit's text to this Equipment's effect text for as long as this is attached to it."
  - Code: only `hasEquipAbility` / `onEquip` (standardEquip [1][G]). No copy logic in the card and nothing in src/engine or src/core references it; no comment marks it as unimplemented.
  - Scenario: attach Svellsongur to any unit with abilities -> nothing is copied; the 3-energy + [G] Equipment (no Might bonus) has no effect.

- [ ] **775 Vaults of Helia** - `battlefields/0775_vaults_of_helia.cpp:21` [confirmed] high
  - Text: "When you hold here, your non-token units cost [1] more to play this turn."
  - Code: `CostModifier{energy_increase=1, affects_non_token_only, this_turn_only}` with no card-type restriction. `canAfford` / `payCardCost` (game_engine.cpp:5638-5648, 5817-5826) apply it to every card the player plays; the only unit filter available, `next_unit_only`, is not set (and would expire after one unit).
  - Scenario: hold Vaults of Helia, then play a spell or gear that turn -> it costs [1] more.

- [ ] **771 Star Spring** - `battlefields/0771_star_spring.cpp:29` [confirmed] medium
  - Text: "The first time a player plays a non-token unit here each turn, they may move another unit they control here to its base."
  - Code: the battlefield trigger carries no subject (`fireBattlefieldTriggers`, trigger_manager.cpp:163-171, 365), so the card guesses the played unit as the highest GameObjectId among the player's non-token units here. Hand cards get their ids at setup, so the just-played unit is often not the highest. `movable` is then "everything except the guess", and the move takes the first entry in map order (lines 65-70) with no player choice.
  - Scenario: unit A (id 80) is at Star Spring; P plays unit B (id 35) there and says yes -> guess = A, movable = {B} -> the unit just played is sent to base instead of A.

- [ ] **182 Scrapheap** - `gear/0182_scrapheap.cpp:23` [confirmed, discard half documented] medium
  - Text: "When this is played, discarded, or killed, draw 1."
  - Code: triggers are `WhenYouPlayThis` and `WhenIDie`. `WhenIDie` never fires for gear (see Treasure Trove). The comment says the discard branch cannot be wired, but `TriggerType::WhenIAmDiscarded` exists and is dispatched from the "discarded" state event (trigger_manager.cpp:1134-1139); the card does not register it.
  - Scenario: Scrapheap is killed, or discarded from hand -> no draw. Only the "played" third works.

- [ ] **124 Arena Bar** - `gear/0124_arena_bar.cpp:22` [confirmed] medium
  - Text: "[E]: Buff an exhausted friendly unit."
  - Code: `TargetRequirements{count=1, must_be_unit, must_be_friendly}`; the struct has no exhausted filter and the card does not override `enumerateLegalTargets` or check in `onActivate`.
  - Scenario: Arena Bar buffs a ready friendly unit (e.g. one about to attack).

- [ ] **369 Poro Snax** - `gear/0369_poro_snax.cpp:24` [documented] medium
  - Text: "[1][G], [E], Kill this: Draw 1."
  - Code: cost is `{exhaust, energy=1, recycle_self}`. The [G] power is never charged (`ActivationCost::power`/`power_domain` are available and paid by the engine), and `recycle_self` puts the card on the bottom of the Main Deck (game_engine.cpp:1463-1468) instead of killing it to trash.
  - Scenario: activate with no Calm power available -> allowed, and Poro Snax returns to the deck to be drawn again instead of going to trash.

- [ ] **271 Aspirant's Climb** - `battlefields/0271_aspirant_s_climb.cpp:18` [confirmed] medium
  - Text: "Increase the points needed to win the game by 1."
  - Code: `victoryScoreModifier()` feeds `effectiveVictoryScore()` (used by `checkWinCondition` and `scorePoints`), but the conquer path's `isWinningPointAttempt` still tests `score >= state_.mode.victory_score - 1` (game_engine.cpp:4376).
  - Scenario: with Aspirant's Climb in play (9 to win) a player on 7 conquers one battlefield -> treated as the winning point, blocked unless every battlefield was scored this turn, and they draw 1 instead of going to 8.

- [ ] **290 Vilemaw's Lair** - `battlefields/0290_vilemaw_s_lair.cpp:14` [confirmed] medium
  - Text: "Units can't move from here to base."
  - Code: `blocks_move_to_base` is read only by the standard-move generator (game_engine.cpp:2642). `EffectExecutor::moveToBase` (effect_executor.cpp:735-752) does not consult it.
  - Scenario: The Syren (184, same batch) "[1], [E]: Move a friendly unit at a battlefield to its base" targets a unit at Vilemaw's Lair -> the unit moves to base.

- [ ] **762 Altar of Blood** - `battlefields/0762_altar_of_blood.cpp:23` [confirmed] medium
  - Text: "If a unit here would die during combat, its controller may pay [A][A][A] to heal it, exhaust it, and recall it instead."
  - Code: the flag is read only in `GameEngine::killUnit` (game_engine.cpp:6314-6360), which offers the payment only when the controller has 3 READY runes in base and pays by recycling exactly those; floating power and exhausted runes are ignored. `EffectExecutor::killObject` (spell/ability kills during the combat) never checks the flag.
  - Scenario: defender has 4 runes, 3 exhausted, plus [A] floating -> no offer. A unit killed by a spell during the showdown at Altar of Blood -> no offer.

- [ ] **772 The Academy** - `battlefields/0772_the_academy.cpp:23` [confirmed] medium
  - Text: "When you hold here, give your next spell this turn [Repeat] equal to its base cost."
  - Code: the engine consumer (game_engine.cpp:1843-1850) builds the Repeat cost from `energy_cost` only (`repeat_cost.power = 0`), and reads/consumes the flag only when the spell has no printed Repeat.
  - Scenario: next spell costs [2][R] -> its granted Repeat costs [2], not [2][R]. If the next spell already has [Repeat], the grant is not consumed and passes to the spell after it.

- [ ] **Equip cost (332, 339, 345, 353, 356, 365, 374, 379, 382, 387)** - `gear/equip_base.h:30` [confirmed] medium
  - Text: "[Equip] [R]" / "[Equip] [1][G]" etc.
  - Code: the energy+Domain `standardEquip` only counts ready runes in base for Energy and requires a rune of the domain on board for Power; it never looks at `rune_pool` (floating Energy/Power). The equip generator (game_engine.cpp:2822-2854) offers the intent without any affordability check, and `onEquip` returning false changes nothing.
  - Scenario: Seal of Rage adds [R], controller has no Fury rune in base -> Equip [R] on Serrated Dirk is offered, chosen, and silently does nothing; it can be chosen again indefinitely.

- [ ] **287 The Dreaming Tree** - `battlefields/0287_the_dreaming_tree.cpp:23` [confirmed] medium
  - Text: "When a player chooses a friendly unit here with a spell for the first time each turn, they draw 1."
  - Code: comment says the trigger is inert, but `WhenAFriendlyUnitChosenHere` is now fired from the "chosen_by_controller" event (trigger_manager.cpp:1121-1122), which is emitted for activated abilities as well as spells (chain_manager.cpp:138, card.cpp:335). The card does not check that the chooser was a spell.
  - Scenario: a player targets their own unit at The Dreaming Tree with a gear ability (Orb of Regret, The Syren) -> they draw 1.

- [ ] **276 Hallowed Tomb** - `battlefields/0276_hallowed_tomb.cpp:28` [confirmed] medium
  - Text: "When you hold here, you may return your Chosen Champion from your trash to your Champion Zone if it is empty."
  - Code: no `confirmOptional` (always returns it), and it takes the first `isUnit() && isChampion()` card in the trash; nothing identifies the Chosen Champion.
  - Scenario: trash holds a main-deck copy of the champion unit and the Chosen Champion -> the wrong card can go to the Champion Zone, and the player cannot decline.

- [ ] **23 Unlicensed Armory** - `gear/0023_unlicensed_armory.cpp:51` [documented] medium
  - Text: "The next time it would die this turn, you may pay [C] to heal it, exhaust it, and recall it instead."
  - Code: sets `death_replacement_recall_pending`; the recall then happens automatically with no [C] payment and no choice.
  - Scenario: the chosen unit would die while its controller has no Power to pay -> it is saved anyway.

- [ ] **946 Kinkou Temple** - `battlefields/0946_kinkou_temple.cpp:29` [documented] medium
  - Text: "Units here with [Tank] have +1 [M]."
  - Code: checks `u.keywords.has(Keyword::Tank)` (printed / this-turn only). Tank granted through auras is not seen; Equipment keywords are added later in the same pass (step 3c).
  - Scenario: a unit wearing Doran's Shield (356, same batch) at Kinkou Temple has [Tank] but gets no +1 [M].

- [ ] **365 Brutalizer** - `gear/0365_brutalizer.cpp:22` [confirmed] low
  - Text: "If this was attached to me this turn, I have an additional +2 [M]."
  - Code: the attach turn is stamped only in the card's own `onEquip`. `attachFree` (card_helpers.h:114; used by Jax 440, Rell 347, Forge of the Fluft 522) attaches without calling it.
  - Scenario: Rell attaches Brutalizer for free -> no +2 [M] that turn.

- [ ] **181 Pack of Wonders** - `gear/0181_pack_of_wonders.cpp:29` [documented in part] low
  - Text: "[E]: Return another friendly gear, unit, or facedown card to its owner's hand."
  - Code: facedown cards are never legal (they have no location; line 51 notes this). The generator-side `enumerateLegalTargets` (29-40) does not exclude the Pack itself, while `onActivate` does.
  - Scenario: Pack of Wonders is the only friendly permanent -> the ability is offered, exhausts the Pack and does nothing. A hidden card can never be returned.

- [ ] **272 Back-Alley Bar** - `battlefields/0272_back_alley_bar.cpp:23` [confirmed] low
  - Text: "When a unit moves from here, give it +1 [M] this turn."
  - Code: fires on every `UnitMovedEvent` leaving the battlefield. Recalls emit the same event (combat-tie recall game_engine.cpp:4192-4201, death-replacement recalls, `moveToBase`), and a recall is not a move.
  - Scenario: attackers are recalled from Back-Alley Bar after a tied combat -> each gets +1 [M] this turn.

- [ ] **143 Pirate's Haven** - `gear/0143_pirate_s_haven.cpp:23` [confirmed] low
  - Text: "When you ready a friendly unit, give it +1 [M] this turn."
  - Code: `readyObject` emits `UnitReadiedEvent` for any object type (effect_executor.cpp:704-706) and `onUnitReadied` does not filter the subject to units; the card does not check either.
  - Scenario: Targon's Peak readies two runes -> two Pirate's Haven abilities go on the chain and "buff" the runes.

- [ ] **948 Piltovan Forge** - `battlefields/0948_piltovan_forge.cpp:22` [confirmed] low
  - Text: "…the first friendly gear activated ability played each turn costs [1] less."
  - Code: the discount lives in `activationEnergyReduction`, which the Equip path bypasses (`payAndQueueAbility` returns from the equip branch at game_engine.cpp:1407-1423 before costs or the `gear_abilities_activated_this_turn` counter).
  - Scenario: Equip [1][R] on Skyfall of Areion as the first gear ability of the turn -> full price, and the next gear ability still gets the discount.

- [ ] **950 Risen Altar** - `battlefields/0950_risen_altar.cpp:26` [documented] low
  - Text: "[Empower] costs of your units here cost [1] or [A] less."
  - Code: only the [1] Energy reduction is modeled.
  - Scenario: an Empower cost of [A] only (no Energy) gets no reduction.

- [ ] **286 The Candlelit Sanctum** - `battlefields/0286_the_candlelit_sanctum.cpp:22` [confirmed] low
  - Text: "Put those you don't back in any order."
  - Code: `predict(2)` asks keep/recycle per card and pushes kept cards back in peek order (effect_executor.cpp:1422-1445), so two kept cards always come back swapped with no order choice.
  - Scenario: keep both cards -> the original second card is now on top.

- [ ] **99 Garbage Grabber / 342 Assembly Rig** - `gear/0099_garbage_grabber.cpp:31`, `gear/0342_assembly_rig.cpp:36` [confirmed] low
  - Text: "Recycle 3 from your trash, [1], [E]: Draw 1." / "[1][R], Recycle a unit from your trash, [E]: …"
  - Code: the recycle cost is paid inside `onActivate` at resolution, not at activation with the rest of the cost.
  - Scenario: the opponent responds by emptying the trash -> the ability resolves with [1]/[R] and the exhaust spent and no effect; the recycled cards also stay in trash while the ability is on the chain.

## Systemic
- S2: 272 Back-Alley Bar, 60 Mask of Foresight, 143 Pirate's Haven (subject guarded only by objectExists), 186 Treasure Trove (kills ctx.source guarded only by objectExists)
- S3: 289 Trifarian War Camp, 291 Void Gate, 946 Kinkou Temple, 766 Forbidding Waste, 764 Black Flame Altar, 292 Windswept Hillock (harmless), 527 Ornn's Forge, 769 Gardens of Becoming (both grants land on the first copy's battlefield, none on the second), 950 Risen Altar (each copy's controller gets the discount at both) - all when both players bring the same battlefield, since the hook runs once per BattlefieldState and each call loops every battlefield with that def; 365 Brutalizer (two copies on board -> +4 each)
- S4: 124 Arena Bar
- S5: 282 Sigil of the Storm (rune to recycle), 276 Hallowed Tomb, 771 Star Spring (unit to move), 339 Recurve Bow (enemy unit here), 99 Garbage Grabber (which 3 trash cards), 342 Assembly Rig (which unit in trash), 284 Targon's Peak (which runes to ready)

## Not read
None
