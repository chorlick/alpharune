# Batch M1: card files whose choices are made without a finalize pass

Item M1 of `plans/remaining/10-testing-phase.md`. This is the exact list of card files under `src/cards/` that ask a
player something in a callback that does not opt into a finalize pass (`Card::choicesAtFinalization`).

**How the list was made.** The list comes from `scripts/scan_choices_without_finalize.py <checkout> <out.json>`, run on
master `9db0efb`. For each card file it finds every function body (brace-matched, with comments and strings removed).
It then finds every asking call in those bodies and follows calls between the file's own functions. A file is listed
when such a call can be reached from a callback whose `kFinalize*` bit the class does not return:

- `onResolve` needs `kFinalizeResolve`.
- `onTrigger` and `onEquippedTrigger` need `kFinalizeTrigger`.
- `onActivate` needs `kFinalizeActivate`.

The asking calls are:

- the `Card` helpers `confirmOptional`, `pickTarget`, `pickTargetPair`, `pickMode`, `pickXAmount` and `pickChainItem`;
- the `card_helpers.h` helpers `selectObject`, `selectObjectAs`, `selectTargetOrNone`, `selectSeveral`,
  `selectSeveralTargets`, `splitDamageAmong`, `pickEffectPlayLocation`, `playTokensChoosingLocations`,
  `pickTokenDest`, `pickMoveDest`, `discardOneThenAct` and `discardThenAct`;
- the executor's `requestChoice`, `chooseObjectNow`, `confirmNow`, `chooseValueNow`, `chooseAmongNow` and
  `chooseLocationNow`.

The scan does not list choices in other hooks: `onReflexiveTrigger` always has a finalize pass (CR 388.4), and
`asYouPlayChoices`, `payAdditionalPlayCost`, `applyReplacement`, `payEquipExtraCost` and `seenOnTopOfDeck` choose at the
time their rule sets.

The plan counted about 107 files. That count used only the six helpers named in the item. With the direct
`requestChoice` calls and the discard and location helpers, the count is **156 files**.

Some files have a "play-time target" (`needsPlayTimeTarget`) or an "activation-time target"
(`ActivatedAbility::needs_activation_time_target`). In those files the engine already asks for that target at
finalization (`GameEngine::chooseFirstExecution`), and the card's `pickTarget` returns the stored answer. Every such file
was still read for its other choices.

## Files (156)

