# 2. Engine rules bugs

Line numbers are in `src/engine/game_engine.cpp` unless another file is named.

## A. Auras

- [ ] **2.1 Text-parsed auras stack on top of the cards' own `applyPassiveAura`** [verified]
  - Where: `:4500-4688`. Step 1b (`:4479`) runs each card's hook; Step 2 then pattern-matches the same `ability_text` and applies it again, often with the wrong scope.
  - Garen, Commander (311), "Other friendly units have +1 [M] here.": matches "other friendly units have" as all-friendly. Units at his battlefield get +2, every other friendly unit anywhere gets +1, even with Garen in base.
  - Wuju Master (784), "[Level 6][>] Your units have +1 [M].": Step 2 scans LegendZone objects (`:4501`) and ignores the level gate. +1 at any XP, +2 at level 6.
  - Rage Amplifier (805): +2, or +3 when Empowered, instead of +1/+2.
  - "units here have" is a substring of "other friendly units here have", so Captain Farron (15) also grants Assault to itself and to enemy units there.
  - `substr(pos, 80)` clause window and fixed keyword check order can pick up a keyword from a later sentence.
  - Card tests call `applyPassiveAura` directly, so none exercise this path.
  - Fix direction: delete Step 2 / Step 3b text parsing once every aura card has a hook (also resolves 5.3 and 6.2).

- [ ] **2.2 `applyPassiveAura` called once per instance, but many cards loop over all instances** [confirmed]
  - Where: `:4462-4481`; fold at `:4844` is a plain sum.
  - Two Garens: other units get four +1 effects. Dr. Mundo, Expert (109): two Mundos with 5 cards in trash each get +10. Wizened Elder (65), Captain Farron (15), Shurelya's (509) likewise. Mosstomper (609) also lacks a location check, so copies in hand get the aura.
  - `forEachSelfOnce` (`card_helpers.h:371`) is the existing fix; 34 aura cards use it.
  - List the rest: `grep -L 'forEachSelfOnce\|forEachEmpoweredSelf' $(grep -l applyPassiveAura src/cards/*/*.cpp) | xargs grep -l 'card_def_id [!=]= cardDefId()'`
  - Alternative: have the engine call once per (def, controller) or pass the source id.

## B. Chain, Repeat, spells

- [ ] **2.3 `payRepeatCost` recycles runes into the main deck and disagrees with its gate** [verified]
  - Where: `:5444-5484` (called from `executePlaySpell` ~`:1885`).
  - Sets `obj.zone = ZoneType::MainDeck` and inserts into `ps.main_deck` (`:5476-5478`); every other path uses `RuneDeck` and clears `is_exhausted` (`:6017-6021`, `:6141-6144`).
  - Gate is `canPayAdditionalCost` (counts pool energy/power, lets ready runes pay power); `payRepeatCost` ignores the pool and only recycles already-exhausted runes (`:5470`).
  - A: any paid Repeat with a power component permanently loses the rune. B: `[Repeat] [R]` with only ready runes — gate says yes, payment returns false, no repeat. C: energy spent then return false, no repeat.
  - No `LeftBoardEvent`, no `power_spent_this_turn++`.

- [ ] **2.4 The Repeat loop never services a pending choice** [verified]
  - Where: `chain_manager.cpp:433-447`. Each repeat resets resume state and calls `resolve_spell` once, with no `hasPendingChoice()` loop like `:390-424`.
  - Called Shot (443) with Repeat paid: re-enters case 0, requests a choice, returns; repeated effect never happens. `pending_.active` stays set, so the next chain item (`:405`) sees the stale choice and resolves twice.
  - Also Hard Bargain (457), Marching Orders (436), Temptation (450), Bonds of Strength (472) — all confirmed by the second pass (07 item 7.4). Frigid Touch (389) likely.

