[Back to the full card review](../08-cards-full-review.md)

# Batch 07 - units/0711_diana_no_longer_human.cpp .. units/0954_recruit.cpp

Files read in full: 88 of 88.

Engine facts traced for this batch (apply to several cards below):
- **Conditional [Deathknell] dispatch (865, 915) works.** `TriggerManager::onUnitDied` (`src/engine/trigger_manager.cpp:640`) dispatches purely on `cardFiresOn(card_def_id, TriggerType::WhenIDie)`. `Keyword::Deathknell` is not read anywhere in `src/engine` (only `main.cpp`, `types.cpp`, `state_renderer.cpp` for naming/rendering) and no card reads it. Both kill paths call `snapshotLastKnown` (`effect_executor.cpp:173`, `game_engine.cpp:6282`) before the unit leaves, so `last_known.empowered` is valid when `onTrigger` runs, even though `recalculateAuras` zeroes `empowered` off-board (`game_engine.cpp:4403`). Karthus doubling also applies. The missing keyword is a data-only gap (see low finding).
- `[Empower]` abilities resolve engine-side (`game_engine.cpp:2072` -> `EffectExecutor::empower`), alt costs and `kill_friendly_unit` / `discard` costs are paid in `game_engine.cpp:1451-1486`; `activationCostReduction` on the Empower index is honoured. All `forEachEmpoweredSelf` users in the batch are correct.
- `createToken` only exhausts **units** (`effect_executor.cpp:1200`: `is_exhausted = !enter_ready && type == CardType::Unit`), so a gear token made with `enter_ready=false` enters ready.
- `createToken` emits `TokenCreatedEvent` / `EnteredBoardEvent` only, never `CardPlayedEvent`.
- Flags written by batch cards and confirmed read by the engine: `suppress_combat_damage`, `ignore_tank`, `prevent_enemy_spell_ability_damage`, `channel_phase_cap`, `early_score_draws_instead`, `spell_discount_power`, `extra_minus_might`, `flow_cost_reduction`, `immune_to_damage`, `__flow_granted_turn`, `discard_type_filter`, `kill_friendly_unit`, `replaceHostileEffect`, `canBePlayed`, `__ability_used_turn_0`, `chose_enemy_unit_this_turn`, `damage_taken_this_turn`, `unlimited_copies`. `surcharge_enemy_multi_move` is written and reset but never read (see 725).
- Token def 954 Recruit: might 1, tag `Recruit`, no keywords, `SuperType::Token` - matches every inline `createToken(..., "Recruit", 1, {"Recruit"}, ...)` caller. No defect. (It is the only token def in this batch.)

## Findings
- [x] **879 Renekton, Brute** - `units/0879_renekton_brute.cpp:19` [confirmed] high — FIXED (sweep-triggers): `Card::mightThreshold()` + `TriggerType::WhenMyMightReachesThreshold`, judged on every settled state (`StateSettledEvent`); test: SweepTriggersTest.Renekton_IsEmpoweredWhenAnotherEffectTakesHimToTen, C7Test.RenektonBrute_*
  - Text: "When my Might becomes 10 or more, empower me."
  - Code: the class has only the `[1]: +1 [M]` activation and the `[Empowered][>]` aura. There is no trigger, no aura check and no engine hook for the Might threshold (grep for the card / "might becomes" finds nothing outside the def text).
  - Scenario: Renekton (4 [M]) pumps himself six times to 10 [M] -> never becomes Empowered, so never gains [Deflect] / [Ganking]. He prints no [Empower] cost, so his Empowered text is unreachable without another card.
  - c7: PARTLY FIXED: he is empowered at 10+ [M] after his own `[1]` ability, after a spell his controller played resolves, when he is buffed, when an Equipment is attached and when he attacks or defends; test: C7Test.RenektonBrute_BecomesEmpoweredWhenHisAbilityTakesHimToTen, C7Test.RenektonBrute_IsCheckedAfterASpellAndWhenBuffed
  - BLOCKED: a Might change from any other source (another card's aura, an opponent's effect) is only noticed at the next of those points. Needs an engine event for a unit's Might crossing a threshold, like the `became_mighty` edge in `GameEngine::cleanup` (only 5 [M] is announced)
