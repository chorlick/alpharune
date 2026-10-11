# 7. Cards — second pass (gap closure)

The first pass left three gaps. This file holds what closing them found.
Paths are under `src/cards/` unless noted. Nothing was built or run.

| Gap | What was done | Result |
|---|---|---|
| Cards driving `requestChoice` / `resume_point` by hand | all 104 files read in full (54 spells, 50 others) | 33 files with findings, below |
| Choices-at-finalization judged from condensed views | all 141 files read in full | late-cost list completed (7.C), new findings (7.D–7.F) |
| Target-less triggers classified by script | all 27 files read, plus 15 more found by grep | 26 BROKEN, 3 PARTIAL (7.A) |
| Loop mutation judged by script | 62 candidate loops read of 596; the rest have no mutating call in the body | clean; one AoE deviation (7.G) |
| Benchmarks under `src/openspiel/` | both files read | minor (7.H) |

Items here extend [03-cards.md](03-cards.md); where an item replaces a 03
entry the 03 entry says so.

## A. Triggers that receive no targets — verified list (replaces 03 §B / 3.2)

**Engine mechanism, confirmed:** no triggered ability of any trigger type ever
receives a non-empty `targets` vector. All four queue sites pass none
(`trigger_manager.cpp:79`, `:103`, `:250`, `:649`); the event subject goes to
`ChainItem::triggering_subject` (`:262`) or `card_counters`, never to
`targets`; `chooseTargetsAtFinalize` (`game_engine.cpp:1190-1198`) returns for
triggers; `runFinalizePass` does not copy `targets` back (`:1379-1388`). The
comment at `game_engine.cpp:1193` ("`targets` carries the triggering event's
subject") describes something nothing implements.

- [ ] **7.1 26 cards whose triggered effect never happens** [confirmed by reading every file]

| File | Line | Trigger | What never happens |
|---|---|---|---|
| `units/0016_dangerous_duo.cpp` | 19 | WhenYouPlayMe | Legion +2 Might |
| `units/0051_solari_shieldbearer.cpp` | 18 | WhenYouPlayMe | stun |
| `units/0076_yasuo_remorseful.cpp` | 18 | WhenIAttack | Might damage to an enemy here |
| `units/0082_whiteflame_protector.cpp` | 18 | WhenYouPlayMe | +8 Might |
| `units/0092_riptide_rex.cpp` | 18 | WhenYouPlayMe | deal 6 |
| `units/0097_blastcone_fae.cpp` | 18 | WhenYouPlayMe | -2 Might |
| `units/0130_crackshot_corsair.cpp` | 18 | WhenIAttack | deal 1 |
| `units/0132_first_mate.cpp` | 18 | WhenYouPlayMe | ready another unit |
| `units/0188_zaunite_bouncer.cpp` | 18 | WhenYouPlayMe | bounce |
| `units/0191_maddened_marauder.cpp` | 18 | WhenYouPlayMe | move a unit to base |
| `units/0199_tideturner.cpp` | 35 | WhenYouPlayMe | swap — opts into `kFinalizeTrigger` but returns on `targets.empty()` before `confirmOptional` |
| `units/0234_harnessed_dragon.cpp` | 18 | WhenYouPlayMe | kill an enemy unit |
| `units/0238_leona_determined.cpp` | 18 | WhenIAttack | stun |
| `units/0330_gem_jammer.cpp` | 18 | WhenYouPlayMe | grant Ganking |
| `units/0361_ribbon_dancer.cpp` | 18 | WhenIMoveToFB | +1 Might |
| `units/0384_aspiring_engineer.cpp` | 18 | WhenYouPlayMe | also wrong if fed a target: bounces an on-board gear; the card says a gear from your trash |
| `units/0453_beast_below.cpp` | 18 | WhenYouPlayMe | also: requirements set both `must_be_enemy` and `must_be_friendly` (line 23, empty legal set); only `targets[0]` handled, card names two units |
| `units/0479_sandshifter.cpp` | 18 | WhenYouPlayMe | kill a unit with 3 Might or less |
| `units/0539_ahri_inquisitive.cpp` | 19 | WhenIAttackOrDefend | -2 Might |
| `units/0589_inviolus_vox.cpp` | 18 | WhenIConquer | +8 Might |
| `units/0618_yuumi_magical_cat.cpp` | 18 | WhenIAttackOrDefend | also never grants the printed [Tank]; the def wrongly puts [Tank] on Yuumi herself |
| `units/0738_vi_peacekeeper.cpp` | 43 | WhenIAttack | stun |
| `battlefields/0278_navori_fighting_pit.cpp` | 18 | WhenYouHoldHere | buff |
| `battlefields/0535_veiled_temple.cpp` | 22 | WhenYouConquerHere | ready a gear / detach |
| `battlefields/0763_amateur_recital.cpp` | 18 | WhenYouHoldHere | move a unit to base |
| `gear/0212_forge_of_the_future.cpp` | 18 | WhenYouPlayThis | body is `killObject(targets[0])`, not the printed effect (play a Recruit token); "Kill this: Recycle up to 4" is not implemented at all |

- [ ] **7.2 Three PARTIAL cards** [confirmed]
  - `gear/0063_spirit_s_refuge.cpp:18` — the on-play buff never happens; the passive Deflect aura works.
  - `gear/0695_blast_cone.cpp:45-50` — moves `find_enemy()`, the first enemy in map order, with no choice; can be a unit already at base.
  - `legends/0744_pridestalker.cpp:23-32` — assumes the played unit arrives in `targets[0]`; the fallback buffs the first friendly unit at a battlefield in hash order, or nothing.
- [ ] **7.3 Tests never exercise this path** [confirmed]
  - No test plays any BROKEN card through the real engine. `card_test_fixture.h:451-459` (`fireTriggerAs(..., {target})`) calls `onTrigger` with hand-fed targets, a path the engine never produces. Blast Cone's tests (`tests/cards/test_audit_fixes_6.cpp:577,590,613`) rely on it.
  - Fix direction: make the fixture queue the trigger through `TriggerManager` / the chain, or drop the `targets` argument from the helper.
- Decision needed: fix per card (`pickTarget` + `kFinalizeTrigger`), or have the engine choose targets at finalization for any trigger whose `getTargetRequirements().count > 0`.
  - **DECISION (chain-triggers stream; documented in `515e90b`, enforced by the audit test in `5e163bf`): the card chooses, through `pickTarget` + `kFinalizeTrigger`. The engine does not choose targets for triggers.**
    - Why: CR 383.3 makes a triggered ability behave like an activated one once its condition is met, and CR 355 puts its choices at finalization. `GameEngine::runFinalizePass` already does exactly that for any card that opts in: it runs `onTrigger` at finalization, the helpers ask the agent, Deflect is paid and "chosen" triggers fire then (`commitChoice`), and at resolution the helpers return the stored answers (a target that is no longer in the list comes back as `kInvalidId`). 91 unit cards use it and `TriggerFinalizationTest` (tests/test_rules_compliance.cpp) covers it through the real engine. An engine-side choice from `getTargetRequirements()` cannot express "here", "another", "from your trash", "up to", or two targets, so it would be right for 7 of the 26 and a second target path for the rest. No engine change was needed; the engine now says so (comment in `chooseTargetsAtFinalize`, CLAUDE.md).
    - Rule for every card in the table (and 7.2): `onTrigger`'s `targets` argument is always empty. Never read it. Instead:
      1. Override `unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }`.
      2. In `onTrigger`, build the legal list yourself from `ctx` (it is rebuilt on every entry, so it is also the resolution-time legality check): on board only (`location.has_value()`), "here" = same `battlefieldId()` as `ctx.source` (for a battlefield card use `battlefieldOfCard(ctx.state, ctx.source)`), "another" = skip `ctx.source`, "from your trash" = iterate `player.trash`, and so on. The event's object, when the text says "it", is `ctx.state.chain.resuming->triggering_subject`.
      3. "You may" first: `int go = confirmOptional(ctx, label, still_legal);`. Then `pickTarget(ctx, label, legal)` for one target or `pickTargetPair` for two. Costs go under `if (costDueNow(ctx))`.
      4. Directly after the last choice: `if (finalizingChoices(ctx)) return;` — on every branch of a multi-trigger card, including branches with no choice. Nothing before that line may change game state.
      5. Then `if (go <= 0) return;` / `if (t == kInvalidId) return;` and the effect. `kInvalidId` covers "no legal target", "mistarget" and "waiting for the agent" alike.
      6. Delete the `getTargetRequirements()` override unless an activated ability of the same card uses it; it does nothing for a trigger and the audit test keys on it.
      7. Remove the card's id from `kKnownOffenders` in `tests/test_review_fix_chain_triggers.cpp` (`TargetlessTriggerAudit`), and test the card by playing it through the engine as `TriggerFinalizationTest` does (`testHook_executeIntent` with an agent that picks the target), not with `fireTriggerAs(..., {target})`.
    - The same applies to an Equipment's `onEquippedTrigger`: it now resolves from the chain too (02 item 2.17), so the helpers and `kFinalizeTrigger` work there.
    - Worked example, Solari Shieldbearer 51 ("When you play me, stun a unit."):
      ```cpp
      unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
      TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
      void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
          std::vector<GameObjectId> units;                       // "a unit": any unit on board
          for (auto& [id, obj] : ctx.state.objects)
              if (obj.isUnit() && obj.location.has_value()) units.push_back(id);
          const GameObjectId t = pickTarget(ctx, "Solari Shieldbearer: stun a unit", units);
          if (finalizingChoices(ctx)) return;                    // choices end here (CR 383.3)
          if (t == kInvalidId) return;                           // none / mistarget / waiting
          ctx.executor.stunUnit(t);
      }
      // getTargetRequirements() removed.
      ```
      For Yasuo, Remorseful 76 ("an enemy unit here") the loop adds `obj.controller != ctx.controller && obj.battlefieldId() == self.battlefieldId()`; for First Mate 132 ("another unit") it adds `id != ctx.source`.
  - **Corrected by the full review (08 §D):** the engine route is weaker than first stated. Only 16, 51, 82, 92, 97, 234 and 330 have target requirements that match their text. Eight more (76, 130, 132, 188, 191, 199, 238, 361) declare requirements that cannot express "here" / "another" / "from a battlefield", so engine-chosen targets would be wrong. Most of the 26 need card work either way.

## B. Repeat + choice (extends 02 item 2.4)

- [x] **7.4 Three more Repeat cards lose the repeat and leak a stale choice** [confirmed] — FIXED: in the chain manager's Repeat loop (02 item 2.4); test: ChainTriggerFixTest.Repeat_RepeatedEffectGetsItsOwnChoiceAndHappens (Frigid Touch, `pickTarget`), ChainTriggerFixTest.Repeat_HandRolledChoiceCardFinishesItsRepeat (Called Shot)
  - `spells/0436_marching_orders.cpp:50` (Repeat [3]), `spells/0450_temptation.cpp:62` (Repeat [2]), `spells/0472_bonds_of_strength.cpp:42` (Repeat [2]). All call `pickTargetPair`.
  - With Called Shot 443 and Hard Bargain 457 that is five cards; the fix is in the chain manager's Repeat loop (`chain_manager.cpp:433-447`), not the cards.

## C. Costs paid at resolution instead of finalization (replaces 03 item 3.12)

- [ ] **7.5 Complete list — 14 cards** [confirmed]

| File:line of late payment | Cost | Payability tested in `still_legal`? |
|---|---|---|
| `units/0500_fiora_worthy.cpp:41` | pay [Y] | No — free decline |
| `units/0446_fae_porter.cpp:49` | pay [P] | No — free decline |
| `units/0796_baccai_reaper.cpp:57` | pay [R] | Yes |
| `units/0107_ava_achiever.cpp:58` | pay [C] | Yes |
| `battlefields/0277_monastery_of_hirana.cpp:44` | spend a buff | Yes; buffed unit auto-picked |
| `battlefields/0765_dusk_rose_lab.cpp:62` | kill a unit you control here | Yes |
| `units/0681_kha_zix_evolving_hunter.cpp:50` | spend 3 XP | Yes; XP kept if the chosen enemy leaves |
| `units/0888_gust_monk.cpp:54-57` | banish a card from any trash | no "may"; skipped if the unit mistargets |
| `units/0869_profiteer.cpp:61` | disempower something you control | Yes |
| `units/0882_shadow_order_disciple.cpp:31` | [Burn 1] | Yes |
| `units/0889_ravenbloom_prefect.cpp:37` | banish me | Yes |
| `units/0349_rumble_hotheaded.cpp:87` | recycle another friendly unit | Yes; both units auto-picked |
| `units/0449_overzealous_fan.cpp:51-52` | kill me | not verified (03 item 3.10) |
| `units/0230_albus_ferros.cpp:28-44` | spend X buffs | X fixed early (03 item 3.11) |

- [ ] **7.6 Additional play costs folded into a `WhenYouPlayMe` trigger, paid after the marker** [confirmed; documented in-code as approximations]
  - Legion Quartermaster's playability gate (`hasLegalTargets`) is never consulted for a unit play, so it is also free with no gear on board (08 §D).
  - `units/0481_zaun_punk.cpp:63`, `units/0367_legion_quartermaster.cpp:50`, `units/0208_cruel_patron.cpp:39` (free with an empty board), `units/0728_stalking_wolf.cpp:54` (free if no Bird/Cat/Dog/Poro).
- [ ] **7.7 Costs never charged** [confirmed]
  - `units/0194_nocturne_horrifying.cpp:43` — "play me for [A]" uses `playIgnoringCost` (TODO in file).
  - `units/0331_sentinel_adept.cpp:62-68` and `units/weaponmaster_base.h:60-70` (reached from `units/0828_riven_shattered.cpp:34`) — Weaponmaster equip is always free.
  - `units/0406_jayce_man_of_progress.cpp:68`, `units/0741_rift_herald.cpp:80` — `playIgnoringCost` for an effect that ignores only Energy; Power should still be paid. Soulgorger (196:48) does it correctly with `EffectPlayCost{.ignore_energy = true}`.
- [ ] **7.8 Cost paid, effect denied** [confirmed]
  - `battlefields/0532_sunken_temple.cpp:36` — `if (!has_mighty) return;` runs at resolution before the `costAlreadyPaid` guard; pay [1], lose the Mighty unit in response, no draw.
  - `legends/0295_relentless_storm.cpp:32` — legend exhausted at finalization, no rune channeled.

## D. Card does not match its text

- [ ] **7.9** `spells/0727_shadow_s_call.cpp:39` — grants [Temporary] via `giveTemporaryKeyword`, which `expireTemporaryKeywords` (`game_engine.cpp:1006`, called `:1125`) strips the same turn, so the unit is never killed. Line 25 also offers units that already have [Temporary]. `spells/0632_turn_to_dust.cpp:20` uses the same call (not reviewed). [confirmed]
- [ ] **7.10** `units/0741_rift_herald.cpp:80` — Deathknell plays a hand unit without erasing it from `ps.hand`; `playIgnoringCost` (`effect_executor.cpp:1686`) never touches the source zone. The unit is on board and in hand. Ava Achiever 107:61 and Jayce 406:67 erase first. [confirmed]
- [ ] **7.11** `spells/0892_twilight_step.cpp:24,28-37`, `spells/0935_shadow_dash.cpp:55,63-71` — set `needsPlayTimeTarget()` so the engine picks a target at play, then hand-roll a second choice at resolution and ignore the first. [confirmed]
- [ ] **7.12** `gear/0021_sun_disc.cpp:42-50`, `units/0614_nami_headstrong.cpp:74-83` — "next unit you play" readies any exhausted friendly unit; the trigger carries no subject (`trigger_manager.cpp:352`). [confirmed]
- [ ] **7.13** `legends/0295_relentless_storm.cpp:26-32` — fires on any unit play while any Mighty friendly unit is on board. [confirmed]
- [ ] **7.14** `legends/0753_green_father.cpp:23-33,46` — replaces the first controlled non-Brush battlefield, not "that battlefield"; recomputed at resolution. [confirmed]
- [ ] **7.15** `units/0226_spectral_matron.cpp:22-45` vs `units/0486_glasc_mixologist.cpp:30-31` — same printed restriction ("no more than [3] and no more than [A]") implemented two different ways, neither matching. Glasc ignores Power cost; Matron compares Energy against available power. [confirmed they disagree]
- [ ] **7.16** `spells/0263_bullet_time.cpp:50-57` — X is capped at already-exhausted runes; `payableAnyPower` is the right count. [confirmed]
- [ ] **7.17** `spells/0702_conscription.cpp:19-36,54-57` — play-time target comes from any enemy unit; the 3-Might cap is applied only at resolution, so the spell can mistarget. `:45` — `ps.xp -= 5` sits unguarded between `confirmOptional` and `pickTarget` [latent double charge]. [confirmed]
- [ ] **7.18** `spells/0209_cull_the_weak.cpp:40-47` — the opponent does not choose; first unit in map order dies. Acceptable Losses (179) shows the two-player pattern. [confirmed]
- [ ] **7.19** `spells/0433_here_to_help.cpp:39-41,52-58` — "you may play" has no decline; battlefield is the first controlled one; falls back to base, which the text does not allow. [confirmed]
- [ ] **7.20** `spells/0604_back_off.cpp:42` — "If you played this from your hand, draw 1" has no play-source check; the card has [Hidden]. [confirmed]
- [ ] **7.21** `spells/0248_stormbringer.cpp:55-61` — the battlefield is not chosen (code comment admits it). [confirmed]
- [ ] **7.22** `spells/0511_defiant_dance.cpp:40` — second target skipped when the first mistargets (CR 359.3.e.5). [confirmed]
- [ ] **7.23** `units/0828_riven_shattered.cpp:33-34` + `weaponmaster_base.h:26-74` — "you may [Equip]" is forced and takes the first Equipment found. [confirmed]
- [ ] **7.24 No target choice; first match is taken** [confirmed]
  - `units/0674_irresistible_faefolk.cpp:28-38,48`, `units/0067_blitzcrank_impassive.cpp:41-51,60`, `units/0583_grim_apothecary.cpp:21-29,39`.
  - `units/0461_fizz_trickster.cpp:25-38`, `units/0112_kai_sa_evolutionary.cpp:26-38` — most recent eligible spell; fizzles if its Power is unaffordable even when an older one is.
  - `units/0349_rumble_hotheaded.cpp:48-67`, `spells/0258_zenith_blade.cpp:45,68-75`, `spells/0429_strike_down.cpp:72-76`, `units/0680_elder_dragon.cpp` (`enemies.front()` instead of "choose up to one").
- [ ] **7.25 Plausible mismatches**
  - `legends/0782_virtuoso.cpp:39,53` — rescans the trash at resolution instead of using `triggering_subject`; a Reaction played in response is banished instead.
  - `units/0613_ivern_nurturer.cpp:49-51` — the "may" is asked at finalization about cards only looked at during resolution; first unit auto-drafted (`:67-75`).
  - `battlefields/0774_valley_of_idols.cpp:34-41` — "it" is a newest-id heuristic; tokens excluded.
  - `units/0491_rek_sai_swarm_queen.cpp:26-46` — "you may reveal" is not asked.
  - `gear/0698_scryer_s_bloom.cpp:61-66` — "Kill this" cost is paid at resolution (`ActivationCost` has no kill-self field).
  - `spells/0457_hard_bargain.cpp:43-55,108-121` — ability to pay [2] judged by ready runes only; floating energy ignored.
  - `spells/0450_temptation.cpp:50-62` — second pick is only a destination anchor but is charged Deflect.

## E. Source or target no longer on board

`objectExists` is true for every id ever created; `moveToBattlefield`,
`moveToBase`, `bounceToHand` (`effect_executor.cpp:559,735,754`), `dealDamage`
and `killObject` guard only on it (see 02 item 2.28). A single on-board guard
in those executor functions would close most of this section.

- [ ] **7.26 Dead source moved, bounced or equipped** [confirmed]
  - `units/0447_loyal_pup.cpp:42`, `units/0617_vex_mocking.cpp:51`, `units/0177_stealthy_pursuer.cpp:36-40` — moved from trash onto a battlefield.
  - `units/0035_vayne_hunter.cpp:45`, `units/0537_vayne_hunter.cpp:44` — trash to hand, id stays in trash too.
  - `units/0331_sentinel_adept.cpp:62-66`, `units/weaponmaster_base.h:61-68`, `units/0347_rell_magnetic.cpp:51-52` — Equipment leaves the board attached to a unit in trash.
  - Lower impact: Adaptatron 56:41, Kato 434:34.
  - Already listed in 03 item 3.14: Ezreal Dashing 404, Blitzcrank 67, Azir 373.
- [ ] **7.27** `units/0149_carnivorous_snapvine.cpp:23,41,45-47` — bounced in response, it damages and "kills" itself in hand; the card ends in hand and trash with a death event. [plausible]
- [ ] **7.28** `spells/0029_falling_star.cpp:37,50-59` — both hits on one token: the second `killObject` runs on a dead token (Banishment, not Trash) and re-emits death events. [plausible]

## F. Shared-helper and bookkeeping gaps

- [ ] **7.29 `commitChoice` flags off-board picks** [plausible]
  - `card.cpp:331-333` sets `chose_enemy_unit_this_turn` for any enemy `isUnit()` object with no location check. Insightful Investigator 697:50 (opponent's hand), Kharox 901:40 (their trash), Decree of Strength 872:39, Bone Skewer 701:58.
- [ ] **7.30 Hand-rolled enemy picks bypass Deflect and chosen-object bookkeeping** [plausible]
  - `spells/0927_shuriken_flip.cpp:54,65`, `spells/0894_decree_of_discord.cpp:44`, `spells/0937_acceleration_gate.cpp:42`, plus the two in 7.11. `objChoice` / `takenObject` do not charge Deflect, push to `chosen_objects` or set `chose_enemy_unit_this_turn`.
- [ ] **7.31 Banish by direct zone write** [confirmed]
  - `spells/0115_promising_future.cpp:75-80`, `units/0491_rek_sai_swarm_queen.cpp:63-65`, `legends/0553_void_burrower.cpp:118-121` — skip `cards_banished_this_turn` and the `"banished_owned"` event.
- [ ] **7.32 `pickTargetPair` partial result treated as complete** [plausible, latent]
  - `units/0373_azir_ascendant.cpp:95-118` and `spells/0309_flash.cpp:46-50` only check for both ids invalid; at `resume_point` 12 the helper returns `{a, kInvalidId}`, so the first half is acted on, then again on re-entry. Masked by the finalize pass; reachable when it is skipped (spell played by an effect, or after `makeNewChoicesFor`, e.g. Rebuttal 939). Kinkou Monk, Zaun Punk and Profiteer check 10/12 explicitly.
- [ ] **7.33 `spells/0071_party_favors.cpp:29-32,40` — options differ only in `ability_index`** [confirmed]
  - Both encode to MakeChoice slot 0 (`action_vocab.cpp:173-190`), so under OpenSpiel the opponent can never choose Runes. Belongs with 01 item 1.12.
- [ ] **7.34 Dead opt-in bit** — `battlefields/0522_forge_of_the_fluft.cpp:18,28`: `needs_activation_time_target = true` routes away from `runFinalizePass` (`game_engine.cpp:1217-1221`), so `kFinalizeActivate` does nothing. [confirmed]
- [ ] **7.35 Low severity**
  - `units/0397_pickpocket.cpp:30` — `cardDB().get(obj.card_def_id)` with no token guard; throws for a gear token without a def (Noxian Demolitionist 867:32 guards it).
  - `gear/0152_mistfall.cpp:28-32`, `units/0343_draven_vanquisher.cpp:38-46`, `units/0037_immortal_phoenix.cpp:44-48,56-61` — affordability ignores floating power/energy or is not tested; Phoenix pays [1] by setting `is_exhausted` directly.
  - `units/0121_teemo_strategist.cpp:38` — if the chosen enemy is gone, the reveal and recycle of the top 5 is skipped too.
  - `gear/0072_solari_shrine.cpp:22` — fires on any enemy unit death (documented in-file).
  - `legends/0756_deceiver.cpp:32-33,91-92`, `gear/0895_forgotten_relic.cpp:56,66` — hand-rolled resume points sit inside `pickMode` / `pickTargetPair` ranges; safe until one of those helpers is added.
  - `units/0878_corrupted_dragon.cpp:68-78` — one resume iteration per unit moved; more than 14 eligible units hits the `kMaxResumeIterations` assert.
  - `units/0491`, `legends/0553`, `gear/0710` — card erased from its zone and the `playPayingCost` return ignored; no current failing path found.
  - `spells/0258_zenith_blade.cpp` — `stunUnitBy` runs before `confirmOptional` unguarded; fixing 3.1 will make the stun fire twice unless this is guarded first.

## G. Loop mutation and AoE

- No iterate-and-mutate defect and no insert-during-iteration over `state.objects` in `src/cards`. Cards collect into a local vector then act, or leave the loop right after the first mutation.
- [ ] **7.36** `units/0680_elder_dragon.cpp:33-56` — kills each victim inside the per-location damage loop instead of in a second pass; the first death trigger queues before the others are damaged. No crash (iterates a local copy). [confirmed, low]
- Fragile, safe as written: `spells/0429_strike_down.cpp:72` (range-for over `attachments` calling `unattachGear`, saved by the `break`); `units/0544`, `units/0629`, `spells/0209`, `spells/0258` (mutate inside a range-for over `objects`, then `break` / `return`).
- Latent: `gear/0474_eye_of_the_herald.cpp:23`, Cull 455 and World Atlas 408 call `createToken` synchronously inside event dispatch. Any future range-for over `state.objects` that moves units and keeps iterating would rehash. Engine side, `trigger_manager.cpp:449` iterates `state_.objects` while calling `fireEquippedTriggers`; today's attack/defend gear only deals damage.

## H. Benchmarks and deleted decks

- [x] **7.37** `src/openspiel/clone_microbench.cpp:99` uses `std::accumulate` without `<numeric>` (compiles only via a transitive include). [confirmed] — FIXED: `#include <numeric>` added
- [x] **7.38** `clone_advance_microbench.cpp` always applies `legal.front()` and does not report whether the checkpoint took the fast `resumeFromSnapshot` path or the full replay, so the "ApplyAction" number mixes two very different costs depending on where decision N lands. [confirmed] — FIXED: the bench finds one resumed and one replayed checkpoint and reports them separately, applying a random legal action to each clone
- [x] **7.39** Both benchmarks (and `clone_breakdown.cpp`, `clone_equivalence_test.cpp`) pass `registry=cards/ban-list.csv`, which the game ignores; header comments mention `registry.json` and a `build-release` directory. [confirmed] — FIXED: the utilities no longer pass `registry=`; header comments corrected (`build`, no registry.json, clone_breakdown's two loops)
- [x] **7.40 Deleted `decks/*_test.txt` files are still referenced** [confirmed] — FIXED: game defaults, the five `src/openspiel` utilities, test_chain, test_deck_validator and test_baron_nashor now use `decks/yi_rebuild.txt` / `decks/fiora.txt` (Baron test builds its own deck from the Fiora list); remainder is io-cli: openspiel_match.cpp, README.md; test: ChainTestFixture.FullGameRunsWithSpellsInDecks, CardTestFixture.BaronNashor_IntegrationFullPlayPathCreatesPit, RiftboundGameTest.DefaultParametersNameDecksThatExist, riftbound_clone_equiv_test
  - Left for io-cli: `src/openspiel/openspiel_match.cpp` (`--deck1` / `--deck2` defaults) and the root `README.md`.
  - 38 deck files were deleted (`581ef91`); `decks/` now holds only `yi_rebuild.txt` (tracked) and `fiora.txt` (untracked).
  - Still pointing at deleted decks: `riftbound_game.cpp:36-37` (the OpenSpiel game's default `deck1`/`deck2`, so `LoadGame("riftbound")` with no parameters throws), `riftbound_game.h:10-11`, all four `src/openspiel` utilities, `openspiel_match.cpp:284-286`, `README.md:53-87`, `tests/cards/test_baron_nashor.cpp:326-328` (`miss_fortune_test.txt`, no skip guard — the test will throw), `tests/test_deck_validator.cpp:125` (skips cleanly).
  - **Update:** the deletion is intended and is committed on `master` as `581ef91`. Every reference above is now broken and must be repointed at a deck that exists (`decks/yi_rebuild.txt` is the only tracked one), with a skip guard in `test_baron_nashor`. Expect `test_baron_nashor` to fail in the stage 0 baseline until this lands.

## Read in full and clean

**Scope note (08 §D):** "clean" here means clean for the classes this pass checked (resume state, finalization, loop mutation). The full review later found text-level defects in spells 0187, 0334, 0466, battlefield 0274, legend 0259 and units 0440, 0470, 0726, 0728; see [cards/batch-08.md](cards/batch-08.md) and [cards/batch-10.md](cards/batch-10.md).

- **Spells:** 0058, 0108, 0128, 0156, 0170, 0179, 0183, 0187, 0198, 0220, 0334, 0366, 0466, 0513, 0672, 0687, 0690, 0693, 0704, 0717, 0730, 0751, 0795, 0870, 0876, 0877, 0890, 0943.
- **Units:** 0041, 0119, 0141, 0165, 0192, 0196, 0208, 0225, 0308, 0440, 0470, 0626, 0642, 0726, 0728, 0729, 0919.
- **Gear / battlefields / legends:** 0228, 0854, 0274, 0259.
- No hand-rolled `resume_point` or `resume_data` slot collides with a helper in the same card.
- Choices-at-finalization: every opted-in callback has the marker on every branch; no new state writes before the marker beyond the four in 03 item 3.13.
