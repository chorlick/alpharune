[Back to the full card review](../08-cards-full-review.md)

# Batch 09 - spells/0511_defiant_dance.cpp .. units/0367_legion_quartermaster.cpp

Files read in full: 81 of 81.

Only findings NOT already in 03-cards.md / 07-cards-second-pass.md are listed. Engine facts used below were traced: `playIgnoringCost` never removes the card from its source zone (`engine/effect_executor.cpp:1686-1759`); `ChainManager` always files a resolved spell in trash unless `banish_on_leave_chain` (`engine/chain_manager.cpp:453-487`); an aura-granted keyword adds Might only through `AuraEffect::might_bonus` (`engine/game_engine.cpp:4838-4850`, `core/game_object.h:317-340`); `card_counters` is never cleared on a zone change; `hasLegalTargets` is consulted only for spells and abilities (`engine/game_engine.cpp:3295,3452,3555`); `Card::pickTarget` / `pickTargetPair` do not filter `untargetable_by_enemy` (`card.cpp:339-627`).

## Findings
- [x] **515 Arcane Shift** - `spells/0515_arcane_shift.cpp:56` [confirmed] high — FIXED: sets `banish_on_leave_chain` instead of calling `banishObject` on itself; test: C3Test.ArcaneShift_ReplaysTheUnitOnceAndBanishesItself
  - Text: "Banish this."
  - Code: `banishObject(ctx.source)` while the spell is still the resolving chain item. That pushes the id into `banishment` (and bumps `cards_banished_this_turn`, emits `banished_owned`), then `ChainManager::stepResolve` (`chain_manager.cpp:468-476`) sees `banish_on_leave_chain == false`, sets `zone = Trash` and pushes the same id into `trash`.
  - Scenario: Arcane Shift resolves -> it sits in the trash (zone Trash) and its id is also in the banishment vector. Annie, Stubborn / Kai'Sa / any trash recursion can take it back and replay it; the card is never actually banished. Fix: set `banish_on_leave_chain = true` instead of calling `banishObject`.
- [x] **347 Rell, Magnetic** - `units/0347_rell_magnetic.cpp:49` [confirmed] high — FIXED: the executor takes a played card out of its zone (`99d179e`, 08 item 8.13); c5: the Equipment is attached only while Rell is on the board, token-safe cost read; test: C5Test.RellMagneticPlaysTheEquipmentOutOfHand
  - Text: "you may play an Equipment with Energy cost no more than [2], ignoring its cost ... Attach it to me."
  - Code: `playIgnoringCost(ctx.controller, eq, ...)` on a card taken from `hand` without erasing it from `ps.hand` (Ava Achiever 107:60-61 erases first; same defect class as 7.10 Rift Herald).
  - Scenario: Rell attacks, plays a 2-cost Equipment from hand -> the Equipment is on board attached to Rell and its id is still in the hand vector: hand size is one too high, it can be discarded/played again as a second copy of the same object.
- [x] **349 Rumble, Hotheaded** - `units/0349_rumble_hotheaded.cpp:35-38` [confirmed] high — ALREADY FIXED: stage 1 (`73e121e`, `1ae4c88`) — `auraKeyword(.., Assault, 1)` per Rumble through `forEachSelfOnce`, engine fold adds the Might, text matcher deleted, no printed Assault; test: AurasCostsTest.AuraAssault_RumbleFarronWarmongerRagingSoul
  - Text: "Your Mechs each have [Assault]. (+1 [M] while we're attackers.)"
  - Code: pushes a raw `AuraEffect{keyword = Assault}` with `keyword_value = 0` and `might_bonus = 0`. The aura fold only sums `might_bonus` and sets the keyword bit; `recomputeMight` adds `assault_value` only (base/temp). `auraKeyword()` in card_helpers.h exists exactly for this and is not used. The engine's legacy text match for "your mechs each have" (`game_engine.cpp:4619-4623`) adds a second, equally Might-less effect.
  - Scenario: Rumble plus another Mech attack -> the other Mech gets the Assault keyword bit but no +1 Might; only Rumble (printed `assault_value = 1` in the def) gets +1. Also: the hook is not `forEachSelfOnce`, and no `location` check on `u`, so once Might is added two Rumbles + the engine text aura will stack three times, and Rumble himself (def `assault_value = 1` + aura) will have Assault 2.