- [x] **884 Spiderling** - `units/0884_spiderling.cpp:15` [confirmed] high — FIXED: `applyPassiveAura` gives each Spiderling +1 [M] per other unit its controller has at its location with its name; test: C7Test.Spiderling_PlusOneForEachOtherSpiderlingHere
  - Text: "I have +1 [M] for each other unit you control here with my name."
  - Code: no `applyPassiveAura` at all; only [Hidden] and `unlimited_copies` are modelled (the class comment lists those two and omits the Might line).
  - Scenario: three Spiderlings at one battlefield are 1 [M] each instead of 3 [M] each; the whole point of the any-number-of-copies deck is lost.
- [x] **732 Atakhan** - `units/0732_atakhan.cpp:19` [documented] high — FIXED (sweep-costs): the kill is chosen before payment and takes the unit's printed Energy and Power off; test: SweepCostsTest.Atakhan_KillsAUnitAndCostsItsPrintedCostLess, SweepCostsTest.Atakhan_TheKillCanBeDeclinedWhenTheFullCostIsPayable
  - Text: "You may kill a friendly unit as an additional cost to play me. If you do, I cost [1] less for each Energy it costs and [Y] less for each Power it costs."
  - Code: not implemented (comment: "ENGINE GAP ... Left unimplemented").
  - Scenario: player with a 6-cost unit on board wants to sacrifice it to play Atakhan for [4][Y][Y][Y]-ish -> no such option; Atakhan always costs the full [10][Y][Y][Y].
  - BLOCKED: needs an engine additional-cost hook that asks for a unit to kill before the cost is paid and feeds its printed cost (CardDB) into the reduction; `Card::OptionalAdditionalCost` has only Energy / Power / discard and `selfCostReduction` cannot choose. (The "defender must kill" half now asks the defender — see S5 below.)
- [x] **731 Ashe, Focused** - `units/0731_ashe_focused.cpp:47` [documented] high — FIXED: the banished card is marked and a delayed ability keyed to that opponent's hold returns it to their hand, whatever has happened to Ashe (`shared:` one-line hook in `TriggerManager::onScore`: delayed abilities with `WhenIHold` fire on their controller's hold); the hand reveal is emitted once; test: C7Test.AsheFocused_TheBanishedCardReturnsWhenTheyHold
  - Text: "Choose a card revealed this way and banish it. When they hold, return it to their hand (even if I'm no longer on the board)."
  - Code: banishes the chosen card and stops; the trace line says "hold-return rider not implemented". No delayed ability is registered.
  - Scenario: opponent holds a battlefield next turn -> the banished card stays banished for the rest of the game (temporary disruption becomes permanent removal).
- [x] **778 Plundering Poro** - `units/0778_plundering_poro.cpp:20` [confirmed] medium — FIXED: `createGoldExhausted`, as reprint 392; test: C7Test.PlunderingPoro_TheGoldEntersExhausted
  - Text: "When I conquer, play a Gold gear token exhausted."
  - Code: calls `createGoldToken(ctx)`, which passes `enter_ready=false` to `createToken`; for `CardType::Gear` that flag is ignored and the token enters ready. The helper that actually exhausts it is `createGoldExhausted` (`card_helpers.h:271`).
  - Scenario: Poro conquers -> the Gold is ready and can be killed/exhausted for Energy in the same turn, instead of being unusable until it readies.
- [x] **715 Carrion Dredger** - `units/0715_carrion_dredger.cpp:43` [confirmed] medium — FIXED: the def no longer carries [Deflect] / `deflect_value` (the Bird token does); test: C7Test.DeflectBelongsToTheBirdTokensOnly
  - Text: "[Deathknell][>] Play a 1 [M] Bird unit token with [Deflect] to your base." (only the token has [Deflect]; the reminder says "a [Deflect] unit", not "me")
  - Code: the def sets `d.deflect_value = 1` and `d.keywords.set(Keyword::Deflect)` on the Dredger itself.
  - Scenario: opponent targets Carrion Dredger with a removal spell -> is charged an extra [A] (or cannot target it at all with no spare Power) that the card does not require.