| file | asking calls in a callback without the opt-in | notes |
|---|---|---|
| `battlefields/0282_sigil_of_the_storm.cpp` | onTrigger: selectObject |  |
| `battlefields/0293_zaun_warrens.cpp` | onTrigger: discardThenAct |  |
| `gear/0017_iron_ballista.cpp` | onActivate: pickTarget | activation-time target |
| `gear/0023_unlicensed_armory.cpp` | onActivate: pickTarget | activation-time target |
| `gear/0160_dazzling_aurora.cpp` | onTrigger: pickMode |  |
| `gear/0181_pack_of_wonders.cpp` | onActivate: pickTarget | activation-time target |
| `gear/0242_baited_hook.cpp` | onActivate: pickEffectPlayLocation, selectObject |  |
| `gear/0375_heart_of_dark_ice.cpp` | onActivate: pickTarget | activation-time target |
| `gear/0405_hextech_anomaly.cpp` | onActivate: pickXAmount |  |
| `gear/0412_the_zero_drive.cpp` | onActivate: requestChoice |  |
| `gear/0438_ancient_henge.cpp` | onActivate: pickXAmount |  |
| `gear/0489_vanguard_armory.cpp` | onActivate: requestChoice |  |
| `gear/0671_blood_rose.cpp` | onActivate: pickTarget | activation-time target, opts in 1 |
| `gear/0698_scryer_s_bloom.cpp` | onActivate: requestChoice |  |
| `gear/0700_the_list.cpp` | onActivate: pickTarget | activation-time target |
| `gear/0710_cursed_sarcophagus.cpp` | onActivate: pickEffectPlayLocation, pickTarget |  |
| `gear/0723_divining_shells.cpp` | onActivate: pickTarget | activation-time target |
| `gear/0736_shard_of_undoing.cpp` | onTrigger: requestChoice |  |
| `gear/0854_bottled_constellation.cpp` | onTrigger: confirmOptional, requestChoice |  |
| `gear/0920_glowstone.cpp` | onActivate: pickMode |  |
| `legends/0256_unforgiven.cpp` | onActivate: pickMode |  |
| `legends/0262_bounty_hunter.cpp` | onActivate: pickTarget | activation-time target |
| `legends/0787_voidreaver.cpp` | onActivate: pickTarget | activation-time target |
| `legends/0926_rogue_assassin.cpp` | onActivate: pickTarget | activation-time target |
| `legends/0930_master_of_shadows.cpp` | onActivate: discardOneThenAct |  |
| `legends/0934_eye_of_twilight.cpp` | onActivate: pickTarget | activation-time target |
| `legends/0938_soul_s_reflection.cpp` | onActivate: pickTarget | activation-time target |
| `legends/0940_matriarch_of_war.cpp` | onActivate: pickTarget | activation-time target |
| `legends/0942_heart_of_the_tempest.cpp` | onActivate: pickTarget | activation-time target |
| `spells/0008_get_excited.cpp` | onResolve: discardOneThenAct |  |
| `spells/0025_blind_fury.cpp` | onResolve: pickEffectPlayLocation |  |
| `spells/0033_shakedown.cpp` | onResolve: requestChoice |  |
| `spells/0058_discipline.cpp` | onResolve: pickTarget | play-time target |
| `spells/0062_reinforce.cpp` | onResolve: requestChoice |  |
| `spells/0071_party_favors.cpp` | onResolve: requestChoice |  |
| `spells/0094_sprite_call.cpp` | onResolve: pickTokenDest |  |
| `spells/0108_convergent_mutation.cpp` | onResolve: pickTarget |  |
| `spells/0115_promising_future.cpp` | onResolve: requestChoice |  |
| `spells/0153_overt_operation.cpp` | onResolve: requestChoice |  |
| `spells/0156_sabotage.cpp` | onResolve: requestChoice |  |
| `spells/0169_gust.cpp` | onResolve: pickTarget | play-time target |
| `spells/0170_morbid_return.cpp` | onResolve: pickTarget | play-time target |
| `spells/0172_rebuke.cpp` | onResolve: pickTarget | play-time target |
| `spells/0179_acceptable_losses.cpp` | onResolve: requestChoice |  |
| `spells/0180_fading_memories.cpp` | onResolve: pickTarget | play-time target |
| `spells/0183_stacked_deck.cpp` | onResolve: requestChoice |  |
| `spells/0187_whirlwind.cpp` | onResolve: requestChoice |  |
| `spells/0198_the_harrowing.cpp` | onResolve: pickEffectPlayLocation, pickTarget | play-time target |
| `spells/0209_cull_the_weak.cpp` | onResolve: requestChoice |  |
| `spells/0213_hidden_blade.cpp` | onResolve: pickTarget | play-time target |
| `spells/0237_king_s_edict.cpp` | onResolve: requestChoice |  |
| `spells/0244_divine_judgment.cpp` | onResolve: requestChoice |  |
| `spells/0247_icathian_rain.cpp` | onResolve: requestChoice | play-time target |
| `spells/0258_zenith_blade.cpp` | onResolve: confirmOptional, pickTarget, requestChoice | play-time target |
| `spells/0261_siphon_power.cpp` | onResolve: pickTarget | play-time target |
| `spells/0300_firestorm.cpp` | onResolve: pickTarget | play-time target |
| `spells/0313_recruit_the_vanguard.cpp` | onResolve: requestChoice |  |
| `spells/0354_desert_s_call.cpp` | onResolve: pickTokenDest |  |
| `spells/0363_thwonk.cpp` | onResolve: pickTarget | play-time target |
| `spells/0389_frigid_touch.cpp` | onResolve: pickTarget | play-time target |
| `spells/0419_punch_first.cpp` | onResolve: pickTarget | play-time target |
| `spells/0433_here_to_help.cpp` | onResolve: confirmOptional, pickMode, pickTarget |  |
| `spells/0443_called_shot.cpp` | onResolve: requestChoice |  |
| `spells/0475_guards.cpp` | onResolve: pickMode |  |
| `spells/0505_void_rush.cpp` | onResolve: confirmOptional, pickMode, pickTarget |  |
| `spells/0510_counter_strike.cpp` | onResolve: pickTarget | play-time target |
| `spells/0513_arise.cpp` | onResolve: playTokensChoosingLocations |  |
| `spells/0515_arcane_shift.cpp` | onResolve: pickMode, pickTarget |  |
| `spells/0516_hostile_takeover.cpp` | onResolve: pickTarget | play-time target |
| `spells/0571_upstage_comedy.cpp` | onResolve: pickTarget | play-time target |
| `spells/0575_lotus_trap.cpp` | onResolve: pickTarget | play-time target |
| `spells/0582_dancing_grenade.cpp` | onResolve: pickTarget, requestChoice | play-time target |
| `spells/0593_combat_experience.cpp` | onResolve: pickTarget | play-time target |
| `spells/0594_double_trouble.cpp` | onResolve: confirmOptional, pickTarget |  |
| `spells/0604_back_off.cpp` | onResolve: pickTarget | play-time target |
| `spells/0608_friendship.cpp` | onResolve: pickTarget | play-time target |
| `spells/0616_tricksy_tentacles.cpp` | onResolve: requestChoice |  |
| `spells/0631_sprite_burst.cpp` | onResolve: playTokensChoosingLocations |  |
| `spells/0635_deadly_flourish.cpp` | onResolve: pickTarget | play-time target |
| `spells/0657_grim_resolve.cpp` | onResolve: pickTarget | play-time target |
| `spells/0663_call_to_battle.cpp` | onResolve: pickMode, pickTarget, requestChoice | play-time target |
| `spells/0665_disposal_order.cpp` | onResolve: pickMode, requestChoice |  |
| `spells/0669_stare_down.cpp` | onResolve: pickMode, pickTarget | play-time target |
| `spells/0687_lunar_boon.cpp` | onResolve: requestChoice |  |
| `spells/0696_existential_dread.cpp` | onResolve: pickTarget | play-time target |
| `spells/0701_bone_skewer.cpp` | onResolve: confirmOptional, pickMode, pickTarget |  |
| `spells/0727_shadow_s_call.cpp` | onResolve: pickTarget | play-time target |
| `spells/0745_thrill_of_the_hunt.cpp` | onResolve: pickMode, pickTarget | play-time target |
| `spells/0747_death_from_below.cpp` | onResolve: pickTarget | play-time target |
| `spells/0760_keeper_s_verdict.cpp` | onResolve: pickTarget, requestChoice | play-time target |
| `spells/0790_brittle_steel.cpp` | onResolve: pickTarget | play-time target |
| `spells/0795_ruthless_strike.cpp` | onResolve: pickTarget | play-time target |
| `spells/0797_consuming_curse.cpp` | onResolve: pickTarget | play-time target |
| `spells/0799_perfect_execution.cpp` | onResolve: pickTarget | play-time target |
| `spells/0802_decree_of_rage.cpp` | onResolve: pickTarget | play-time target |
| `spells/0818_twilight_shroud.cpp` | onResolve: pickTarget | play-time target |
| `spells/0827_decree_of_focus.cpp` | onResolve: pickTarget | play-time target |
| `spells/0838_iterative_design.cpp` | onResolve: pickMode |  |
| `spells/0846_shock_blast.cpp` | onResolve: pickTarget | play-time target |
| `spells/0848_decree_of_insight.cpp` | onResolve: pickTarget | play-time target |
| `spells/0853_temporal_breach.cpp` | onResolve: pickTarget | play-time target |
| `spells/0859_guttural_roar.cpp` | onResolve: pickTarget | play-time target |
| `spells/0868_onslaught.cpp` | onResolve: pickTarget | play-time target |
| `spells/0872_decree_of_strength.cpp` | onResolve: pickTarget |  |
| `spells/0876_wild_claw.cpp` | onResolve: requestChoice |  |
| `spells/0877_cataclysmic_duel.cpp` | onResolve: requestChoice |  |
| `spells/0887_up_from_the_deep.cpp` | onResolve: playTokensChoosingLocations |  |
| `spells/0890_shadows_of_the_past.cpp` | onResolve: requestChoice |  |
| `spells/0892_twilight_step.cpp` | onResolve: pickMode, pickTarget | play-time target |
| `spells/0893_wind_and_ghosts.cpp` | onResolve: pickTarget | play-time target |
| `spells/0894_decree_of_discord.cpp` | onResolve: requestChoice |  |
| `spells/0903_dragon_form.cpp` | onResolve: pickTarget | play-time target |
| `spells/0913_ki_barrier.cpp` | onResolve: pickTarget | play-time target |
| `spells/0914_lacerate.cpp` | onResolve: pickTarget | play-time target |
| `spells/0918_decree_of_unity.cpp` | onResolve: pickTarget | play-time target |
| `spells/0929_dominus.cpp` | onResolve: pickTarget | play-time target |
| `spells/0931_death_mark.cpp` | onResolve: playTokensChoosingLocations |  |
| `spells/0933_siphoning_strike.cpp` | onResolve: pickTarget | play-time target |
| `spells/0935_shadow_dash.cpp` | onResolve: pickMode, pickTarget | play-time target |
| `spells/0937_acceleration_gate.cpp` | onResolve: requestChoice |  |
| `spells/0943_lightning_rush.cpp` | onResolve: requestChoice |  |
| `units/0003_chemtech_enforcer.cpp` | onTrigger: discardThenAct |  |
| `units/0020_scrapyard_champion.cpp` | onTrigger: discardThenAct |  |
| `units/0030_jinx_demolitionist.cpp` | onTrigger: discardThenAct |  |
| `units/0068_caitlyn_patrolling.cpp` | onActivate: pickTarget | activation-time target |
| `units/0109_dr_mundo_expert.cpp` | onTrigger: requestChoice |  |
| `units/0113_malzahar_fanatic.cpp` | onActivate: pickTarget | activation-time target |
| `units/0178_undercover_agent.cpp` | onTrigger: discardThenAct |  |
| `units/0185_traveling_merchant.cpp` | onTrigger: discardThenAct |  |
| `units/0192_mindsplitter.cpp` | onTrigger: requestChoice |  |
| `units/0230_albus_ferros.cpp` | onTrigger: selectSeveral |  |
| `units/0381_ornn_blacksmith.cpp` | onTrigger: confirmOptional, pickTarget |  |
| `units/0403_card_sharp.cpp` | onTrigger: confirmOptional, requestChoice |  |
| `units/0423_fae_dragon.cpp` | onTrigger: selectSeveral |  |
| `units/0431_akshan_mischievous.cpp` | onTrigger: pickTarget |  |
| `units/0440_jax_unrelenting.cpp` | onTrigger: confirmOptional |  |
| `units/0444_corrupt_enforcer.cpp` | onTrigger: discardThenAct |  |
| `units/0470_ezreal_prodigy.cpp` | onTrigger: requestChoice |  |
| `units/0544_yone_blademaster.cpp` | onTrigger: pickTarget |  |
| `units/0588_xerath_freed.cpp` | onActivate: pickTarget | activation-time target |
| `units/0612_iascylla.cpp` | onTrigger: confirmOptional, pickTarget |  |
| `units/0613_ivern_nurturer.cpp` | onTrigger: selectObject |  |
| `units/0626_fate_weaver.cpp` | onTrigger: confirmOptional, pickTarget |  |
| `units/0642_hwei_brooding_painter.cpp` | onTrigger: requestChoice |  |
| `units/0685_evershade_stalker.cpp` | onTrigger: discardThenAct |  |
| `units/0697_insightful_investigator.cpp` | onTrigger: confirmOptional, pickTarget |  |
| `units/0706_maduli_the_gatekeeper.cpp` | onActivate: pickMode |  |
| `units/0722_ultrasoft_poro.cpp` | onActivate: playTokensChoosingLocations |  |
| `units/0726_safety_inspector.cpp` | onTrigger: selectObjectAs |  |
| `units/0731_ashe_focused.cpp` | onTrigger: pickTarget |  |
| `units/0732_atakhan.cpp` | onTrigger: selectObjectAs |  |
| `units/0741_rift_herald.cpp` | onTrigger: confirmOptional, pickTarget |  |
| `units/0752_shadow.cpp` | onActivate: pickTarget | activation-time target |
| `units/0847_sky_crusier.cpp` | onActivate: pickTarget | activation-time target |
| `units/0878_corrupted_dragon.cpp` | onTrigger: requestChoice |  |
| `units/0896_illaoi_prophet_of_the_great_kraken.cpp` | onTrigger: pickTokenDest |  |