- [ ] **2.5 "Then recycle it" leaves the spell in both trash and main deck** [verified]
  - Where: `chain_manager.cpp:461-468`. Pushes onto `trash`, then `recycleCards` (`effect_executor.cpp:907-921`) inserts into the deck without removing from the source zone.
  - Fizz, Trickster (461) / Kai'Sa, Evolutionary (112): the id is in `trash` and `main_deck`; a later burn-out or Garbage Grabber pushes it into the deck again and it can be drawn twice.

- [x] **2.6 `executePlaySpell` writes spell metadata to `chain.items.back()`, which may be a trigger** [verified] — FIXED: `executePlaySpell` finds the spell's chain item by the id `addSpell` returns (same for the granted-ability stamp in `payAndQueueAbility`); test: PlayCombatFixTest.SpellMetadata_LandsOnTheSpell_NotOnATriggerPushedAboveIt
  - Where: `:1927-1937`. `addSpell` emits `chosen_by_controller` after pushing (`chain_manager.cpp:46-51`); `fireTrigger` synchronously pushes an ability. The returned `chain_id` is discarded.
  - With a `WhenYouChooseAFriendlyUnit` watcher (Blade Dancer 555, Irelia Fervent 380, Spirit Wheel 465, Jae Medarda 463, Dreaming Tree 287) and a targeted spell on a friendly unit: `repeats_paid` / `total_energy_spent` land on the trigger, and `resume_data[5]=1` is not set on the spell so `pickTarget` asks again at resolution.
  - Same `back()` pattern at `:1509` is safe today only because granted-ability intents carry no targets.

- [x] **2.7 `executeIntent` has no case for `PlayReaction` / `ActivateReactionAbility`** [verified] — FIXED: `executeIntent` routes `PlayReaction` (spell / permanent / hidden card) and `ActivateReactionAbility`; test: PlayCombatFixTest.Showdown_PlayReactionIntent_PlaysThePermanent
  - Where: `:1535-1578`; generated at `:2974`, routed by `resolveShowdownDecision` (`:3783-3789`).
  - Rengar, Pouncing (348) "pounce" play in a showdown hits `default: break`; focus passes are cleared and the action stays legal.

- [x] **2.8 `generateShowdownActions` offers `[Action]` abilities without the usual gates** [confirmed] — FIXED: the showdown ability loop applies the main-phase gates (`canActivateAbility`, `canActivateAbilityAt`, energy/power, typed discard count, XP); `payAndQueueAbility` refuses an activation whose exhaust / energy / power / XP cost cannot be paid before paying anything; tests: PlayCombatFixTest.Showdown_ActionAbility_NotOfferedWhenItsCostCannotBePaid, PlayCombatFixTest.ActivatedAbility_UnpayableCostIsRefused_NotQueuedForFree
  - Where: `:2983-3029`. Missing vs `:3186-3209` and `:3500-3538`: `canActivateAbility`, `canActivateAbilityAt` (Empower gate), energy/power affordability, discard count, XP.
  - `payAndQueueAbility` ignores the return of `payActivationResources` (`:1460-1461`) and does `xp -= xp_cost` unchecked (`:1489`).
  - Failure: ability goes on the chain for free; XP can go negative.

- [x] **2.9 Trash-replay / Flow plays bypass play-legality gates and cost tax** [confirmed] — FIXED: `generateTrashReplayActions` applies `Card::canBePlayed` and `locked_spell_names`; generator and `payTrashReplayGrant` both add cost increases to the grant (CR 356.1.a / 356.3: `spell_tax_energy`, `energy_increase` modifiers, and the [A] extras from `extraAnyPowerCost` = `spell_tax_power` + Mystic Vortex); tests: PlayCombatFixTest.TrashReplay_RespectsCanBePlayedAndTheNameLock, PlayCombatFixTest.TrashReplay_PaysTheSpellTaxOnTopOfTheGrant
  - Note for engine-auras-costs: the two gates are duplicated from the top of `canAfford` (their function); discounts (CR 356.4) are still not applied to a grant cost.
  - Where: `:3403-3491`. `generateTrashReplayActions` never calls `canAfford` (where `Card::canBePlayed` and `locked_spell_names` live, `:5604-5615`); `payTrashReplayGrant` charges only the grant, so `spell_tax_energy/power` and the Mystic Vortex reaction tax are skipped.