- [x] **722 Ultrasoft Poro** - `units/0722_ultrasoft_poro.cpp:55` [confirmed] medium — FIXED: the def no longer carries [Deflect] / `deflect_value`; also the two Birds (no location printed) now go where their controller chooses (`playTokensChoosingLocations`); test: C7Test.DeflectBelongsToTheBirdTokensOnly, C7Test.UltrasoftPoro_EachBirdGoesWhereItsControllerChooses
  - Text: "[E]: Play two [1] [M] Bird unit tokens with [Deflect]. ..." (the Poro itself has no [Deflect])
  - Code: the def sets `d.deflect_value = 1` and `Keyword::Deflect` on the Poro.
  - Scenario: opponent must pay [A] to choose the 5 [M] Poro with a spell or ability; the printed card has no such protection.
- [x] **718 Loyal Poro** - `units/0718_loyal_poro.cpp:28` [confirmed] medium — FIXED (sweep-triggers): `LastKnown::other_friendly_units_here`, taken for a whole batch of deaths before the first unit leaves (`beginDeathBatch`); test: SweepTriggersTest.LoyalPoro_UnitsThatDieTogetherWereNotAlone
  - Text: "[Deathknell][>] If I didn't die alone, draw 1. (... I wasn't alone if there were other friendly units here.)"
  - Code: checks, when the trigger resolves from the chain, whether another friendly unit is currently at `last_location`. Units that died in the same combat/AoE have already left, and units that arrived after the death count.
  - Scenario: Loyal Poro and a friendly unit both die in the same combat -> at resolution nobody is there -> no draw, although the Poro did not die alone.
  - BLOCKED: "alone" has to be read from before the deaths. Needs `GameObject::LastKnown` to carry the other friendly units at the location, snapshotted for the whole batch before any unit of a simultaneous kill leaves (`processLethalDamage` kills one by one; `TriggerManager::onUnitDied` also bypasses `triggerCondition`, so the card cannot judge it at event time). Same engine work as the Battle Mistress 517 carry-over row.
- [x] **777 Lonely Poro** - `units/0777_lonely_poro.cpp:32` [confirmed] medium — FIXED (sweep-triggers): as 718; test: SweepTriggersTest.LonelyPoro_UnitsThatDieTogetherDidNotDieAlone, .LonelyPoro_DrawsWhenItDiesAlone
  - Text: "[Deathknell][>] If I died alone, draw 1."
  - Code: same resolve-time scan as Loyal Poro (inverse condition).
  - Scenario: Lonely Poro and another friendly unit die together in combat -> the other unit is already gone at resolution -> draws 1 although it did not die alone. Conversely, it dies alone at base, a token is created there before the trigger resolves -> no draw.
  - BLOCKED: as Loyal Poro 718 above (last-known "other friendly units here" for a batch of deaths).