## Verdicts, per card and per choice

Every file was read in full, against its printed text (`ability_text`) and the rules. The reading was split across
five sub-agents (29, 27, 32, 33 and 35 files). Each sub-agent's verdicts were then checked against the card text by
the stream agent, and every MOVE file was read again in full before it was changed.

Verdicts:

- **FIN**: already made at finalization. It is either the engine's play-time or activation-time target, an intent
  target, or a callback that already opts in.
- **RES**: made at resolution, and that is correct. The rule is given.
- **MOVED**: it was made at resolution and belonged at finalization. It was moved in this batch, with a test.
- **Q**: the rules do not settle it. A judge question was written and the behaviour pinned.

Abbreviations:

- "M5" marks the location of an effect play. It is chosen as the played card or token is finalized, during the resolution (CR 420.3.b.1-420.3.b.2, with Rek'Sai's example). Item M5 asks whether to keep this convention.
- "hidden" is CR 355.10.a: a hand, a deck, banishment, or cards revealed while the item resolves.
- "1v1" marks "choose an opponent". It is a target (CR 355.10.d.2), but with one opponent there is nothing to ask.

**Scan false positives.** Akshan (0431), Jax (0440) and Yone (0544) inherit `kFinalizeTrigger` from
`WeaponmasterUnit` (`units/weaponmaster_base.h`). The scan reads one file at a time, so it does not see the opt-in.
All three files place every choice before their marker.

