[Back to the full card review](../08-cards-full-review.md)

# Batch 05 - units/0219_vanguard_sergeant.cpp .. units/0494_trove_golem.cpp

Files read in full: 91 of 91.

## Findings
- [ ] **467 Vex, Cheerless** - `units/0467_vex_cheerless.cpp:18` [confirmed] high
  - Text: "While I'm in combat, friendly spells cost [1][A] less to a minimum of [1], and enemy spells cost [1][A] more."
  - Code: `applyPassiveAura` pushes two `CostModifier`s with `this_turn_only = false` onto `cost_modifiers` on EVERY aura pass. `recalculateAuras` (game_engine.cpp:4389-4441) never clears `cost_modifiers`; the only erasers are the end-of-turn `this_turn_only` sweep (game_state.h:295) and the one-shot `next_*_only` sweep (game_engine.cpp:5877). The modifiers are not spell-restricted (no spell-only field is set, so game_engine.cpp:5638-5662 applies them to units and gear too), `combat_active_only` means "any combat on any battlefield" (5630-5633), not "Vex is in combat", and the [A] (power) half is not modelled (`power_reduction` unset).
  - Scenario: Vex sits on board for a turn; cleanup runs dozens of times, leaving dozens of stacked -1/+1 modifiers. In the next combat anywhere (Vex at base, or already dead) every card Vex's controller plays costs [1] total and every card the opponent plays costs +N energy (unplayable). The modifiers persist for the rest of the game.

- [ ] **311 Garen, Commander** - `units/0311_garen_commander.cpp:20` [confirmed] high
  - Text: "Other friendly units have +1 [M] here."
  - Code: the card's `applyPassiveAura` gives +1 to other friendly units at Garen's battlefield, AND the engine text matcher (game_engine.cpp:4601-4603, pattern "other friendly units have", `all_friendly = true`, no here filter because "here" comes after "have") gives +1 to every other friendly unit anywhere.
  - Scenario: Garen at battlefield A with an ally; another ally at base. Ally at A has +2 (should be +1), ally at base has +1 (should be 0). Garen at base still gives +1 to all other friendly units.

- [ ] **243 Darius, Executioner** - `units/0243_darius_executioner.cpp:43` [confirmed] high
  - Text: "Other friendly units have +1 [M] here."
  - Code: no card-side aura; the engine text matcher (game_engine.cpp:4601-4603) matches "other friendly units have" with `all_friendly = true` and no location filter.
  - Scenario: Darius at battlefield A (or at base); friendly units at battlefield B and in base all get +1 [M]. Only units at Darius's location should.

- [ ] **380 Irelia, Fervent** - `units/0380_irelia_fervent.cpp:19` [confirmed] high
  - Text: "When you choose or ready me, give me +1 [M] this turn."
  - Code: registers `WhenYouChooseAFriendlyUnit` with no subject filter. TriggerManager (trigger_manager.cpp:1105-1119) fires that trigger on the controller's OTHER cards (`if (id == e.object ...) continue;`) with the chosen unit as subject, never on the chosen unit itself. Also the Awaken-phase ready emits only "became_ready" -> `WhenIBecomeReady` (trigger_manager.cpp:1126), not `WhenIAmReadied` (CR 416.3.a says the player readies objects in Awaken).
  - Scenario: you target Irelia with a spell -> no +1. You target any other friendly unit -> Irelia gets +1. Irelia readied in your Awaken phase -> no +1.

- [ ] **463 Jae Medarda** - `units/0463_jae_medarda.cpp:21` [confirmed] high
  - Text: "When you choose me with a spell, draw 1."
  - Code: same trigger; it never fires on the chosen unit (trigger_manager.cpp:1110). The card's comment assumes it fires on all friendly units. When it does fire (another unit chosen) the chain scan for a spell targeting Jae fails, so nothing happens. Deferred-target spells would also be missed (the resolving spell is in `chain.resuming`, not `chain.items`).
  - Scenario: play a spell choosing Jae Medarda -> no card drawn, ever. Fix needs a self-chosen dispatch (or drop the `id == e.object` exclusion) and `triggerCondition(subject == source)`.