- [x] **712 Vex, Apathetic** - `units/0712_vex_apathetic.cpp:20` [documented] medium — FIXED: sets `GameObject::cant_move_this_turn` on the played unit: `GameEngine::standardMoveDestinations` offers it no Standard Move, `EffectExecutor::moveToBase` / `moveToBattlefield` refuse a move by its controller's effects, and the Expiration Step clears it; test: SweepFlowTest.Vex_ThePlayedUnitCannotBeMovedThisTurn
  - Text: "[Stun] it. They can't move it this turn."
  - Code: only stuns (TODO comment: can't-move flag not modelled).
  - Scenario: opponent plays a unit with a ready/[Accelerate] entry to base while Vex is at a battlefield -> it is stunned but can still be moved to a battlefield this turn (to contest / conquer an empty one).
- [x] **740 Poppy, Defender of the Meek** - `units/0740_poppy_defender_of_the_meek.cpp:24` [documented] medium — FIXED (sweep-costs): the 3 XP are spent before payment and count toward affordability; test: SweepCostsTest.Poppy_SpendsXpForADiscountChosenBeforePayment
  - Text: "You may spend 3 XP as an additional cost to play me. If you do, I cost [3] less."
  - Code: `selfCostReduction` returns 3 whenever XP >= 3 and the WhenYouPlayMe trigger then deducts 3 XP - no choice. The deduction also runs when Poppy is played by an effect that ignores her cost.
  - Scenario: player with exactly 3 XP and plenty of Energy wants to keep the XP for a [Level 3] effect -> forced to spend it. Poppy put into play free by an effect with 3+ XP -> loses 3 XP for no discount.
  - BLOCKED: `Card::OptionalAdditionalCost` has no XP cost, and its `reduce_energy` is only honoured for the discard variant decided before payment in `executePlayCard`. Needs an XP optional cost with a pre-payment reduction (same code path as the Brazen Buccaneer / Kraken Hunter carry-over row); the card then drops `selfCostReduction` and its trigger.
- [x] **725 Mageseeker Investigator** - `units/0725_mageseeker_investigator.cpp:25` [documented] medium — FIXED: a Standard Move can take several units (CR 144.3): after the first unit `executeStandardMove` asks, one unit at a time, which others join; the card sets `BattlefieldState::multi_move_tax[opponent]` and each unit beyond the first costs that [A], paid as it is added (a unit that cannot be paid for is not offered); test: SweepFlowTest.MageseekerInvestigator_TaxesEachUnitBeyondTheFirst, .StandardMove_SeveralUnitsMoveAsOneAction, .StandardMove_DecliningToAddUnitsMovesOne
  - Text: "Opponents must pay [A] for each unit beyond the first to move multiple units to my battlefield at the same time."
  - Code: sets `BattlefieldState::surcharge_enemy_multi_move`, which nothing reads (only the reset at `game_engine.cpp:4445`).
  - Scenario: opponent moves three units to the Investigator's battlefield in one turn -> pays nothing; the card is a vanilla 4/4.
- [x] **734 LeBlanc, Fragmented** - `units/0734_leblanc_fragmented.cpp:20` [confirmed] low — FIXED: 2 only during her controller's own Awaken / Beginning / Scoring step; test: C7Test.LeBlancFragmented_DrawsTwoOnlyInHerControllersBeginningPhase
  - Text: "[Deathknell][>] Draw 1. If it's your Beginning Phase, draw 2 instead."
  - Code: `phase == TurnPhase::BeginningStep` only - no turn-player check, and Awaken / Scoring steps (which the engine itself treats as the Beginning Phase, `effect_executor.cpp:255`) are excluded.
  - Scenario: LeBlanc dies to the opponent's start-of-Beginning trigger on the opponent's turn -> draws 2 (should be 1). She dies during her controller's Scoring step (hold trigger) -> draws 1 (should be 2).
- [x] **722 Ultrasoft Poro** - `units/0722_ultrasoft_poro.cpp:21` [confirmed] low — FIXED: `canActivateAbilityAt` checks the activating Poro; test: C7Test.UltrasoftPoro_OnlyThePoroAtABattlefieldCanUseItsAbility
  - Text: "Use this ability only while I'm at a battlefield."
  - Code: `canActivateAbility` returns true if ANY Ultrasoft Poro the player controls is at a battlefield; it is not per-instance (`canActivateAbilityAt` is not overridden).
  - Scenario: one Poro at a battlefield, a second ready in base -> the base Poro may exhaust to make two Birds.
- [x] **831 Astral Heron** - `units/0831_astral_heron.cpp:29` [plausible] low — FIXED: confirmed; the Heron counts the plays announced to it each turn, so a first spell answered by its controller's own Reaction is still recognised when it resolves, and a Heron that was in base for the first card gives nothing later. Known limit, documented in the card: a response that was countered is never announced, so the spell under it is not recognised; test: C7Test.AstralHeron_DiscountsOnceForTheFirstCardOfTheTurn, C7Test.AstralHeron_AFirstSpellAnsweredByYourOwnReactionStillCounts, C7Test.AstralHeron_InBaseForTheFirstCardGivesNoDiscountLater
  - Text: "When you play your first card each turn, if I'm at a battlefield, your next card costs [2][A][A] less."
  - Code: "first card" is tested at resolution as `cards_played_this_turn == 1`. Spell plays fire this trigger on resolution (`onSpellResolved`), by which time the counter may have moved on.
  - Scenario: first card of the turn is a spell; the player responds to it with their own [Reaction] spell before it resolves -> counter is 2 when either trigger resolves -> no discount is ever created.
- [x] **865 Baccai Witherclaw / 915 Noxian Emissary** - `units/0865_baccai_witherclaw.cpp:59`, `units/0915_noxian_emissary.cpp:55` [confirmed] low — FIXED: both defs set `Keyword::Deathknell` (Karthus now repeats only objects that carry it); test: C7Test.EmpoweredDeathknellCardsCarryTheDeathknellKeyword, C7Test.Karthus_RepeatsAnEmpoweredDeathknell
  - Text: "[Empowered][>][>>][Deathknell][>] ..."
  - Code: `d.keywords` sets only `Keyword::Empower`, not `Keyword::Deathknell`. The death trigger still dispatches and works (see engine facts above); the only effect is the missing keyword in rendered/dumped card data and in `last_known.keywords`.
  - Scenario: none in play today; a future "unit with [Deathknell]" check or the registry dump would not see these two.
- [x] **908 Reluctant Leader / 712 Vex, Apathetic** - `units/0908_reluctant_leader.cpp:22`, `units/0712_vex_apathetic.cpp:22` [plausible] low — FIXED (sweep-triggers): a token is played (`TriggerManager::onTokenCreated` -> `announceUnitPlayed`); test: SweepTriggersTest.TokenUnit_IsAUnitBeingPlayed, .TokenUnit_IsAUnitTheOpponentPlayed
  - Text: "When you play another unit ..." / "When an opponent plays a unit ..."
  - Code: both rely on `CardPlayedEvent`; `createToken` never emits it, so unit tokens that card text says are "played" do not trigger either card.
  - Scenario: Noxian Emissary's Deathknell plays two Recruits with Reluctant Leader on board -> no +2 [M]; opponent plays tokens while Vex is at a battlefield -> not stunned.
  - BLOCKED: confirmed — a token is played (CR 350.2) but `EffectExecutor::createToken` emits only `TokenCreatedEvent` / `EnteredBoardEvent`. Needs the engine to announce a token unit as played ("When you play a unit" / "When an opponent plays a unit" dispatch); no card change.
- [x] **754 Daisy!** - `units/0754_daisy.cpp:42` [plausible] low — FIXED: `stunUnitBy`, on the enemy unit here her controller chooses (`pickTarget` at finalization); "while your units have all 4 tags" is the `triggerCondition`; test: C7Test.Daisy_NeedsAllFourTagsAndStunsTheChosenEnemyUnitHere
  - Text: "[Stun] an enemy unit here."
  - Code: uses `stunUnit` rather than `stunUnitBy(oid, ctx.source)`, so the stun is not attributed to Daisy's controller for WhenYouStun triggers (see also S5).
  - Scenario: Daisy stuns with a "when you stun an enemy unit" card on board -> that card does not trigger.

## Systemic
- S2: 711 Diana, No Longer Human (self +2 [M] with no on-board check), 712 Vex, Apathetic (remembered `__opp_played_unit_id` victim guarded only by objectExists), 724 Enthralling Protector (buffs ctx.source guarded only by objectExists)
  - c7: 711 Diana — FIXED (on-board check; C7Test.Diana_GetsTheMightOnlyWhileOnTheBoard); 712 Vex — FIXED (the remembered unit must be an enemy unit on the board; C7Test.VexApathetic_StunsTheUnitAnOpponentPlays); 724 Enthralling Protector — ALREADY FIXED by 02 item 2.28 (`buffUnit` does nothing off the board)
- S3: 716 Crimson Pigeons (loops every Pigeons instance per call; two attacking together get +4 each)
  - c7: 716 Crimson Pigeons — ALREADY FIXED by the 2.2 aura contract (one call per card and controller); test: C7Test.TwoCrimsonPigeonsAttackingTogether_EachGetPlusTwoOnce
- S4: 724 Enthralling Protector ("[Buff] me" calls buffUnit unconditionally; repeatable with XP)
  - c7: 724 Enthralling Protector — ALREADY FIXED by 02 item 2.14 (`buffUnit`); test: C7Test.EnthrallingProtector_ASecondActivationAddsNoSecondBuff
- S5: 754 Daisy! (first non-stunned enemy unit here in map order), 732 Atakhan (defender's choice auto-picked as lowest Might; documented), 779 Veteran Poro (WeaponmasterUnit base takes the first Equipment, free), 847 Sky Crusier (engine discards the first gear in hand for "Discard a gear")
  - c7: 754 Daisy! — FIXED (chosen); 732 Atakhan — FIXED: the defender chooses (`selectObjectAs`; C7Test.Atakhan_TheDefenderChoosesWhichOfTheirUnitsHereDies); 779 Veteran Poro — choice ALREADY FIXED in `weaponmaster_base.h` (helpers), the free Equip is the 7.7 engine gap; 847 Sky Crusier — BLOCKED: the typed discard cost is paid by `payAndQueueAbility` ("the first matching cards in hand"); the engine has to ask which gear

## Not read
None