- [ ] **2.10 Safety caps fail silently** [plausible]
  - `processFEPR` exits after 100 iterations with the chain populated, `oc_state` Closed, no `ChainEmptiedEvent` (`chain_manager.cpp:161-199`); `runChain` (`:1944`) does not check.
  - Resume overflow is `assert(false)` only (`chain_manager.cpp:391-396`); in Release it leaves the recorded choice in `last_applied_choice_`.

- [ ] **2.11 Deflect payment result is ignored** [not confirmed] — `chain_manager.cpp:34`, `:119`.

## C. Triggers

- [x] **2.12 "When you play a unit here" battlefield triggers never fire on normal plays** [verified] — FIXED: both play paths set `card.location` before emitting `CardPlayedEvent` (`executePlayCard`; `chain_manager.cpp` in a `shared:` commit); test: PlayCombatFixTest.PlayUnitToBattlefield_FiresPlayHereTrigger_AndSetsBattlefieldZone
  - Where: `trigger_manager.cpp:362-367` reads `battlefieldId()` during `CardPlayedEvent`, but both play paths emit before setting `card.location` (`:1696` vs `:1703`; `chain_manager.cpp:333` vs `:343`).
  - Star Spring (771), Valley of Idols (774). Only facedown plays and `playIgnoringCost` set location first. Tests call the trigger directly.

- [ ] **2.13 Battle Mistress's legend trigger is never dispatched** [confirmed]
  - Where: `trigger_manager.cpp:665-679`. The `WhenAnEnemyUnitDies` sweep requires `location.has_value()`; legends have none (`:326`).

- [x] **2.14 Buff handling violates CR 427.1.b / 702.3** [verified for A] — FIXED: `buffUnit` places a Buff only on a unit that has none (new `Card::canHaveAnyNumberOfBuffs()` exempts Lee Sin, Ascetic 78), returns whether one was placed and emits `"buffed"` only then; `giveTemporaryMight` / `giveTemporaryKeyword` now emit `"might_changed"` / `"keyword_granted"`; test: ExecutorReviewFixTest.Buff_BlindMonkDoesNotStackASecondBuff, ExecutorReviewFixTest.Buff_LeeSinAsceticCanHaveAnyNumber, ExecutorReviewFixTest.Buff_SimianAncestorTriggersOnlyWhenActuallyBuffed, ExecutorReviewFixTest.TemporaryMight_IsNotABuff_FrigidTouchOnSimianAncestor
  - Card streams: every S4 card that goes through `ctx.executor.buffUnit` is capped with no card change. A card that prints "if it was buffed this way" should branch on the `bool` `buffUnit` now returns. Nothing listens for `"might_changed"` / `"keyword_granted"` yet.
  - A: `buffUnit` does `buff_count += 1` unconditionally (`effect_executor.cpp:672-683`). Only Showstopper guards it. Blind Monk's "[1],[E]: Buff a friendly unit" adds a permanent +1 every turn without limit.
  - B: `giveTemporaryMight` (including negative) and `giveTemporaryKeyword` emit the same `"buffed"` event (`effect_executor.cpp:645`, `:669`; dispatch `trigger_manager.cpp:1265-1285`). An opponent's Frigid Touch on Simian Ancestor fires "When you buff me, ready me".