- [ ] **422 Yordle Explorer** - `units/0422_yordle_explorer.cpp:27` [confirmed] high
  - Text: "When you play a card with Power cost [A][A] or more, draw 1."
  - Code: at resolution scans `chain.items` for the newest spell/permanent item to find the played card. For units/gear the trigger is added at `CardPlayedEvent` (game_engine.cpp:1696) BEFORE `addPermanent` (1721), so the permanent resolves first and is gone; for spells the trigger fires from `onSpellResolved` after the spell left the chain. `WhenYouPlayAUnit/AGear` are fired without a subject (trigger_manager.cpp:352/356).
  - Scenario: play any [A][A] unit, gear or spell with an empty chain -> `played == kInvalidId` -> no draw. With another spell lower on the chain, that spell's power cost is tested instead.

- [ ] **388 Forecaster** - `units/0388_forecaster.cpp:20` [confirmed] high
  - Text: "Your Mechs have [Vision]."
  - Code: grants an aura Vision keyword to on-board Mechs. Vision is resolved in `TriggerManager::onCardPlayed` with `obj.keywords.has(Keyword::Vision)` (trigger_manager.cpp:317) - base keywords only, and at that moment the played unit has no location and no aura yet.
  - Scenario: Forecaster on board, play any other Mech -> no Vision look. Only Forecaster's own printed Vision bit ever works.

- [ ] **316 Tibbers** - `units/0316_tibbers.cpp:22` [confirmed] high
  - Text: "When you play me, deal 3 to all units at battlefields."
  - Code: damages every unit with `location.has_value()`, which includes units in both bases.
  - Scenario: Tibbers played to base with two friendly 3-Might units in base -> both die (and Tibbers takes 3). Units in bases must be untouched; filter on `isAtBattlefield()`.

- [ ] **246 Viktor, Leader** - `units/0246_viktor_leader.cpp:22` [documented] high
  - Text: "When another non-Recruit unit you control dies, play a 1 [M] Recruit unit token into your base."
  - Code: fires on every friendly death; the comment says the dead unit is not available, but `triggerCondition(..., subject)` receives it (trigger_manager.cpp:671) and the dead object keeps its tags.
  - Scenario: a Recruit token dies -> Viktor makes a new Recruit. Every Recruit replaces itself indefinitely.

- [ ] **458 Harpoon Squad** - `units/0458_harpoon_squad.cpp:21` [documented] high
  - Text: "When I move from a battlefield, give me +2 [M] this turn."
  - Code: +2 on any move. The origin is available: `card_counters["__moved_from_bf"]` is set to -1 for a move out of base (trigger_manager.cpp:835).
  - Scenario: Harpoon Squad moves base -> battlefield to attack and fights as a 6-Might unit; it should be 4.

- [ ] **241 Shen, Kinkou / 376 Janna, Savior / 348 Rengar, Pouncing** - `units/0241_shen_kinkou.cpp:37`, `units/0376_janna_savior.cpp:71`, `units/0348_rengar_pouncing.cpp:40` [confirmed] high
  - Text: "[Reaction] (Play any time, even before spells and abilities resolve, including to a battlefield you control.)"
  - Code: `Keyword::Reaction` is only consulted for spells (game_engine.cpp:3260-3265, chain_manager.cpp:510-511). `generateShowdownActions` / `generateClosedStateActions` offer units only via Ambush or `playableAsReactionToAttack`. No per-card hook compensates.
  - Scenario: opponent attacks your battlefield; Shen (Shield 2, Tank) or Janna in hand cannot be played in the showdown. Rengar can only be played to a battlefield he is attacking, never as a Reaction to base / a controlled battlefield.

- [ ] **441 Sivir, Ambitious** - `units/0441_sivir_ambitious.cpp:21` [documented] high
  - Text: "When I conquer after an attack, if you assigned 5 or more excess damage to enemy units, you may deal that much to an enemy unit."
  - Code: `onTrigger` only logs. The comment says excess damage is not tracked, but `PlayerState::excess_combat_damage` / `excessCombatDamageAt()` (card_helpers.h:87) now exist.
  - Scenario: Sivir conquers with 6 excess damage -> nothing happens.

- [ ] **232 Fiora, Victorious** - `units/0232_fiora_victorious.cpp:49` [confirmed] medium
  - Text: "While I'm [Mighty], I have [Deflect], [Ganking], and [Shield]."
  - Code: the def sets `Keyword::Deflect/Ganking/Shield`, `shield_value = 1`, `deflect_value = 1` as BASE data, so she has all three at 4 Might. When Mighty the aura adds a second Deflect, and `deflectCostFor` sums base + aura (effect_executor.cpp:368-370) -> Deflect 2. The aura Shield carries no value/might.
  - Scenario: unbuffed 4-Might Fiora moves battlefield to battlefield, costs [A] to target and defends at 5 (which then makes her "Mighty"). CR example at core-rules.md:2532 says she has no keywords at 4 Might.