### Battlefields, gear, legends (29)

| file | card | choices and verdicts |
|---|---|---|
| battlefields/0282 | Sigil of the Storm | rune to recycle: RES ("you must recycle", CR 355.10.f, the rule's own example) |
| battlefields/0293 | Zaun Warrens | discard: RES (hidden) |
| gear/0017 | Iron Ballista | unit: FIN (activation-time target) |
| gear/0023 | Unlicensed Armory | discard cost: FIN (paid on activation); friendly unit: FIN (activation-time target); "may pay [C]" in its replacement: RES (decided as the replaced death happens, CR 370) |
| gear/0160 | Dazzling Aurora | location of the revealed unit: RES (hidden, M5) |
| gear/0181 | Pack of Wonders | object to return: FIN (activation-time target) |
| gear/0242 | Baited Hook | unit to kill: FIN (intent target); unit from the top 5: RES (hidden, a later "may", CR 355.18); location: RES (M5) |
| gear/0375 | Heart of Dark Ice | unit: FIN |
| gear/0405 | Hextech Anomaly | X: RES. This is an [Add] ability, which resolves as it is activated with no chain item and no priority (CR 401.2), so it has no separate finalization. |
| gear/0412 | The Zero Drive | location of each banished unit: RES (M5); Equip: FIN (the engine's Equip) |
| gear/0438 | Ancient Henge | X: RES (an [Add] ability, as Hextech Anomaly) |
| gear/0489 | Vanguard Armory | each token's location: RES (M5) |
| gear/0671 | Blood Rose | may and [1]: FIN (opts in); unit to ready: FIN (activation-time target) |
| gear/0698 | Scryer's Bloom | Predict: RES (hidden) |
| gear/0700 | The List | named tag: FIN (`asYouPlayChoices`, CR 355.1); unit: FIN (activation-time target) |
| gear/0710 | Cursed Sarcophagus | banished unit: RES (banishment is not a public zone, CR 355.10.a.1); location and the unit's cost: RES (M5, CR 355.18) |
| gear/0723 | Divining Shells | unit: FIN |
| gear/0736 | Shard of Undoing | the opponent's unit: RES ("each opponent must kill", CR 355.10.e-f) |
| gear/0854 | Bottled Constellation | leading "you may": **MOVED** (CR 383.3.a); the 3 units/gear killed: **MOVED** (a cost in the instruction, right after the leading may, is the ability's base cost, CR 383.3.b, 405); a death replaced while paying: **Q** (judge question 16, pinned to `killAsCost` as for Dusk Rose Lab and Overzealous Fan) |
| gear/0920 | Glowstone | "Choose a player": **MOVED** (a player is a target, CR 355.7; CR 403.2) |
| legends/0256 | Unforgiven | friendly unit: FIN (intent target); the battlefield a unit in base moves to: **MOVED** (move destination, CR 355.4, 403.2) |
| legends/0262 | Bounty Hunter | unit: FIN |
| legends/0787 | Voidreaver | unit (both abilities): FIN; a unit's destination is fixed as its base |
| legends/0926 | Rogue Assassin | unit: FIN; its destination is fixed as base |
| legends/0930 | Master of Shadows | discard: RES (hidden; it is the effect, not the cost) |
| legends/0934 | Eye of Twilight | unit: FIN |
| legends/0938 | Soul's Reflection | unit: FIN |
| legends/0940 | Matriarch of War | unit: FIN |
| legends/0942 | Heart of the Tempest | unit: FIN |

### Spells 0008-0300 (27)

| file | card | choices and verdicts |
|---|---|---|
| 0008 | Get Excited! | unit: FIN (intent target); discard: RES (hidden) |
| 0025 | Blind Fury | revealed card: RES (hidden; one opponent, so only one card); location: RES (M5) |
| 0033 | Shakedown | enemy unit: FIN (intent target); "unless its controller has you draw 2": RES (that player's choice, CR 355.10.e) |
| 0058 | Discipline | unit: FIN (play-time target) |
| 0062 | Reinforce | unit from the top 5: RES (hidden); location: RES (M5) |
| 0071 | Party Favors | Cards or Runes: RES (each other player chooses, CR 355.10.e) |
| 0094 | Sprite Call | location: RES (M5) |
| 0108 | Convergent Mutation | friendly unit: FIN (intent target); "another friendly unit": **MOVED** (a second target, CR 355.5/355.8); the spell now needs two friendly units to be played (CR 355.8) |
| 0115 | Promising Future | each player's card and location: RES (hidden, each player, M5) |
| 0153 | Overt Operation | per unit, spend its buff or not: RES ("for each friendly unit" is selection by characteristics, CR 355.10.d; a spell's may, CR 355.18) |
| 0156 | Sabotage | opponent: 1v1; card from the revealed hand: RES (hidden) |
| 0169 | Gust | unit: FIN |
| 0170 | Morbid Return | unit in your trash: **MOVED**. The trash is public, so the unit is a target (CR 355.10.a's own example). The card said `needsPlayTimeTarget` but had no target list, so nothing was asked before resolution. It is now not offered with no unit in the trash (CR 355.8). |
| 0172 | Rebuke | unit: FIN |
| 0179 | Acceptable Losses | each player's gear: RES (CR 355.10.e) |
| 0180 | Fading Memories | unit or gear: FIN |
| 0183 | Stacked Deck | card from the top 3: RES (hidden) |
| 0187 | Whirlwind | each player's unit: RES (CR 355.10.e) |
| 0198 | The Harrowing | unit in your trash: **MOVED** (as Morbid Return); location: RES (M5); paying its Power: RES (CR 355.18) |
| 0209 | Cull the Weak | each player's unit: RES (CR 355.10.e) |
| 0213 | Hidden Blade | unit: FIN |
| 0237 | King's Edict | the other player's choice: RES (CR 355.10.e) |
| 0244 | Divine Judgment | each player's keeps: RES (CR 355.10.e; the hand is hidden) |
| 0247 | Icathian Rain | first "Deal 2 to a unit": FIN (play-time target); the other five: **MOVED** (each is a target, CR 355.5; their Deflect is a cost of the play, CR 809) |
| 0258 | Zenith Blade | enemy unit: FIN; the friendly unit that may move: **MOVED** (CR 355.12: the objects of a spell's "may" are targets chosen as it is played, apart from the decision); the "may" itself: RES (a spell's may, CR 355.12, 355.18) |
| 0261 | Siphon Power | the battlefield (through a unit there): FIN. The timing is right; what is targeted is a separate defect, listed in the report. |
| 0300 | Firestorm | the battlefield (through an enemy unit there): FIN. Same note as Siphon Power. |

### Spells 0313-0745 (32)

| file | card | choices and verdicts |
|---|---|---|
| 0313 | Recruit the Vanguard | each token's location: RES (M5) |
| 0354 | Desert's Call | location (per [Repeat]): RES (M5) |
| 0363 | Thwonk! | unit (per [Repeat]): FIN (play-time target, re-chosen at finalization for each [Repeat], CR 820.2) |
| 0389 | Frigid Touch | unit (per [Repeat]): FIN |
| 0419 | Punch First | unit: FIN |
| 0433 | Here to Help | the may: RES (a spell's may); unit in hand: RES (hidden); battlefield: RES (M5; CR 355.17's example is this card's play) |
| 0443 | Called Shot | card to draw: RES (hidden) |
| 0475 | Guards! | token location: RES (M5); reflexive may and [C]: FIN (reflexive, CR 388.4) |
| 0505 | Void Rush | the may, card and location: RES (a spell's may; hidden; M5) |
| 0510 | Counter Strike | unit: FIN |
| 0513 | Arise! | token locations: RES (M5); "ready up to two": FIN (reflexive) |
| 0515 | Arcane Shift | friendly unit: FIN (intent target); enemy unit at a battlefield: **MOVED** (a second target, CR 355.5); the spell now needs one to be played (CR 355.8); where the unit is played: RES (M5) |
| 0516 | Hostile Takeover | unit: FIN |
| 0571 | Upstage Comedy | unit (per [Repeat]): FIN |
| 0575 | Lotus Trap | unit: FIN |
| 0582 | Dancing Grenade | unit: FIN; "its controller may play this again": RES (that player's choice, CR 355.10.e) |
| 0593 | Combat Experience | unit: FIN |
| 0594 | Double Trouble | the may and the unit: RES (a spell's may; hidden) |
| 0604 | Back Off | unit: FIN |
| 0608 | Friendship | unit: FIN |
| 0616 | Tricksy Tentacles | the enemy units: **MOVED** (any number, CR 355.13, as a group, CR 355.11); the location they move to: **MOVED** (move destination, CR 355.4) |
| 0631 | Sprite Burst | token locations: RES (M5) |
| 0635 | Deadly Flourish | unit: FIN |
| 0657 | Grim Resolve | unit: FIN |
| 0663 | Call to Battle | unit: FIN; the battlefield it moves to: **MOVED** (CR 355.4); the opponent's unit: RES (CR 355.10.e) |
| 0665 | Disposal Order | mode: **MOVED** (CR 355.3); up to 3 cards from the opponent's trash: **MOVED** (public zone, targets, CR 355.13) |
| 0669 | Stare Down | friendly unit: FIN; "and a battlefield": **MOVED** (a chosen object, CR 355.5/355.7) |
| 0687 | Lunar Boon | discard: RES (hidden) |
| 0696 | Existential Dread | unit (per [Repeat]): FIN |
| 0701 | Bone Skewer | "Choose a battlefield": **MOVED** (a chosen object, CR 355.5). It is a sentence of its own, not a play permission (CR 355.10.b). The "may" and the unit from the revealed hand: RES (a spell's may; hidden). |
| 0727 | Shadow's Call | unit: FIN |
| 0745 | Thrill of the Hunt | unit: FIN; battlefield: RES (CR 355.10.b, pinned by batch 3 as D3; item M4 asks who chooses) |

### Spells 0747-0943 (33)

| file | card | choices and verdicts |
|---|---|---|
| 0747 | Death from Below | unit: FIN; reflexive may and [A]: FIN (reflexive) |
| 0760 | Keeper's Verdict | unit: FIN; top or bottom: RES (the owner's choice, CR 355.10.e) |
| 0790 | Brittle Steel | gear: FIN |
| 0795 | Ruthless Strike | optional discard: FIN (CR 355.1.a); unit: FIN |
| 0797 | Consuming Curse | unit: FIN |
| 0799 | Perfect Execution | unit: FIN |
| 0802 | Decree of Rage | unit: FIN |
| 0818 | Twilight Shroud | unit: FIN |
| 0827 | Decree of Focus | unit: FIN |
| 0838 | Iterative Design | token location: RES (M5) |
| 0846 | Shock Blast | unit: FIN |
| 0848 | Decree of Insight | unit: FIN |
| 0853 | Temporal Breach | unit: FIN; location: fixed ("the same location"), no choice |
| 0859 | Guttural Roar | unit: FIN |
| 0868 | Onslaught | unit: FIN |
| 0872 | Decree of Strength | opponent: 1v1; Mind card from the revealed hand: RES (hidden) |
| 0876 | Wild Claw | card from the top 5 and its location: RES (hidden, M5); reflexive "may empower": FIN (reflexive) |
| 0877 | Cataclysmic Duel | each player's unit: RES (CR 355.10.e, the rule's own example) |
| 0887 | Up from the Deep | token locations: RES (M5) |
| 0890 | Shadows of the Past | up to 2 units from trashes: **MOVED** (public zone, targets, CR 355.13) |
| 0892 | Twilight Step | unit: FIN; where it moves: **MOVED** (CR 355.4) |
| 0893 | Wind and Ghosts | unit: FIN |
| 0894 | Decree of Discord | any number of enemy Order units, total Might 5 or less: **MOVED** (CR 355.11, 355.13) |
| 0903 | Dragon Form | unit: FIN |
| 0913 | Ki Barrier | unit: FIN |
| 0914 | Lacerate | unit: FIN |
| 0918 | Decree of Unity | unit or gear: FIN |
| 0929 | Dominus | unit: FIN; the granted ability's cost: FIN (activation) |
| 0931 | Death Mark | token location: RES (M5) |
| 0933 | Siphoning Strike | unit: FIN |
| 0935 | Shadow Dash | enemy unit: FIN; the battlefield it moves to: **MOVED** (CR 355.4) |
| 0937 | Acceleration Gate | up to 4 units, gear or runes: **MOVED** (targets, CR 355.13) |
| 0943 | Lightning Rush | the may and the card: RES (hidden) |

### Units (35)

| file | card | choices and verdicts |
|---|---|---|
| 0003 | Chemtech Enforcer | discard: RES (hidden) |
| 0020 | Scrapyard Champion | discards: RES |
| 0030 | Jinx, Demolitionist | [Accelerate]: FIN (CR 355.1.a); discards: RES |
| 0068 | Caitlyn, Patrolling | unit: FIN |
| 0109 | Dr. Mundo, Expert | 3 cards from your trash: **MOVED** (public zone, targets, CR 355.10.a). With fewer than 3 cards: **Q** (judge question 15), pinned to CR 403.4 strictly. |
| 0113 | Malzahar, Fanatic | unit or gear killed as the cost: FIN (activation; an [Add] ability) |
| 0178 | Undercover Agent | discards: RES |
| 0185 | Traveling Merchant | discard: RES |
| 0192 | Mindsplitter | opponent: 1v1; card from the revealed hand: RES (hidden) |
| 0230 | Albus Ferros | buffs to spend: RES. A buff is not a permanent, rune, chain item, player or zone, so it is not a valid target (CR 355.9.a). CR 355.10.c.1's example also does not count a spent buff as a target. So this is an effect choice at resolution (CR 355.18). |
| 0381 | Ornn, Blacksmith | the may and the gear: RES (a later may, CR 383.3.a.3; hidden) |
| 0403 | Card Sharp | "you and each opponent may": **Q** (judge question 14), pinned at resolution (CR 383.3.a.3, 355.10.e) |
| 0423 | Fae Dragon | up to four friendly units: **MOVED** (targets, CR 355.5/355.13). Units that already have a buff are now offered too (CR 427.1.c). |
| 0431 | Akshan, Mischievous | optional cost: FIN; [Weaponmaster] may and Equipment: FIN (base class); enemy gear: FIN (opts in through the base class, marker present) |
| 0440 | Jax, Unrelenting | [Weaponmaster]: FIN; "may pay [1] to draw 1": FIN (base-class opt-in, marker and `costDueNow` present) |
| 0444 | Corrupt Enforcer | discard: RES |
| 0470 | Ezreal, Prodigy | discard: RES |
| 0544 | Yone, Blademaster | [Weaponmaster]: FIN; enemy unit in a base: FIN (base-class opt-in, marker present) |
| 0588 | Xerath, Freed | unit: FIN |
| 0612 | Iascylla | the delayed ability's leading may and its enemy unit: **MOVED** (CR 383.3.a; a delayed trigger's choices are made as it is finalized, CR 355.5.b) |
| 0613 | Ivern, Nurturer | unit from the top 3: RES (hidden, a later may); the reflexive buff: FIN (reflexive) |
| 0626 | Fate Weaver | the may and the spell: RES (a later may; hidden) |
| 0642 | Hwei, Brooding Painter | discard: RES; "ready up to 2 runes": FIN (reflexive) |
| 0685 | Evershade Stalker | discard: RES |
| 0697 | Insightful Investigator | opponent: 1v1; "you may pay 2 XP to choose a card": RES (not the first part of the effect, CR 383.3.a.3/383.3.b; hidden) |
| 0706 | Maduli the Gatekeeper | the enemy battlefield it moves to: **MOVED** (CR 355.4, 403.2) |
| 0722 | Ultrasoft Poro | token locations: RES (M5) |
| 0726 | Safety Inspector | optional XP cost: FIN; each player's unit: RES (CR 355.10.e-f) |
| 0731 | Ashe, Focused | opponent: 1v1; card from the revealed hand: RES (hidden) |
| 0732 | Atakhan | optional kill cost: FIN (`payAdditionalPlayCost`); the defender's unit: RES (CR 355.10.e-f) |
| 0741 | Rift Herald | the may and the unit: RES (a later may; hidden); Deathknell unit from hand: RES (hidden) |
| 0752 | Shadow | unit: FIN |
| 0847 | Sky Crusier | discarded gear: FIN (cost); unit: FIN |
| 0878 | Corrupted Dragon | leading "you may": **MOVED** (CR 383.3.a); any number of enemy units here: **MOVED** (targets, CR 355.13) |
| 0896 | Illaoi | token location: RES (M5) |

### Totals

- 156 files.
- 24 cards have at least one choice MOVED to finalization: 3 battlefield/gear/legend cards, 16 spells and 5 units.
- 3 questions are left to the judges, numbered 14, 15 and 16.
- Every other choice is FIN or RES as listed.