- [x] **2.15 Predict / reveal-and-choose / Vision recycles never announce `"recycled_main"`** [confirmed] — FIXED: `predict` and `revealAndChoose` emit it once when they put a card on the bottom; the Vision block in `TriggerManager::onCardPlayed` emits it too (`shared:` commit); test: ExecutorReviewFixTest.Predict_RecyclingFiresWhenYouRecycle, ExecutorReviewFixTest.RevealAndChoose_RecyclingFiresWhenYouRecycle, ExecutorReviewFixTest.Vision_RecyclingFiresWhenYouRecycle
  - Card streams: cards that recycle with `deck::putOnBottom` directly still announce nothing — Abandon 693 and Scryer's Bloom 698 (hand-rolled Predict), Stacked Deck 183, Baited Hook 242 ("recycle the rest"), Last Rites 471 (Equip cost). Recipe: after the last `putOnBottom`, `ctx.events.emit(ObjectStateChangedEvent{<one recycled id>, "recycled_main"})` once, or hand the ids to `ctx.executor.recycleCards`.
  - Where: `effect_executor.cpp:1438-1441`, `:1530`; `trigger_manager.cpp:327-330`. Karma, Channeler does not trigger off Predict (CR 437.1).

- [ ] **2.16 Showdown battlefield triggers use `PlayerId::None` as controller** [latent]
  - Where: `trigger_manager.cpp:1335`. `state.player(None)` indexes `players[-1]`. No battlefield declares `WhenAShowdownBeginsHere` today.

- [ ] **2.17 Equipped triggers run synchronously inside signal dispatch** [not confirmed]
  - Where: `trigger_manager.cpp:123-144`. Skip the chain, `is_rules_text_inactive` and `triggerCondition`. Eye of the Herald (474) calls `createToken` inside `UnitMovedEvent` dispatch; a rehash would invalidate any caller iterating `objects`.

## D. Combat and scoring

- [x] **2.18 `CombatEndedEvent` carries the wrong winner** [confirmed] — FIXED: `combatResolutionStep` emits `std::nullopt` when there is no winner, and a combat whose attackers were recalled is "No Result" (CR 468.3.d); the player left alone still takes / keeps control (CR 468.5); tests: CombatResultTest.AttackerWhoWipesTheDefendersWins, CombatResultTest.DoubleWipeHasNoWinner, CombatResultTest.TieWithRecalledAttackersHasNoWinner
  - Where: `:4207-4233`. `combat_winner` may be `None` but the event field is `std::optional<PlayerId>`, always engaged; `onCombatEnded` (`trigger_manager.cpp:571`) fires `WhenIWinCombat` for both legends on a double wipe.
  - On a tie, attackers are recalled (`:4192`) and survivors recomputed (`:4208`), so the defender is declared winner; CR 468.3.d says "No Result".

- [x] **2.19 Final-point check ignores the effective victory score** [verified] — FIXED: `isWinningPointAttempt` uses `effectiveVictoryScore()`; test: ScoringFixTest.FinalPointCheck_UsesTheEffectiveVictoryScore
  - Where: `:4375-4377` uses `state_.mode.victory_score - 1`; `checkWinCondition` (`:5037`) uses `effectiveVictoryScore()`. Aspirant's Climb (271): a conquer at 7 is treated as the final point though 9 is needed.

- [x] **2.20 A conquer whose final point is replaced by a draw emits no `ScoreEvent`** [confirmed] — FIXED: the draw-instead branch emits `ScoreEvent` (CR 473.2); the `cannot_gain_points` branch of `scoreConquer` / `scoreHold` now marks the battlefield scored and emits it too (CR 470.1: scoring is distinct from gaining points; Tianna says "can't gain points"); tests: ScoringFixTest.ConquerWhoseFinalPointBecomesADraw_StillTriggersConquerAbilities, ScoringFixTest.CantGainPoints_StillScoresTheBattlefield
  - The turn-gated return is NOT A DEFECT: Forgotten Monument 523 reads "Players can't score here until their third turn", so there is no Score and nothing to trigger or mark.
  - Where: `:4315-4320`. Battlefield is marked scored but `WhenIConquer` never fires (CR 473.2); the Otterpus branch (`:4305`) does emit.
  - The `cannot_gain_points` and turn-gated early returns (`:4273-4287`, `:4335-4347`) also skip the event and the scored mark [plausible].