- [ ] **464 Sivir, Mercenary** - `units/0464_sivir_mercenary.cpp:53` [confirmed] medium
  - Text: "If you've spent at least [A][A] this turn, I have +2 [M] and [Ganking]."
  - Code: `d.keywords.set(Keyword::Ganking)` in the def makes Ganking unconditional.
  - Scenario: no power spent this turn; Sivir can still move battlefield to battlefield.

- [ ] **452 Ancient Warmonger** - `units/0452_ancient_warmonger.cpp:62` [confirmed] medium
  - Text: "I have [Assault] equal to the number of enemy units here."
  - Code: def has `assault_value = 1` + `Keyword::Assault`; `recomputeMight` adds `assault_value` while attacking (game_object.h:326) on top of the aura's `might_bonus = enemies`.
  - Scenario: attacks into 2 enemy units -> +3 instead of +2. Set `assault_value = 0`.

- [ ] **451 Treasure Hunter / 476 Honest Broker** - `units/0451_treasure_hunter.cpp:20`, `units/0476_honest_broker.cpp:20` [confirmed] medium
  - Text: "play a Gold gear token exhausted."
  - Code: both call `createGoldToken`, and `createToken` only exhausts units (`is_exhausted = !enter_ready && type == Unit`, effect_executor.cpp:1200), so the Gold enters READY. The other Gold cards in the batch use `createGoldExhausted`.
  - Scenario: Treasure Hunter moves -> Gold can be killed for [A] in the same turn; it should be unusable until it readies.

- [ ] **413 Buhru Captain** - `units/0413_buhru_captain.cpp:17` [confirmed] medium
  - Text: "When you play me, you may draw 1 or buff me."
  - Code: always draws 1; no choice, no buff option, no decline.
  - Scenario: player wants the +1 [M] buff (or to decline with an empty deck to avoid burn out) -> forced draw.

- [ ] **431 Akshan, Mischievous** - `units/0431_akshan_mischievous.cpp:93` [confirmed] medium
  - Text: "[Weaponmaster]"
  - Code: the class derives from `UnitCard`, sets the keyword bit, and `onTrigger` returns immediately unless the [O][O] additional cost was paid. Nothing in the engine reads `Keyword::Weaponmaster`, so the Weaponmaster equip never happens. (The gear steal also takes the first enemy gear - S5.)
  - Scenario: Akshan played with a friendly Equipment in base -> no equip offered.