- [x] **107 Ava Achiever** - `units/0107_ava_achiever.cpp:88` [confirmed] high — FIXED: `Keyword::Hidden` removed from the def; test: C4Test.Defs_DoNotCarryKeywordsTheTextOnlyMentions
  - Text: "When I attack, you may pay [C] to play a card with [Hidden] from your hand, ignoring its cost. If it's a unit, play it here." (no printed [Hidden]; `cards/json/ogn_107_ava_achiever.json` has no keywords)
  - Code: `d.keywords.set(Keyword::Hidden)` - the keyword is only referenced by the text.
  - Scenario: the hide generator (`game_engine.cpp:2858-2860`) offers to hide Ava for [A] and play her later as a Reaction for [0] instead of paying 5 Energy. She also counts as a Hidden card for Teemo, Strategist's reveal and for another Ava's "play a card with [Hidden]".
- [x] **330 Gem Jammer** - `units/0330_gem_jammer.cpp:41` [confirmed] high — FIXED: `Keyword::Ganking` removed from the def; the trigger chooses a unit (`pickTarget` + `kFinalizeTrigger`) and grants it; test: C5Test.GemJammerGivesTheChosenUnitGanking, C5Test.KeywordsTheTextOnlyMentionsAreNotOnTheDef
  - Text: "When you play me, give a unit [Ganking] this turn." (no printed [Ganking] on Gem Jammer itself)
  - Code: `d.keywords.set(Keyword::Ganking)`.
  - Scenario: Gem Jammer permanently has Ganking and may move battlefield to battlefield every turn (`game_engine.cpp:2657`), while the effect it is supposed to grant never happens (7.1). Same extraction error as Ava: worth a repo-wide grep for keywords that the text only mentions (in this batch also harmless ones: Shadow's Call 727:58 `Temporary`, Abandon 693:110 `Predict`, Sentinel Adept 331:91 `Equip`).
- [x] **336 Blast Corps Cadet** - `units/0336_blast_corps_cadet.cpp:31` [confirmed] medium — FIXED: the paid flag is erased once the trigger resolves; same fix in the range's other three (390 Frostcoat Cub, 420 Sea Monkey, 431 Akshan); 044, 590, 684 are other streams'; test: C5Test.BlastCorpsCadetPaymentCoversOnePlay
  - Text: "When you play me, if you paid the additional cost, deal 2 to a unit at a battlefield."
  - Code: reads `card_counters["__blastcorps_paid"] != 1` and never erases the flag; `maybePayOptionalAdditionalCost` (`game_engine.cpp:5550-5558`) only ever writes 1, and nothing clears `card_counters` when the object leaves play. Ruthless Strike 795:45 and Rampage 870:61 erase theirs.
  - Scenario: play Cadet paying [1][R]; later it is bounced (Star-Crossed, Zaunite Bouncer) or returned from trash (Cemetery Attendant) and replayed WITHOUT paying -> it deals 2 again for free. The same never-cleared `!= 1` pattern is in 044, 390, 420, 431, 590, 684 (outside this batch; not read).
- [x] **367 Legion Quartermaster** - `units/0367_legion_quartermaster.cpp:34-36` [confirmed] medium — FIXED: the gate is `canBePlayed` (what the engine asks for a unit play); the gear is chosen and returned under `costDueNow` when the play trigger is finalized; test: C5Test.LegionQuartermasterNeedsAGearAndReturnsIt
  - Text: "As an additional cost to play me, return a friendly gear to its owner's hand."
  - Code: gates playability with a `hasLegalTargets` override, but the engine consults `hasLegalTargets` only for spells and activated abilities, never for a unit play (the unit gate is `Card::canBePlayed`, `game_engine.cpp:5608`). The file comment claims the gate works.
  - Scenario: no friendly gear on board -> Quartermaster is still offered and played; `pickTarget` finds nothing and the 3-cost 4-Might unit enters with no cost paid. (Extends 7.6, which only lists the late payment.) Cruel Patron 208 has the same hole and no gate at all.
  - c4: 208 Cruel Patron — FIXED: `canBePlayed` gates the play on a friendly unit to kill; see 07 item 7.6; test: C4Test.CruelPatron_NeedsAFriendlyUnitAndKillsTheChosenOne
- [x] **162 Miss Fortune, Captain** - `units/0162_miss_fortune_captain.cpp:38-48` [confirmed] medium — FIXED: candidates are everything exhausted other than her — on-board objects and legends in their Legend Zone, either side (not enemy objects that can't be chosen); test: C4Test.MissFortuneCaptain_FirstMoveOnly_AndLegendsCount
  - Text: "you may ready something else that's exhausted."
  - Code: candidates need `obj.location.has_value()` and `controller == ctx.controller`. Legends live in `ZoneType::LegendZone` with no location (`game_engine.cpp:4464`).
  - Scenario: your legend is exhausted and it is the only exhausted thing you have -> the trigger is not offered at all; with other exhausted objects the legend is never among the choices. (Text is also not limited to friendly objects.)
- [x] **730 Undying Loyalty** - `spells/0730_undying_loyalty.cpp:32-36,38-54` [documented] medium — FIXED: eligible = at most 2 Energy and at most one Power; the [2] discount is given up front when a Bird/Cat/Dog/Poro unit is in the trash and paid back if another unit is chosen (offered only if it can be), so the cost follows the unit chosen; location chosen; test: C3Test.UndyingLoyalty_OnlyABirdCatDogOrPoroEarnsTheDiscount, C3Test.UndyingLoyalty_AUnitThatIsNotAPoroIsNotOfferedWithoutTheTwoEnergy
  - Text: "This costs [2] less if you choose a Bird, Cat, Dog, or Poro. Play a unit with cost no more than [2] and no more than [A] from your trash"
  - Code: `selfCostReduction` returns 2 if ANY unit with one of the four tags is in trash (no Energy check, not tied to the unit actually chosen); `eligible` ignores Power cost entirely.
  - Scenario: trash holds a 5-cost Poro and a 2-cost non-Poro -> the spell costs 0 Energy and revives the non-Poro. A unit costing [2][R][R] is revived although it exceeds "[A]". Not listed with Matron/Glasc in 7.15.
- [x] **702 Conscription** - `spells/0702_conscription.cpp:67` [confirmed] medium — FIXED: the recall writes the location (no UnitMovedEvent, no move blockers), as Hostile Takeover does; test: C3Test.Conscription_WithoutTheXpOnlySmallUnitsAndTheRecallIsNotAMove
  - Text: "Take control of it, exhaust it, and recall it."
  - Code: `moveToBase(picked)`, which emits `UnitMovedEvent` and honours move blockers. CR 457-458: a Recall is not a Move and does not trigger move abilities (Hostile Takeover 516:86-96 does it correctly by writing the location).
  - Scenario: conscript Miss Fortune, Captain (or any "when I move" unit) -> her move trigger fires for the new controller, readying something for free.
- [x] **604 Back Off** - `spells/0604_back_off.cpp:37` [confirmed] low — FIXED: `stunUnitBy(picked, ctx.source)`; test: C3Test.BackOff_StunsForItsControllerAndDrawsOnlyWhenPlayedFromHand
  - Text: "[Stun] a unit."
  - Code: `stunUnit(picked)` = `stunUnitBy(target, target)`, so `UnitStunnedEvent.stunner` is the victim's controller (`effect_executor.cpp:773-800`).
  - Scenario: you Back Off an enemy unit while controlling Vex, Mocking -> "when you stun" never fires. Heroic Charge 717:67 and Bone Skewer 701:87 use `stunUnitBy`. (Solari Shieldbearer 51:19 and Leona 238:19 have the same call behind the dead S1 path.)
- [ ] **Target requirements that cannot express the printed restriction (7 S1 cards)** - `units/0076_yasuo_remorseful.cpp:29`, `units/0130_crackshot_corsair.cpp:28`, `units/0238_leona_determined.cpp:23`, `units/0132_first_mate.cpp:23`, `units/0188_zaunite_bouncer.cpp:23`, `units/0361_ribbon_dancer.cpp:23`, `units/0191_maddened_marauder.cpp:23`, `units/0199_tideturner.cpp:24-33` [confirmed] low
  - Text: "an enemy unit here" (76, 130, 238); "another unit" / "another friendly unit" (132, 188, 361); "a unit from a battlefield" (191); "a unit you control at another location" (199)
  - Code: `TargetRequirements` has no "here" / "not me" field; 76/130/238 say only `must_be_at_battlefield`, 132/188/361 allow the source itself, 191 allows units already in base, 199's override allows the Tideturner itself and units at its own location.
  - Scenario: if 7.1 is fixed by letting the engine choose trigger targets from `getTargetRequirements()`, Yasuo/Corsair/Leona hit units at other battlefields, First Mate readies itself, Bouncer bounces itself, Marauder "moves" a unit already in base. These need `pickTarget` with a card-built list.
  - c4: 76, 130, 132, 188, 191, 199 — FIXED: `getTargetRequirements()` deleted; each builds its own list ("here", "another", "from a battlefield", "at another location"); see 07 item 7.1; test: C4Test.YasuoRemorseful_HitsOnlyAnEnemyUnitHere, CrackshotCorsair_*, FirstMate_*, ZauniteBouncer_*, MaddenedMarauder_*, Tideturner_*
  - c5: 238 Leona, Determined ("an enemy unit here", `stunUnitBy` so the stunner is Leona) and 361 Ribbon Dancer ("another friendly unit") — FIXED: card-built lists with `pickTarget`, `getTargetRequirements()` deleted; test: C5Test.LeonaDeterminedStunsAnEnemyUnitHere, C5Test.RibbonDancerBuffsAnotherFriendlyUnit
- [ ] **"Can't be chosen by enemy spells and abilities" ignored** - `spells/0511_defiant_dance.cpp:20-37`, `units/0119_ahri_inquisitive.cpp:27-31`, `units/0121_teemo_strategist.cpp:28-33`, `units/0149_carnivorous_snapvine.cpp:25-29`, `units/0200_twisted_fate_gambler.cpp:52-56,87-91`, `units/0067_blitzcrank_impassive.cpp:42-49` [confirmed] low
  - Text: each chooses "a unit" / "an enemy unit"
  - Code: hand-built legal lists with no `untargetable_by_enemy` test; `pickTarget` / `pickTargetPair` do not add one (only `enumerateLegalTargets` and `chooseableUnits` do).
  - Scenario: opponent controls Baron Nashor / Ruin Runner / a Twilight Shroud unit -> Defiant Dance can give it -2 [M], Snapvine can fight it, Ahri can shrink it, Blitzcrank can pull it.
  - c4: 119 Ahri, 121 Teemo Strategist, 149 Snapvine, 200 Twisted Fate, 67 Blitzcrank — FIXED: lists built with `chooseableUnits`; test: C4Test.CantBeChosenUnitsAreNotOfferedToAhriTeemoOrSnapvine, C4Test.Blitzcrank_PullsTheEnemyUnitItsControllerChooses. Reprint Ahri 539 is outside this range.
  - c3: 511 Defiant Dance — FIXED: both picks through `chooseableUnits`; test: C3Test.DefiantDance_TheSecondUnitIsStillAffectedWhenTheFirstIsGone (same code path)
- [ ] **Effect-played units always land in base** - `units/0006_flame_chompers.cpp:35`, `units/0037_immortal_phoenix.cpp:68`, `units/0196_soulgorger.cpp:53`, `units/0226_spectral_matron.cpp:72`, `units/0194_nocturne_horrifying.cpp:43`, `units/0349_rumble_hotheaded.cpp:92`, `spells/0704_heedless_resurrection.cpp:86`, `spells/0730_undying_loyalty.cpp:73`, `spells/0515_arcane_shift.cpp:31`, `spells/0513_arise.cpp:103` [confirmed; Arise documented] low
  - Text: "play me" / "play a unit from your trash" / "play a ... token"
  - Code: `playIgnoringCost` / `playPayingCost` called with no location or a hard-coded base. CR 355.2.a lets the controller choose base or a battlefield they control; Wild Claw 876:138-151 shows the prompt.
  - Scenario: you control a battlefield and revive a unit with Soulgorger -> it can only go to base, never to the battlefield to hold/defend.
  - c4: 6 Flame Chompers, 37 Immortal Phoenix, 196 Soulgorger, 194 Nocturne — FIXED: the controller chooses base or a battlefield they control through new `pickEffectPlayLocation` / `effectPlayLocation` in `card_helpers.h` (`shared:` commit `56f226f`; other streams can use it); test: C4Test.FlameChompers_PaysAndChoosesItsLandingPlace, C4Test.Soulgorger_PlaysTheChosenUnitWhereItsControllerChooses, C4Test.Nocturne_*, C4Test.ImmortalPhoenix_*
  - c3: 704 Heedless Resurrection, 730 Undying Loyalty, 515 Arcane Shift — FIXED: `playLocations` + `pickMode`; 513 Arise! — FIXED: per-token location (`playTokensChoosingLocations`); test: C3Test.Arise_EachSandSoldierCanBePlayedToAControlledBattlefield, C3Test.ArcaneShift_ReplaysTheUnitOnceAndBanishesItself, C3Test.HeedlessResurrection_TheKillIsPaidAsItIsPlayedAndTheUnitCanComeBack
  - c5: 226 Spectral Matron and 349 Rumble, Hotheaded — FIXED: base or a battlefield the controller controls, chosen as the unit is played (`pickMode`, only when there is a choice); test: C5Test.SpectralMatronLimitsThePrintedCostAndAsksWhere
- [x] **704 Heedless Resurrection** - `spells/0704_heedless_resurrection.cpp:75-80` [documented] low — FIXED: `kFinalizeResolve` — the unit is chosen and killed as the spell is played; the unit to play is chosen at resolution from the trash as it is then; test: C3Test.HeedlessResurrection_TheKillIsPaidAsItIsPlayedAndTheUnitCanComeBack
  - Text: "As an additional cost to play this, kill a friendly unit."
  - Code: the kill happens in `onResolve` (no `kFinalizeResolve`, no cost hook).
  - Scenario: the spell is countered -> the friendly unit survives although the cost should already be paid; the opponent can also respond to the spell by removing the chosen unit, which then fizzles the revive budget. Same class as 7.6 (units), not listed there.
- [x] **693 Abandon** - `spells/0693_abandon.cpp:23-28,36-38` [confirmed] low — FIXED: `hasLegalTargets` requires the chain top to be a spell; the Predict recycle goes through `recycleCards` (announces "recycled_main", 02 item 2.15); test: C3Test.CountersAreOnlyPlayableAgainstTheChainTop, C3Test.Abandon_RecyclingThePredictedCardIsAnnounced
  - Text: "Counter a spell."
  - Code: `hasLegalTargets` is true if ANY chain item is a spell, but resolution only looks at `items.back()` and does nothing unless that one is a spell.
  - Scenario: opponent's spell is on the chain with a triggered ability above it -> Abandon is offered, costs [2], and counters nothing (only the Predict happens).
- [x] **701 Bone Skewer** - `spells/0701_bone_skewer.cpp:38-41,58-63` [documented] low — FIXED: battlefield (`pickMode`), reveal once, then a real "may" and the unit; test: C3Test.BoneSkewer_ChoosingAUnitIsOptional
  - Text: "You may choose a unit from it."
  - Code: `pickTargetPair` forces a pick whenever the opponent's hand holds a unit.
  - Scenario: the opponent's only hand unit has a strong play effect -> you must put it into play for them.
- [x] **230 Albus Ferros** - `units/0230_albus_ferros.cpp:33-43` [confirmed] low — FIXED: the buffs are chosen one at a time as the ability resolves (`pickSeveral`), runes follow the buffs actually spent; test: C5Test.AlbusFerrosSpendsTheBuffsThePlayerChooses
  - Text: "spend any number of buffs"
  - Code: buffs are removed greedily in `state.objects` iteration order; the player chooses only the count.
  - Scenario: two buffed units, spend 1 -> the engine picks which unit loses its buff.
- [ ] **playPayingCost result ignored after the card left its zone** - `spells/0876_wild_claw.cpp:153,165`, `units/0196_soulgorger.cpp:52-53`, `units/0112_kai_sa_evolutionary.cpp:53,62`, `units/0349_rumble_hotheaded.cpp:91-92` [plausible] low
  - Code: the card is erased from trash / deck+banishment, then `playPayingCost` is called and its `false` return is not handled (the header says the caller must leave the card where it was). Extends the 7.35 list (491, 553, 710); as there, `canPlayPayingCost` is checked just before, so no failing path was found today. Wild Claw is the most exposed: an agent decision sits between the check (resume point 1) and the payment (resume point 2).
  - c4: 196 Soulgorger, 112 Kai'Sa — FIXED: neither erases the card first (`playPayingCost` removes it only when paid, 08 item 8.13) and both handle a `false` return; only units / spells whose Power can be paid are offered.
  - c3: 876 Wild Claw — FIXED: the chosen card stays listed in banishment until `playPayingCost` succeeds (which removes it), so a failed payment leaves it banished, not zoneless; no test: no failing-payment path could be constructed (the finding is [plausible])
  - c5: 349 Rumble, Hotheaded — FIXED: no manual erase; `playPayingCost` takes the Mech out of the trash only if it is played; test: C5Test.RumbleHotheadedLetsThePlayerChooseBothCards
- [x] **890 Shadows of the Past** - `spells/0890_shadows_of_the_past.cpp:86` [confirmed] low — FIXED: stray `effect_text` removed (data only)
  - Code: `d.effect_text = "1"` - stray data; no other spell in the batch sets `effect_text`.

## Systemic
- S1: none new (16, 51, 76, 82, 92, 97, 130, 132, 188, 191, 199, 234, 238, 330, 361 are all already in 7.1; see the requirements finding above for what else is wrong with eight of them).
- S2: 147 Wildclaw Shaman (`:30-33` buffs and readies `ctx.source` after only `objectExists`), 343 Draven, Vanquisher (`:53`, no visible effect). Others in this batch (35, 56, 67, 149, 177, 331, 347) are already in 3.14 / 7.26 / 7.27.
  - c4: 147 — FIXED with 03 item 3.9 (`buffUnit` / `readyObject` refuse an off-board Shaman).
  - c5: 343 Draven, Vanquisher — FIXED: +2 only while he is on the board.
- S3: 349 Rumble, Hotheaded (first-instance scan inside the per-instance hook, plus the engine text aura; see finding).
  - c5: 349 — ALREADY FIXED (`73e121e`, `1ae4c88`), see the finding.
- S4: 56 Adaptatron (`:42`), 235 Karma, Channeler (`:40`).
  - c4: 56 — ALREADY FIXED by `EffectExecutor::buffUnit` (02 item 2.14).
  - c5: 235 Karma, Channeler — ALREADY FIXED by `buffUnit` (`99d179e`, one Buff per unit).
- S5: 230 Albus Ferros (which buffs are spent). 67, 112, 349 already in 7.24.
  - c5: 230 — FIXED, see the finding; 349 Rumble chooses both units now (07 item 7.24).
- S7: none new (727 Shadow's Call is 7.9).

## Corrections
- 07 item 7.1 "Decision needed": "The engine route fixes ~22 of these at once" is too optimistic. Of the 15 cards from that table in this batch, 8 (76, 130, 238, 132, 188, 361, 191, 199) have target requirements that do not express the printed restriction ("here", "another", "from a battlefield", "at another location"), so engine-chosen targets would be wrong; only 16, 51, 82, 92, 97, 234, 330 are correct as written.
- 07 item 7.6: Legion Quartermaster is described only as a late payment; its playability gate (`hasLegalTargets`) is dead code for a unit, so it is "free with no gear on board" exactly like Cruel Patron.
- 03 item 3.2 / 07 read-clean list: no errors found for this batch's cards. 0119 Ahri (clean list) is correct apart from the missing untargetable filter above; the S1 Ahri is 0539.

## Not read
None