- [x] **2.21 Burn-out point bypasses the "can't gain points" lock** [confirmed] — FIXED: `burnOut` checks `cannot_gain_points` and emits `PointsGainedEvent`; `burnOutUntilDeckHasCards` stops when a pass neither returns a card nor gives a point; test: ExecutorReviewFixTest.BurnOut_RespectsCantGainPoints, ExecutorReviewFixTest.BurnOut_GivesAPointAndAnnouncesIt, ExecutorReviewFixTest.BurnOut_WithNothingToRecycleAndNoPointStops
  - Where: `effect_executor.cpp:445` does `opp_ps.score++` directly; no `cannot_gain_points` check, no `PointsGainedEvent`.

## E. Movement, zones, costs

- [x] **2.22 Effect-driven moves bypass `GameEngine::moveUnit`** [confirmed] — FIXED: `moveToBase` / `moveToBattlefield` increment `moves_this_turn` and move attached gear with the unit; a "move" to the current location is no longer a move (CR 421.1); test: ExecutorReviewFixTest.EffectMove_CarriesAttachedGear, ExecutorReviewFixTest.EffectMove_CountsTowardKaynUnleashed, ExecutorReviewFixTest.EffectMove_ToWhereItAlreadyIsIsNotAMove
  - Where: `effect_executor.cpp:735-771` vs `:6246-6275`. No `moves_this_turn++`, attached gear does not move with the unit (CR 719.3.a).
  - Bard, Mercurial moving an equipped unit leaves the gear at base; Kayn, Unleashed moved twice by effects does not gain immunity.

- [x] **2.23 `resolvePermanent` sets `zone = Base` even when the unit lands on a battlefield** [confirmed] — FIXED: `resolvePermanent` sets `BattlefieldZone` when the location is a battlefield; test: PlayCombatFixTest.PlayUnitToBattlefield_FiresPlayHereTrigger_AndSetsBattlefieldZone
  - Where: `:2100`; `moveUnit` (`:6253-6258`) sets `BattlefieldZone`. The unit is omitted from the play UI's battlefield bucket (`io/play_server.cpp:554`, `:528`) and from `scripted_agent.cpp:28`.

- [x] **2.24 Champion-zone play ignores the "can't play cards" lockout** [confirmed] — FIXED: the champion in its zone is generated by the same loop as units in hand, so it gets the lockout and the same locations (CR 420.1.a, 355.2.a — the rules treat the two sources alike); the alternative-cost play is gated on the lockout too; test: PlayCombatFixTest.ChampionZonePlay_RespectsTheLockoutAndGetsControlledBattlefields
  - Where: `:2745-2787`; every other play loop checks `locked_out` (`:2672`, `:2791`, `:2806`). The champion also gets only `BaseLocation` while hand units get controlled battlefields (`:2714-2729`) [plausible; rules not checked].

- [ ] **2.25 `executeHideCard` pays `[A]` through a private path** [confirmed]
  - Where: `:2499-2513`, gate `:2862-2872`. Ready runes only, pool power ignored, no `power_spent_this_turn`, first hit in map order, recycles a ready rune. A player with all runes exhausted or with floating power is not offered Hide.

- [ ] **2.26 Altar of Blood payment in `killUnit` is hand-rolled and wrong** [confirmed]
  - Where: `:6309-6361`. Requires three ready runes (`:6323-6326`); no `LeftBoardEvent`, no `power_spent_this_turn`. Also an agent decision inside `cleanup()`, contradicting `game_engine.h:423-428`.

