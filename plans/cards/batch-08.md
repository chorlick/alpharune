[Back to the full card review](../08-cards-full-review.md)

# Batch 08 - battlefields/0274_fortified_position.cpp .. spells/0472_bonds_of_strength.cpp

Files read in full: 81 of 81.

Only findings not already in `03-cards.md` / `07-cards-second-pass.md` are listed.

## Findings
- [x] **242 Baited Hook** - `gear/0242_baited_hook.cpp:24` [confirmed] high — FIXED: cost is `{exhaust, energy 1, power 1 Order}` ([C] is this card's Domain, CR 135.2.e.6 — not Chaos as 02 item 2.27's note says); the gear stays on the board; test: C0Test.BaitedHook_PaysPowerStaysOnBoardAndPlaysThePickedUnit
  - Text: "[1][C], [E]: Kill a friendly unit. ..." ([C] = one Power of this card's Domain, CR 135.2.e.6.)
  - Code: the cost is `ActivationCost{.exhaust = true, .energy = 1, .recycle_self = true}`. `recycle_self` means "recycle this card": `game_engine.cpp:1463-1468` puts the source gear on the bottom of the Main Deck. No Power is charged (`power` is 0).
  - Scenario: activate Baited Hook once -> the gear leaves the board into the deck (a repeatable 3-cost engine becomes one-shot) and the Order Power is never paid. Should be `.power = 1, .power_domain = Domain::Order`.
- [x] **556 Chem-Baroness** - `legends/0556_chem_baroness.cpp:19` [confirmed] medium — FIXED: `WhenIHold`; test: ReviewFixC1Test.ChemBaroness_DoesNotTriggerOnAConquer
  - Text: "When you or an ally hold, you may exhaust me to play a Gold gear token exhausted."
  - Code: `triggerType()` is `WhenIConquerOrHold`; `trigger_manager.cpp:731` fires that on every score, Conquer included. Gloomist (785), same wording, correctly uses `WhenIHold`.
  - Scenario: conquer a battlefield -> the Gold prompt is offered; a hold-only legend makes Gold on conquers too.
- [x] **21 Sun Disc** - `gear/0021_sun_disc.cpp:24` [confirmed] medium — FIXED: `onActivate` arms only when a card other than Sun Disc was played this turn (CR 812.1.c; the Disc notes its own play turn in `onPlay`); dead `requiresLegion()` override removed; test: C0Test.SunDisc_NeedsAnotherCardPlayedThisTurn, .SunDisc_DoesNotCountItselfOnTheTurnItIsPlayed
  - Text: "[E]: [Legion] — The next unit you play this turn enters ready. (Get the effect if you've played another card this turn.)"
  - Code: the only Legion gate is `requiresLegion() { return true; }`, and nothing in `src/engine` reads `requiresLegion()` (only `card.h` declares it). `onActivate` arms unconditionally.
  - Scenario: first action of the turn: exhaust Sun Disc, then play a unit -> it is readied although no other card was played.
- [x] **695 Blast Cone** - `gear/0695_blast_cone.cpp:52,55-69` [confirmed] medium — FIXED: the stun is a standing trigger on new `TriggerType::WhenYouMoveAnEnemyUnit` (dispatched from `TriggerManager::onUnitMoved` when a spell or ability of another player moves the unit, `shared:` commit); the on-play move lets the controller choose the unit and its destination; test: ReviewFixC1Test.BlastCone_StunsAUnitMovedByAnotherOfYourAbilities, ReviewFixC1Test.BlastCone_TheControllerChoosesTheUnitAndWhereItGoes, ReviewFixC1Test.BlastCone_AUnitThatMovesItselfDoesNotTrigger
  - Text: "When you move an enemy unit, you may exhaust this to [Stun] it." (a standing trigger, separate from the on-play sentence) and "you may move an enemy unit" (no destination restriction).
  - Code: the stun exists only as a tail of the `WhenYouPlayThis` handler; there is no trigger type for "you moved an enemy unit" (`grep -i MoveAnEnemy src` is empty). The on-play move is always `moveToBase`.
  - Scenario: Blast Cone on board, later play Temptation (450) to move an enemy unit -> no stun offer. On play, an enemy unit can only be sent to its base, never to another battlefield.
- [x] **522 Forge of the Fluft** - `battlefields/0522_forge_of_the_fluft.cpp:37` [confirmed] medium — FIXED: attached Equipment is a legal pick (moved with `attachFree`), the unit it is already on is not; `hasLegalTargets` stops the ability being offered with nothing to attach; test: C0Test.ForgeOfTheFluft_MovesAnAttachedEquipment, .ForgeOfTheFluft_NotOfferedWithNothingToAttach
  - Text: "[E]: Attach an Equipment you control to a unit you control."
  - Code: candidates require `!obj.attached_to.has_value()`. CR 435.1.f: attaching an attached card detaches it from its current bearer, and `attachFree` already handles that.
  - Scenario: your only Equipment is attached to unit A -> `equips` is empty, the legend is exhausted and nothing happens; the Equipment can never be moved to unit B.
- [ ] **29 Falling Star / 466 Switcheroo / 334 Angle Shot / 187 Whirlwind** - `spells/0029_falling_star.cpp:26-31`, `spells/0466_switcheroo.cpp:25-29,35-39`, `spells/0334_angle_shot.cpp:27-31`, `spells/0187_whirlwind.cpp:24-29` [confirmed] medium
  - Text: each chooses "a unit".
  - Code: the hand-built candidate lists omit the `untargetable_by_enemy` filter that `Card::enumerateLegalTargets` / `chooseableUnits` apply (Challenge 128, Facebreaker 220, Strike Down 429 in this batch do filter). Whirlwind also builds raw `MakeChoice` options, so no Deflect is charged (same class as 07 item 7.30).
  - Scenario: opponent controls a "can't be chosen by enemy spells and abilities" unit (Baron Nashor) -> Falling Star deals 3 to it, Switcheroo swaps its Might, Whirlwind bounces it.
  - c2: 29 Falling Star, 334 Angle Shot, 187 Whirlwind — FIXED: candidates come from `chooseableUnits` (Angle Shot also filters the Equipment); Whirlwind builds each player's options with `offerableChoices` and pays Deflect with `commitChosenObject`, from the choosing player's side; test: ReviewFixC2Test.HandBuiltUnitChoicesSkipAnUnchoosableEnemyUnit, ReviewFixC2Test.Whirlwind_ChoosingAnOpponentsDeflectUnitCostsPower. 466 Switcheroo is outside the c2 range.
  - c3: 466 Switcheroo — FIXED: both picks through `chooseableUnits`; test: C3Test.Switcheroo_DoesNotOfferAUnitThatCannotBeChosenByEnemySpells
- [x] **63 Spirit's Refuge** - `gear/0063_spirit_s_refuge.cpp:36-45,63-64` [confirmed] medium — FIXED: (a) units that already have [Deflect] are skipped (`hasKeywordInAuraPass`), (b) so a second Refuge adds nothing, (c) the def no longer gives the gear itself [Deflect]; test: C0Test.SpiritsRefuge_DeflectOnlyIfTheUnitHadNone, .SpiritsRefuge_BuffsAChosenUnitWhenPlayed
  - Text: "Friendly buffed units have [Deflect] if they didn't already."
  - Code: (a) every buffed unit gets a Deflect-1 aura with no "didn't already" check, and `deflectCostFor` (`effect_executor.cpp:368-370`) sums `deflect_value` plus each aura, so a unit with printed Deflect costs 2. (b) The hook is called once per on-board Refuge and each call adds another aura from the first instance (S3), so two Refuges give Deflect 2. (c) The def sets `d.keywords.set(Keyword::Deflect)` and `d.deflect_value = 1` on the gear itself (`game_engine.cpp:5250-5253` copies both), so the opponent pays [A] to choose the Refuge gear, which the card does not say.
  - Scenario: opponent targets your buffed Deflect unit -> charged 2 [A] instead of 1; opponent targets Spirit's Refuge with a gear-kill -> charged 1 [A].
- [x] **556 Chem-Baroness** - `legends/0556_chem_baroness.cpp:38-39,57` [documented] medium — FIXED: the legend's aura hook raises the Gold bonus while its controller's score is within 3 of the Victory Score, and the Gold cards add the extra [1]; test: ReviewFixC1Test.ChemBaroness_GoldAddsAnExtraEnergyNearTheVictoryScore
  - Text: "While your score is within 3 points of the Victory Score, your Gold [ADD] an additional [1]."
  - Code: not implemented (the trace line says "Gold-add bonus passive not modeled"; nothing in `gear/0326_gold.cpp` reads the score).
  - Scenario: at 5+ points, killing a Gold adds only [A], never the extra [1].
  - c0: 326 Gold — the Gold half is done: `gear/0326_gold.cpp` adds floating Energy equal to `card_counters["__gold_add_bonus_energy"]` on its controller's legend object (absent / 0 = none). Chem-Baroness 556 (c1) only has to set that counter on itself from `applyPassiveAura` while the score condition holds; test: C0Test.Gold_AddsTheLegendsBonusEnergy
- [x] **259 Swift Scout** - `legends/0259_swift_scout.cpp:18-20` [documented] medium — FIXED (sweep-costs): `Card::adjustHideCost` (`GameEngine::hideCostFor` / `canPayHideCost`); test: SweepCostsTest.SwiftScout_HidesForOneEnergyInsteadOfThePower
  - Text: "You may pay [1] to hide a card with [Hidden] instead of [A]."
  - Code: not implemented ("ENGINE GAP" comment); the hide cost path (`game_engine.cpp:2505`) always recycles a rune.
  - Scenario: Teemo player with one ready rune and no rune to spare for Power cannot hide a card for [1].
  - BLOCKED: the Hide cost is charged by `GameEngine::executeHideCard` (`payAdditionalCost(player, 0, 1, Domain::Count)`) and gated by the hide generator, with no card hook; it needs a per-player alternative hide cost (e.g. `PlayerState::hide_cost_may_be_energy`, set from the legend's aura hook) read at both sites, with a choice when both can be paid.
- [x] **309 Flash** - `spells/0309_flash.cpp:30-44` [documented] low — FIXED: the units are chosen one prompt at a time at finalization and "No more units" is offered after the first; test: ReviewFixC2Test.Flash_UpToTwo_OneUnitCanStayBehind, ReviewFixC2Test.Flash_StillMovesTwo
  - Text: "Move up to 2 friendly units to base."
  - Code: `pickTargetPair` has no decline; with two or more friendly units at battlefields exactly two must be moved, and zero is impossible when one exists.
  - Scenario: you want to pull one defender out and keep the second holding -> both are moved to base.
- [x] **306 Gentlemen's Duel** - `spells/0306_gentlemen_s_duel.cpp:46-51` [plausible] low — ALREADY FIXED: confirmed plausible, fixed in the helper (07 item 7.32: `pickTargetPair` returns `{kInvalidId, kInvalidId}` while waiting for the second pick, commit `f892d67`), so `friendly == kInvalidId` is the suspend; the card is unchanged; test: ChoiceHelperTest.PickTargetPairReturnsNothingWhileWaitingForTheSecondPick, ReviewFixC2Test.GentlemensDuel_GivesTheBonusOnce
  - Text: "Give a friendly unit +3 [M] this turn."
  - Code: only `friendly == kInvalidId` is treated as "suspended". While the second pick is pending `pickTargetPair` returns `{a, kInvalidId}` (resume_point 12, `card.cpp:583-584`), so `giveTemporaryMight(friendly, 3)` runs, then runs again on re-entry. Masked by the finalize pass; reachable when choices are redone at resolution (`game_engine.cpp:1312-1321`, "make new choices"). Same shape as 07 item 7.32; this card is not listed there.
  - Scenario: opponent's Rebuttal makes new choices for the Duel -> friendly unit gets +6 and deals 3 extra damage.
- [x] **242 Baited Hook** - `gear/0242_baited_hook.cpp:54-71` [documented] low — FIXED: the pick may be declined (`selectObject` with "None"); the unit is banished (`banishObject`: counted, announced) and then played from there; the rest is announced as recycled; test: C0Test.BaitedHook_PlayingAUnitIsOptional, .BaitedHook_PaysPowerStaysOnBoardAndPlaysThePickedUnit
  - Text: "You may banish a unit from among them ... and play it"
  - Code: when any eligible unit is in the top 5 the pick is mandatory (comment: "we always play the chosen one"); the unit is also played straight from the deck with no banish step, so banish triggers / `cards_banished_this_turn` never see it.
  - Scenario: the only eligible unit is one you would rather recycle -> it is played anyway.
- [ ] **671 Blood Rose / 783 Piltover Enforcer / 274 Fortified Position** - `gear/0671_blood_rose.cpp:47-48`, `legends/0783_piltover_enforcer.cpp:32`, `battlefields/0274_fortified_position.cpp:25` [confirmed] low
  - Text: "Ready a unit." / "ready a unit." / "choose a unit. It gains [Shield 2] this combat."
  - Code: all three restrict the choice to friendly units. Matriarch of War (940), same "Ready a unit" wording, offers every unit.
  - Scenario: an enemy unit can never be chosen (rarely wanted, but legal by the text).
  - c0: 274 Fortified Position — FIXED: offers every chooseable unit on the board, either player's; test: C0Test.FortifiedPosition_OffersEveryUnit
  - c2: 940 Matriarch of War — NOT A DEFECT: named only as the correct comparison ("Ready a unit." offers every unit, `enumerateLegalTargets` in `legends/0940_matriarch_of_war.cpp`); nothing to change.
  - c1: 671 Blood Rose, 783 Piltover Enforcer — FIXED: any unit on the board can be chosen (Blood Rose also had no target list of its own, so its ability was never offered); test: ReviewFixC1Test.BloodRose_ReadiesAnyUnit, ReviewFixC1Test.PiltoverEnforcer_CanReadyAnEnemyUnit. 274 Fortified Position is not in c1's range.
- [x] **698 Scryer's Bloom** - `gear/0698_scryer_s_bloom.cpp:46-58,73-79` [confirmed] low — FIXED: when both cards are kept the controller chooses which goes on top; test: ReviewFixC1Test.ScryersBloom_KeptCardsGoBackInTheChosenOrder
  - Text: "[Predict 2] ... Recycle any of them and put the rest back in any order."
  - Code: the two cards are popped top-first and each kept card is `push_back`ed in that order, so keeping both always reverses them; the player is never asked for an order.
  - Scenario: keep both -> the following "draw 1" draws the card that was second from the top, not the original top card, with no way to choose.
- [x] **21 Sun Disc** - `gear/0021_sun_disc.cpp:49` [confirmed] low — FIXED: the unit that was played uses the arm up whether or not it needed readying; test: C0Test.SunDisc_ReadiesThePlayedUnitAndIsUsedUpByIt
  - Text: "The next unit you play this turn enters ready."
  - Code: when no friendly unit is exhausted the handler returns before erasing `__sundisc_arm_turn`.
  - Scenario: arm Sun Disc, play a unit with [Accelerate] (already ready, board otherwise ready) -> the arm survives and the second unit played that turn is readied instead.
- [x] **782 Virtuoso** - `legends/0782_virtuoso.cpp:63-65` [confirmed] low — FIXED: banishes through `EffectExecutor::banishObject`; test: ReviewFixC1Test.Virtuoso_BanishesTheSpellThatWasPlayedAndCountsIt
  - Text: "you may banish it"
  - Code: banishes by direct zone write (erase from trash, set zone, push to `banishment`); skips `cards_banished_this_turn` and the `"banished_owned"` event. Same defect as 07 item 7.31, which does not list this card.
  - Scenario: a "when a card you own is banished" trigger never fires for Virtuoso's banish.
- [x] **457 Hard Bargain** - `spells/0457_hard_bargain.cpp:20-25,34-38` [confirmed] low — FIXED: `hasLegalTargets` requires the chain top to be a spell; test: C3Test.CountersAreOnlyPlayableAgainstTheChainTop
  - Text: "Counter a spell unless its controller pays [2]."
  - Code: `hasLegalTargets` accepts a spell anywhere on the chain, but resolution only looks at `chain.items.back()` and no-ops if that is not a spell.
  - Scenario: chain is [enemy spell, enemy triggered ability]; Hard Bargain is offered, paid for, and does nothing.
- [x] **248 Stormbringer** - `spells/0248_stormbringer.cpp:55-59` [confirmed] low — FIXED: the battlefield is chosen (07 item 7.21) from every battlefield, so with no enemy unit anywhere the unit still moves; test: ReviewFixC2Test.Stormbringer_MovesToAnEmptyBattlefield
  - Text: "Deal damage ... to all enemy units at a battlefield, then move your unit there."
  - Code: if no enemy unit is at any battlefield the spell returns before the move (follows from the unchosen battlefield in 07 item 7.21, but a different wrong result).
  - Scenario: enemy has no units at battlefields -> your base unit is not moved to the battlefield.

## Systemic
- S2: 242 Baited Hook (`targets[0]` killed and its Might read after only `objectExists`; CR 359.3.e.12 uses this exact card as the example), 152 Mistfall (`readyObject(subject)`), 555 Blade Dancer (`readyObject(chosen)` after the cost is paid), 108 Convergent Mutation (`targets[0]`)
  - c0: 242 Baited Hook — FIXED (the victim must be a unit on the board; its Might is read before the kill and kept on the chain item; a replaced death stops the ability); 152 Mistfall — FIXED (the subject must still be a unit on the board); test: C0Test.BaitedHook_PaysPowerStaysOnBoardAndPlaysThePickedUnit, .Mistfall_NotOfferedWithoutThePowerAndPaysFromThePool
  - c2: 108 Convergent Mutation — ALREADY FIXED: `targets[0]` reaches the card only if it is still a legal target (on the board and friendly; `GameEngine::resolveSpell` re-validation) and `giveTemporaryMight` / the executor guard (02 item 2.28) ignore an off-board unit; the card is unchanged.
  - c1: 555 Blade Dancer — FIXED: readies the chosen unit only if it is still a unit on the board
- S3: 63 Spirit's Refuge (see finding)
- S4: 787 Voidreaver (ability 0), 278 Navori Fighting Pit, 63 Spirit's Refuge (the last two are also S1, already listed in 07)
  - c0: 278 Navori Fighting Pit, 63 Spirit's Refuge — ALREADY FIXED by the executor (`99d179e`: one Buff per unit); S3 for 63: FIXED, see the finding
  - c1: 787 Voidreaver — ALREADY FIXED by 02 item 2.14 (`buffUnit`)
- S1, S5, S6: nothing beyond the cards already listed in 03 / 07. S7: none in this batch.

## Corrections
- 07 "Read in full and clean" lists spells 0187, 0334, 0466, battlefield 0274 and legend 0259. That holds only for the narrow classes that pass checked; against the full checklist each has a finding above (untargetable filter for 0187 / 0334 / 0466, friendly-only choice for 0274, unimplemented first sentence for 0259).
- `gear/0710_cursed_sarcophagus.cpp:18-20` header comment says the replay is free; the code at `:82-88` does charge the full cost through `playPayingCost`. The code is right and the comment is stale, so 07's note on 0710 stands and no cost defect exists there.
  - c1: stale header comment corrected
- No entry in 03 or 07 for a card in this batch was found to be wrong.

## Not read
None