- [ ] **324 Armed Assailant / 414 Combat Chef / 421 Veteran Poro / 448 Master Bingwen / 407 Ornn, Forge God / 435 Lucian, Merciless** - `units/weaponmaster_base.h:26`, `units/0407_ornn_forge_god.cpp:19`, `units/0435_lucian_merciless.cpp:32` [documented] medium
  - Text: "[Weaponmaster] (When you play me, you may [Equip] one of your Equipment to me for [A] less, even if it's already attached.)"
  - Code: mandatory, first Equipment in map order, attached for free whatever its Equip cost (CR 821.1.c: pay the Equip cost reduced by [A]; 821.1.c.4/5: can't if no cost / unpayable). No on-board check on the Weaponmaster unit.
  - Scenario: only Equipment is on your best unit and has Equip [2][A] -> it is ripped off that unit and attached to a 2-Might Veteran Poro at no cost, with no option to decline. If the unit was killed in response, the gear is attached to an off-board object and its location becomes nullopt (gear disappears).

- [ ] **377 Jax, Unmatched** - `units/0377_jax_unmatched.cpp:21` [plausible] medium
  - Text: "Your Equipment everywhere have [Quick-Draw]. (Each gains [Reaction]. When you play it, attach it to a unit you control.)"
  - Code: aura grants `Keyword::QuickDraw` only. The engine offers Quick-Draw gear only in `generateClosedStateActions` (game_engine.cpp:3052-3069); there is no Quick-Draw section in `generateShowdownActions`, and a main-phase gear play carries no target so the attach at 2195 (`!item.targets.empty()`) is skipped. Engine-level, but it is the whole card.
  - Scenario: Jax on board, Equipment played in the main phase or with focus in a showdown -> not attached / not offered.

- [ ] **240 Sett, Kingpin** - `units/0240_sett_kingpin.cpp:25` [confirmed] medium
  - Text: "I get +1 [M] for each buffed friendly unit at my battlefield."
  - Code: `if (oid == sid) continue;` excludes Sett; the text has no "other".
  - Scenario: buffed Sett alone at a battlefield -> 6 Might (5 + buff); should be 7.

- [ ] **337 Minotaur Reckoner** - `units/0337_minotaur_reckoner.cpp:25` [documented] medium
  - Text: "Units can't move to base."
  - Code: stamps `cant_move_to_base = true` on every on-board unit; `recalculateAuras` never resets the flag (not in the reset block at game_engine.cpp:4391-4407). Only the standard-move generator reads it (2644); `EffectExecutor::moveToBase` does not.
  - Scenario: Reckoner dies -> every unit that was on board can never move back to base for the rest of the game. While it is alive, effect moves to base (Janna, Savior) still work.

- [ ] **378 Needlessly Large Yordle** - `units/0378_needlessly_large_yordle.cpp:30` [documented] medium
  - Text: "I cost [2][G] less for each point you scored from holding this turn."
  - Code: only the energy half; the comment says no power hook exists, but `Card::selfPowerCostReduction` is wired (game_engine.cpp:5598).
  - Scenario: 2 hold points -> should cost [6][G]; costs [6][G][G][G].

- [ ] **231 Commander Ledros** - `units/0231_commander_ledros.cpp:17` [documented] medium
  - Text: "As you play me, you may kill any number of friendly units as an additional cost. Reduce my cost by [Y] for each killed this way."
  - Code: not implemented (no hook in card or engine).
  - Scenario: cannot sacrifice units to reduce the [Y][Y][Y][Y] cost.

- [ ] **423 Fae Dragon** - `units/0423_fae_dragon.cpp:22` [documented] medium
  - Text: "When you spend a buff, play a Gold gear token exhausted."
  - Code: not implemented (no spend-a-buff event exists). First clause buffs the first four unbuffed units in map order (S5).
  - Scenario: a buff is spent -> no Gold.

- [ ] **403 Card Sharp** - `units/0403_card_sharp.cpp:19` [documented] medium
  - Text: "you and each opponent may play a Gold gear token exhausted. For each opponent who did, you play a Gold gear token exhausted."
  - Code: both players always take it; the opponent is never asked.
  - Scenario: opponent would decline (1 Gold for them hands you 2) -> forced 2-for-1.

- [ ] **394 Breakneck Mech** - `units/0394_breakneck_mech.cpp:45` and `:68` [confirmed] medium
  - Text: "Your Mechs have [Deflect] and [Ganking]. / I enter ready if you control another Mech."
  - Code: (a) `entersReadyOnPlay` skips every object with the same card def, so another Breakneck Mech is not "another Mech". (b) The def has `deflect_value = 1` + base Deflect AND the aura adds Deflect to all Mechs including itself; `deflectCostFor` sums them (CR 809.2 sums separate grants, but this is one ability).
  - Scenario: (a) second Breakneck Mech played with only the first on board enters exhausted. (b) opponent must pay [A][A] to choose a lone Breakneck Mech.

- [ ] **416 Direwing** - `units/0416_direwing.cpp:22` [confirmed] low
  - Text: "I enter ready if you control another Dragon."
  - Code: skips every object with Direwing's def id instead of this instance (the 3-arg `entersReadyOnPlay(state, controller, self)` overload gives the instance).
  - Scenario: second Direwing with the first on board enters exhausted.

- [ ] **492 Renata Glasc, Industrialist** - `units/0492_renata_glasc_industrialist.cpp:24` [plausible] low
  - Text: "Your tokens enter ready."
  - Code: `createToken` honours the flag, but `createGoldExhausted` (card_helpers.h:277) sets `is_exhausted = true` afterwards.
  - Scenario: with Renata on board, every "Gold gear token exhausted" still enters exhausted; whether her replacement should win needs a ruling check.

- [ ] **236 Karthus, Eternal** - `units/0236_karthus_eternal.cpp:22` and `:43` [confirmed] low
  - Text: "Your [Deathknell] effects trigger an additional time."
  - Code: `deathknell_double_count` repeats every `WhenIDie` trigger (trigger_manager.cpp:640-653) with no Deathknell-keyword check. The def also sets `Keyword::Deathknell` on Karthus, who has no Deathknell ability.
  - Scenario: Draven, Audacious ("When I die in combat ..." - not Deathknell) dies with Karthus on board -> opponent scores 2.

- [ ] **376 Janna, Savior** - `units/0376_janna_savior.cpp:25` [confirmed] low
  - Text: "heal your units here, then move up to one enemy unit from here to its base."
  - Code: returns if not at a battlefield, so nothing is healed when she is played to base ("here" can be a base, CR 464.2.b). With exactly one enemy unit `pickTarget` forces it, so "up to one" cannot be declined.
  - Scenario: Janna played to base as a response -> damaged units in base are not healed.

- [ ] **480 Trusty Ramhound** - `units/0480_trusty_ramhound.cpp:24` [confirmed] low
  - Text: "While you have another unit here, I have +1 [M]."
  - Code: requires a battlefield; no bonus in base.
  - Scenario: Ramhound in base next to another unit takes 2 damage and dies; it should have 3 Might.

- [ ] **410 Renata Glasc, Mastermind** - `units/0410_renata_glasc_mastermind.cpp:22` [confirmed] low
  - Text: "Use my abilities only while I'm at a battlefield."
  - Code: `canActivateAbility` is true if ANY Renata the player controls is at a battlefield, not the activating one (`canActivateAbilityAt` has the source).
  - Scenario: one Renata at a battlefield, one in base -> the base copy can use both abilities.

- [ ] **304 Lux, Illuminated** - `units/0304_lux_illuminated.cpp:25` [confirmed] low
  - Text: "When you play a spell that costs [5] or more, give me +3 [M] this turn."
  - Code: reads the newest spell in the controller's trash rather than the trigger's subject (`triggering_subject`).
  - Scenario: a 5+ spell that is banished (Flow) or recycled on resolution is not in the trash -> an older spell's cost is tested (missed buff, or a buff for a cheap spell).

- [ ] **312 Lux, Crownguard** - `units/0312_lux_crownguard.cpp:29` [documented] low
  - Text: "[Add] [2]. Use only to play spells."
  - Code: plain floating energy, spendable on anything. Also `Keyword::Reaction` is set on the unit (as on 377 Jax) although only the ability is a Reaction - harmless today only because unit Reaction is not implemented.
  - Scenario: exhaust Lux to pay for a unit.

- [ ] **381 Ornn, Blacksmith** - `units/0381_ornn_blacksmith.cpp:31` [documented] low
  - Text: "You may reveal a gear from among them and draw it."
  - Code: always takes the first gear; no choice among several, no decline; the reveal is private (`revealed_to_all = false`).
  - Scenario: two gear in the top 4 -> the player cannot pick the second.

- [ ] **462 Irelia, Graceful** - `units/0462_irelia_graceful.cpp:27` [documented] low
  - Text: "Your spells that choose me cost [1] or [A] less."
  - Code: always [1] energy; no [A] option; deferred-target spells get nothing.
  - Scenario: a [0]-energy [A] spell choosing Irelia gets no discount.

- [ ] **341 Void Hatchling** - `units/0341_void_hatchling.cpp:23` [documented] low
  - Text: "If you would reveal cards from a deck, look at the top card first."
  - Code: only `revealUntil` / `revealAndChoose` honour the flag; cards that reveal by hand bypass it (in this batch: 364 Apprentice Smith).
  - Scenario: Apprentice Smith moves with Hatchling on board -> no peek.

- [ ] **432 Fiora, Peerless** - `units/0432_fiora_peerless.cpp:33` [confirmed] low
  - Text: "double my Might this combat."
  - Code: `giveTemporaryMight` lasts until end of turn.
  - Scenario: after the combat Fiora still has doubled Might against a damage spell that turn.

## Systemic
- S2: 324 Armed Assailant, 414 Combat Chef, 421 Veteran Poro, 448 Master Bingwen, 407 Ornn Forge God, 435 Lucian Merciless, 431 Akshan Mischievous (gear attached to an off-board unit); 223 Peak Guardian, 243 Darius Executioner, 304 Lux Illuminated, 370 Simian Ancestor, 380 Irelia Fervent, 395 Dropboarder, 398 Prize of Progress, 458 Harpoon Squad (buff/ready on self without an on-board check)
- S3: 232 Fiora Victorious, 240 Sett Kingpin, 302 Yi Meditative, 311 Garen Commander, 391 Gearhead, 407 Ornn Forge God, 452 Ancient Warmonger, 464 Sivir Mercenary, 480 Trusty Ramhound
- S5: 324 Armed Assailant, 414 Combat Chef, 421 Veteran Poro, 448 Master Bingwen, 407 Ornn Forge God, 435 Lucian Merciless, 431 Akshan Mischievous, 385 Bubble Bot, 381 Ornn Blacksmith, 423 Fae Dragon

## Not read
None