- [x] **2.27 `recycle_self` activation cost bypasses leave-board handling** [plausible] — FIXED: the `recycle_self` cost in `payAndQueueAbility` unattaches / sheds gear, clears statuses, banishes a token instead (CR 183.1), puts a card into its OWNER's deck and emits `LeftBoardEvent`; tests: PlayCombatFixTest.RecycleSelfCost_GoesToTheOwnersDeckAndAnnouncesLeavingTheBoard, PlayCombatFixTest.RecycleSelfCost_TokenCeasesToExistInstead
  - Needs card stream: both users of `recycle_self` misuse it. Baited Hook 242 prints `[1][C], [E]` — `[C]` is a Chaos Power, so the cost is `{.exhaust, .energy = 1, .power = 1, .power_domain = Domain::Chaos}`. Poro Snax 369 prints `[1][G], [E], Kill this` — it needs the Calm Power and a kill-self cost, not a recycle.
  - Where: `:1463-1469` (Baited Hook 242, Poro Snax 369). No `LeftBoardEvent`, no detach, no token check, inserts into the activator's deck rather than the owner's.

- [x] **2.28 `killObject` and `dealDamage` have no on-board guard** [plausible] — FIXED: confirmed (a dead token died twice; a card in hand was "killed" into trash while still listed in hand). `dealDamage`, `killObject`, `bounceToHand`, `buffUnit`, `stunUnitBy`, `moveToBase`, `moveToBattlefield` now return unless the object has a location; `readyObject` also accepts a legend or a battlefield; test: ExecutorReviewFixTest.OffBoard_SecondKillOfADeadTokenAnnouncesNothing, ExecutorReviewFixTest.OffBoard_CardInHandIsNotKilledOrDamaged, ExecutorReviewFixTest.OffBoard_DeadUnitIsNotMovedBouncedBuffedOrStunned, ExecutorReviewFixTest.OffBoard_ReadyStillWorksForALegendButNotACardInTrash
  - The second pass found about 20 cards that hit this or the same gap in `moveToBase` / `moveToBattlefield` / `bounceToHand` (`effect_executor.cpp:559,735,754`): see 07 §E. Fixing it in the executor is the cheap route.
  - Where: `effect_executor.cpp:167-170`, `:21-24`. Only check is `zone == Trash`; dead tokens sit in Banishment. A second `killObject` on a dead token re-emits `UnitDiedEvent`; killing an object in hand moves it to trash without removing it from `hand`.

- [x] **2.29 `__was_mighty` only re-armed on board** [plausible] — `:5012-5022`. `killUnit` does not clear it; a returning unit at 5+ might would not emit "became_mighty" (Fiora 500/519). — FIXED: confirmed (nothing cleared `__was_mighty` off the board); the edge scan in `cleanup()` now resets it for units with no location; test: PlayCombatFixTest.BecameMighty_FiresAgainForAUnitThatReturnsToTheBoard

- [x] **2.30 `BattlefieldState&` held across chain and showdown execution** [plausible, narrow] — FIXED: `scoringStep` and `advanceMainPhase` iterate by index and re-fetch by id after a chain / showdown / combat; `runShowdown`, `runCombat` and `combatDamageStep` no longer use a `BattlefieldState&` taken before one (`combatResolutionStep` resolves no chain item, so its reference is safe and unchanged); test: PlayCombatFixTest.MainPhase_CombatThatAddsABattlefield_ClearsTheRightStagedFlag
  - Where: `:3674`, `:3833`, `:3924`, `:4168` (also `:897`, `:795`). `addBattlefieldToken` (`effect_executor.cpp:1261`) does `push_back`. Only Baron Nashor's `onPlay` reaches it; no in-showdown path found. Re-fetch by id or reserve capacity.

- [x] **2.31 Temporary Deflect may never expire** [not confirmed] — `effect_executor.cpp:667`, `deflect_value += value` with no temp counter. — FIXED: confirmed (expiration cleared the keyword bit but `deflectCostFor` still charged the leftover `deflect_value`, which grew every turn). The grant is now kept in turn-stamped `card_counters` (`__temp_deflect`, `__temp_deflect_turn`) and read only by `deflectCostFor`; test: ExecutorReviewFixTest.TemporaryDeflect_ExpiresAndDoesNotAccumulateAcrossTurns

- [ ] **2.32 Resolved spell is trashed to the controller, not the owner** [not confirmed reachable] — `chain_manager.cpp:479`.
