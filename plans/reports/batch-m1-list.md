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

## Verdicts

Verdicts follow, per card and per choice, once each file has been read in full.
