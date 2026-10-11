# 8. Cards — full review (all 955 files)

Every card file under `src/cards/<type>/` was read in full and compared against
its own `ability_text`: each sentence, trigger type, targets, numbers, costs,
optional vs mandatory, durations, keywords and def data, auras, and C++
defects. Nothing was built or run; all findings come from reading code.

**Result: 335 findings — 114 high, about 134 medium, about 87 low.** "High"
means the card's effect is missing or wrong in normal play.

## Where the findings are

Per-card findings live in one file per batch, each a checkbox list with
file:line, the card text, what the code does, and a game scenario.

| Batch | Files | Scope | Findings | High |
|---|---|---|---|---|
| [00](cards/batch-00.md) | 83 | battlefields, gear 17–387 | 31 | 10 |
| [01](cards/batch-01.md) | 90 | gear 396+, legends to 928 | 40 | 20 |
| [02](cards/batch-02.md) | 93 | legends 930+, runes, spells to 363 | 42 | 16 |
| [03](cards/batch-03.md) | 91 | spells 368–933 | 39 | 14 |
| [04](cards/batch-04.md) | 89 | spells 939+, units to 218 | 25 | 8 |
| [05](cards/batch-05.md) | 91 | units 219–494 | 39 | 12 |
| [06](cards/batch-06.md) | 89 | units 495–709 (incl. reprints 537–569) | 46 | 20 |
| [07](cards/batch-07.md) | 88 | units 711–954 (Vendetta, tokens) | 18 | 4 |
| [08](cards/batch-08.md) | 81 | re-read: hand-rolled / finalization cards, part 1 | 18 | 1 |
| [09](cards/batch-09.md) | 81 | re-read, part 2 | 20 | 5 |
| [10](cards/batch-10.md) | 79 | re-read, part 3 | 17 | 4 |
| **Total** | **955** | | **335** | **114** |

Batches 08–10 are the 241 files earlier passes had read only for narrow defect
classes; they list only what is new beyond [03-cards.md](03-cards.md) and
[07-cards-second-pass.md](07-cards-second-pass.md). Batches 02 and 04–06 include
the file ranges in their names, not strict type boundaries.

## A. Root causes — fix these first

About half of the high findings trace to a handful of engine or shared-helper
defects. Fixing the root cause fixes every card listed under it, so work this
section before the per-card lists. Items already in an earlier plan file are
cross-referenced.

- [x] **8.1 Engine text-parsed auras collide with card text** (extends 02 item 2.1) — FIXED: parser deleted; hooks written for Darius 243 / 547, Leona 79, Baron Nashor 709 (not in the list above: "Other friendly units have +2 [M]" was parser-only too) and Draven, Showboat 28; every card whose text matched a parser pattern was checked against its hook; test: see 02 item 2.1
  - The parser at `game_engine.cpp:4500-4795` matches substrings of `ability_text` and applies an aura on top of, or instead of, the card's own.
  - Newly confirmed victims: Leona, Zealot 79 (every unit at her battlefield, herself included, gets -8 [M] min 1), Allay 603 (enemies there get [Deflect]; allies get it twice), Darius, Executioner 243 / 547 and Garen, Commander 311 ("here" trails the clause, so the aura is global), Wizened Elder 65 (+1 twice), Wuju Master 784 (no Level gate), Rage Amplifier 805 (+1 extra), Rumble, Hotheaded 349.
  - Fix: delete the parser; give Darius 243/547 and Leona 79 a card-side `applyPassiveAura` first, since they rely on it.

- [x] **8.2 Permanent `CostModifier` pushed on every aura pass, never removed** — FIXED: `recalculateAuras` tags modifiers pushed by a hook (`CostModifier::from_aura`) and drops them at the top of the next pass; Herald of Scales 140 is one Dragon-only modifier per Herald (`required_tag`), Eager Apprentice 84 and Vex, Cheerless 467 use the continuous spell discount / tax fields (Vex only while she is in combat), Vaults of Helia 775 is `units_only`; Ornn's Forge 527 needed no card change; Risen Altar 950's per-controller slot mix-up (batch-00) is not touched; test: AurasCostsTest.OrnnsForge_OneModifierHoweverManyCleanups, .EffectModifiersSurviveTheAuraPass, .HeraldOfScales_DragonsOnly_NoStacking, .EagerApprentice_SpellsOnly, .VexCheerless_OnlyWhileSheIsInCombat, .VaultsOfHelia_ModifierIsUnitsOnly
  - `recalculateAuras` (`game_engine.cpp:4389-4455`) does not clear `cost_modifiers`; the only erasers are the end-of-turn `this_turn_only` sweep (`game_state.h:295`) and the one-shot `next_*` sweep (`game_engine.cpp:5877`).
  - Ornn's Forge 527 (gear discount stacks without limit, outlives control), Eager Apprentice 84, Herald of Scales 140 (-2 per pass, no Dragon filter), Vex, Cheerless 467 (two modifiers per pass, all card types). After a few cleanups every card that player plays is near free for the rest of the game.
  - Related scope errors: Vaults of Helia 775 taxes spells and gear as well as units; Risen Altar 950.
  - Fix: tag aura-sourced modifiers and clear them at the top of `recalculateAuras`, the same way `aura_effects` are rebuilt.
  - c0: 527 Ornn's Forge, 950 Risen Altar — the per-slot mix-ups are FIXED card-side (each copy acts for its own controller); 775 Vaults of Helia verified; Risen Altar's "or [A] less" stays open (cards/batch-00.md); test: C0Test.SameBattlefieldTwice_ControllerScopedDiscounts

- [x] **8.3 Aura-granted [Assault N] / [Shield N] add no Might** — FIXED: engine fold — `recalculateAuras` adds max(1, `keyword_value`) per granted Assault / Shield while the bearer is an attacker / a defender; `auraKeyword()` no longer sets `might_bonus`; cards that hand-rolled the bonus or also printed the keyword on the def were converted (Captain Farron 15, Raging Soul 19, Ancient Warmonger 452, Rumble, Hotheaded 349, Fiora, Victorious 232); test: AurasCostsTest.EquippedAssaultAndShield_AddMightInCombat, .AuraShield_TaricAndMechanizedMenace, .AuraAssault_RumbleFarronWarmongerRagingSoul, .AuraAssaultShield_PurifierAndBlackFlameAltar, .AuraKeywordHelper_AssaultCountedOnce, .AuraKeywordHelper_ShieldCountedOnce
  - The aura fold (`game_engine.cpp:4843-4850`) sums only `might_bonus` and sets the keyword bit; `recomputeMight` (`game_object.h:325-331`) adds only the object's own `assault_value` / `shield_value`. `auraKeyword()` in `card_helpers.h` exists to add the conditional Might and is not used.
  - No effect at all: Serrated Dirk 332, Cloth Armor 387, Black Flame Altar 764, Purifier 502, Mechanized Menace 551, Rumble, Hotheaded 349 ("Your Mechs each have [Assault]").
  - Fix: fold `keyword_value` in the engine, or route every one through `auraKeyword()`.
  - c0: 332 Serrated Dirk, 387 Cloth Armor, 764 Black Flame Altar — verified ALREADY FIXED by this item; Black Flame Altar now uses `auraKeyword`

- [ ] **8.4 Aura-granted keywords the engine never honours**
  - Vision is read from base keywords only, at play time, before any aura exists (`trigger_manager.cpp:317`): Forecaster 388 does nothing.
  - `Keyword::Weaponmaster` is read nowhere: Emperor of the Sands 512.
  - Vision — FIXED by engine-auras-costs (`shared:` commit in `TriggerManager::onCardPlayed`, possible now that 2.12 sets the location before `CardPlayedEvent`): the handler refreshes the continuous effects and asks `hasKeyword(Keyword::Vision)`; test: AuraVisionTest.Forecaster_MechPlayedLooksAtTheTopCard, .NoVisionSource_TopCardStays.
  - BLOCKED (Weaponmaster): the keyword exists only as the `WeaponmasterUnit` card base class, for cards that print it. A Sand Soldier token (five cards create them: 354, 475, 478, 513, 521 and the legend itself) has no card class, so honouring an aura-granted Weaponmaster needs a keyword-level "when you play me, you may [Equip] ... for [A] less" handler in the trigger path (chain-triggers' file), with a real choice and payment — the base class today attaches the first Equipment for free. Nothing is missing in `recalculateAuras`: the keyword is granted and visible through `hasKeyword()`.

- [x] **8.5 Gear never gets `WhenIDie`** — FIXED: `TriggerManager::onLeftBoard` fires a killed gear's own WhenIDie (with Deathknell doubling); test: ChainTriggerFixTest.GearDeath_TemporaryGearGetsItsDeathknell, ChainTriggerFixTest.GearDeath_KilledGearTriggers. Treasure Trove's "leaves the board" by bounce is still a card gap (batch-00)
  - `UnitDiedEvent` is emitted only for units (`effect_executor.cpp:261`, `game_engine.cpp:6419`); a dying gear emits only `LeftBoardEvent`, which `TriggerManager` does not subscribe to.
  - Treasure Trove 186 does nothing; Scrapheap 182's "killed" branch is dead; Sprite Fountain 640's Deathknell never fires.
  - c0: 186 Treasure Trove — FIXED: new `TriggerType::WhenILeaveTheBoard` covers the bounce (`shared:` 776a84b); 182 Scrapheap also triggers on a discard; test: C0Test.TreasureTrove_TriggersHoweverItLeavesTheBoard, .Scrapheap_DrawsWhenDiscardedAndWhenKilled

- [x] **8.6 Multi-target spell generation is wrong** — FIXED: the engine no longer assumes friendly-plus-enemy. Target sets come from the new `Card::enumerateLegalTargetSets` or, by default, from the requirement: "up to N" (`optional`) = every non-empty set of at most N distinct targets, fixed N = every ordered choice of N distinct targets. Used by hand, trash-replay and hidden plays. Void Assault 758 overrides it to keep its friendly-then-enemy pairs (`shared:` commit); tests: PlayCombatFixTest.MultiTarget_TwoFriendlyTargets_AreOfferedAsOrderedPairs, PlayCombatFixTest.MultiTarget_UpToThree_OffersEverySetOnce, PlayCombatFixTest.MultiTarget_VoidAssault_StillPairsAFriendlyWithAnEnemy
  - Singularity 105: fixed by the default (1 or 2 distinct units of either side).
  - Bellows Breath 402: now gets up to three targets. Needs card stream: override `enumerateLegalTargetSets` to keep only sets at one location ("at the same location").
  - Deathgrip 484, Smoke and Mirrors 645: now offered with two friendly units. Needs card stream: Deathgrip should also be playable with a single friendly unit (kill, no buff, draw) — add the one-target sets in an override; Smoke and Mirrors should list only pairs at different locations.
  - Piercing Light 346: now hits one or two distinct units. Needs card stream: its requirement forces BOTH at a battlefield; override `enumerateLegalTargetSets` with {unit at a battlefield} and {unit at a battlefield, any other unit}, and widen `enumerateLegalTargets` so the second survives the resolve-time filter.
  - The generator (`game_engine.cpp:3369`) turns any `count == 2` requirement into one-friendly-plus-one-enemy pairs, and emits a single target for any other count.
  - Singularity 105 and Piercing Light 346 always hit one of the caster's own units, and are not offered when either side has no target.
  - Deathgrip 484 and Smoke and Mirrors 645 (two friendly targets) are never offered from hand.
  - Bellows Breath 402 (`count = 3`) only ever gets one target.
  - c3: 402 Bellows Breath, 484 Deathgrip, 645 Smoke and Mirrors — FIXED: each overrides `enumerateLegalTargetSets`; test: C3Test.BellowsBreath_TargetSetsAreUnitsAtOneLocation, C3Test.Deathgrip_IsPlayableOnASingleFriendlyUnit, C3Test.SmokeAndMirrors_IsOfferedForUnitsAtDifferentLocationsOnly. Smoke and Mirrors from facedown is still blocked by the hidden generator's all-targets-here filter (CR 811.1.d.4; batch-03)

- [ ] **8.7 `requiresLegion()` and `requiresLevel()` are never read by the engine**
  - Engine side — NOT A DEFECT: the two virtuals are card-wide flags, and no engine gate keyed on them would be right. Legion / Level are per-ability conditions (CR 812.1.b): Darius 243/547 gate only the play trigger, not the aura; Gustwalker 637 and Scorchclaw 578 put [Level] on the passive, not on [Hunt]; Concentrate 653 and Master Yi 621 use Level only for a cost reduction; Hand of Noxus 296 wants "a card", the units "another card". Cards that work gate themselves (16, 20, 217, 243, 547, 637, 660, 713). The engine was left alone.
  - BLOCKED: needs card stream (card files are not this stream's). Hand of Noxus 296 `onActivate`: `if (cards_played_this_turn < 1) return;`. Vanguard Captain 218 `onTrigger` and Sun Disc 21 `onActivate`: `if (ctx.state.player(ctx.controller).cards_played_this_turn < 2) return;`. Scorchclaw 578: remove the `xp < 3` gate from the [Hunt 2] trigger and add the [Level 3] passive (+1 [M] in `applyPassiveAura`, `entersReadyOnPlay` when xp >= 3), as Gustwalker 637 / Bandle Soldier 713 do. Undying Legion 587: needs a play-a-unit-from-trash path (today's `TrashReplayGrant` is spell-only) — larger, see §B.
  - Suggest the helpers stream delete `requiresLegion` / `requiresLevel` / `levelThreshold` once those cards are done: nothing may read them.
  - Declared in `card.h:639`, called nowhere. Cards that rely on them have no gate: Hand of Noxus 296, Vanguard Captain 218, Sun Disc 21, Undying Legion 587, Scorchclaw 578. (Scrapyard Champion 20 and Trifarian Gloryseeker 217 check `cards_played_this_turn` themselves and work.)
  - c0: 21 Sun Disc — FIXED: self-gates in `onActivate` on a card other than itself having been played this turn (CR 812.1.c); its `requiresLegion()` override is removed; test: C0Test.SunDisc_NeedsAnotherCardPlayedThisTurn, .SunDisc_DoesNotCountItselfOnTheTurnItIsPlayed

- [ ] **8.8 Keywords set on the def because the text mentions them**
  - `Keyword::Temporary` on the card itself — the unit is killed at its controller's next Beginning Phase (`game_engine.cpp:761`): Sprite Mother 106, Trevor Snoozebottom 610, Petal Pixie 638, Lillia, Fae Fawn 644, Sprite Queen 646, LeBlanc, Everywhere at Once 652.
  - Other spurious keywords: Ava Achiever 107 (`Hidden` — can be hidden for [A] and played for [0]), Gem Jammer 330 (`Ganking`), Yuumi 618 and Lillia, Protector of Dreams 620 (`Tank`), Carrion Dredger 715 and Ultrasoft Poro 722 (`Deflect`, which only their tokens should have).
  - Missing: Windsinger 459 lacks `Keyword::Hidden`, so it is never offered to hide. Baccai Witherclaw 865 and Noxian Emissary 915 lack `Deathknell` (data only — their triggers work).
  - Looks like one extraction error; each batch file lists its instances.

- [ ] **8.9 Repeat** (extends 02 item 2.4 and 07 item 7.4)
  - Repeat spells that request a choice lose the repeat and leave a stale pending choice: Frigid Touch 389, Upstage Comedy 571, Existential Dread 696, Rocket Barrage 400 — with Called Shot 443, Hard Bargain 457, Marching Orders 436, Temptation 450 and Bonds of Strength 472 that is nine cards. A granted Repeat (The Academy, Syndra) on any choice-requesting spell hits the same gap [plausible].
  - `parseRepeatCost` rejects costs written as `[Repeat] — ...`: Square Up 579 and Curtain Call 743 never repeat.
  - A Repeat cost can be paid up to 32 times; CR 820.1.c.3 allows once.
  - chain-triggers stream: bullet 1 FIXED (02 item 2.4; all nine cards go through the same loop; tests ChainTriggerFixTest.Repeat_*). Bullet 3 FIXED (`executePlaySpell` offers the cost once; test ChainTriggerFixTest.Repeat_CostCanBePaidOnlyOnce).
  - BLOCKED (bullet 2): Square Up's cost is "Discard 1" and Curtain Call has three separate costs; neither fits `parseRepeatCost`'s `[N][D]` shape. Needs a card-declared Repeat cost (e.g. `Card::repeatCosts()` in card.h, read by `executePlaySpell`, with a discard payment that asks the agent) plus the two card files: card.h / game_engine.cpp / cards, outside this stream.
  - c3: 579 Square Up — the forced discard is gone (base effect only); 743 Curtain Call — the mask is per play and the target fits the mode. Both Repeat costs still need the card-declared Repeat cost (not done, sweep)

- [x] **8.10 Unit [Reaction] is not implemented** — FIXED: new `Card::hasReactionTiming()` (the def keyword cannot be used: it is also set on ~25 permanents that only have a [Reaction] ability). `generateReactionPermanentActions` offers such units from hand or the Champion Zone in Closed States and showdowns, to the base or a controlled battlefield (CR 813.1.c.1, 813.3.a); `chain_manager.cpp` clears the Champion Zone on such a play. Shen 241, Janna 376, Rengar 348 and Sumpworks Map 647 opt in (`shared:` commit); tests: PlayCombatFixTest.ReactionUnit_IsOfferedInAClosedState_ToBaseAndControlledBattlefields, PlayCombatFixTest.ReactionUnit_IsOfferedAndPlayedInAShowdown, PlayCombatFixTest.ReactionUnit_PlayedFromTheChampionZoneInResponseToASpell
  - `Keyword::Reaction` is consulted only for spells (`game_engine.cpp:3260-3265`, `chain_manager.cpp:510-511`). Shen, Kinkou 241, Janna, Savior 376 and Rengar, Pouncing 348 cannot be played at Reaction timing (see also 02 item 2.7).

- [x] **8.11 `WhenYouChooseAFriendlyUnit` never fires on the chosen unit** — FIXED: the chosen unit is no longer skipped; the subject is the chosen unit. Irelia 380 and Jae Medarda 463 still need `triggerCondition(subject == source)` (batch-05, card streams); test: ChainTriggerFixTest.ChosenUnit_JaeMedardaDrawsWhenChosenWithASpell, ChainTriggerFixTest.ChosenUnit_IreliaTriggersOnHerOwnChoice
  - `trigger_manager.cpp:1105-1119` skips the chosen object. Jae Medarda 463 never draws; Irelia, Fervent 380 gets +1 only when a different unit is chosen.

- [x] **8.12 Quick-Draw gear does not attach when played in the main phase** — FIXED: `resolvePermanent` attaches [Quick-Draw] gear whenever it is played (CR 819.1.d), its controller choosing the unit as it lands; the Closed-State generator emits one play per gear (the per-unit targets it used to emit were dropped by `ChainManager` and never attached), and showdowns offer it too; tests: PlayCombatFixTest.QuickDraw_PlayedInTheMainPhase_AttachesToAUnitYouChoose, PlayCombatFixTest.QuickDraw_AtReactionTiming_IsOneActionAndAttaches
  - The attach needs a targeted play intent (`game_engine.cpp:2195`), which only `generateClosedStateActions` emits (`:3052-3068`). Long Sword 345, Sterak's Gage 379, Cloth Armor 387.
  - c0: 345 Long Sword, 379 Sterak's Gage, 387 Cloth Armor — verified ALREADY FIXED by this item; no card change

- [x] **8.13 `playIgnoringCost` leaves the card in its source zone** (extends 07 item 7.10) — FIXED: `playIgnoringCost` (and so a successful `playPayingCost`) removes the card from any hand, trash, banishment, Champion Zone or Main Deck before placing it; test: ExecutorReviewFixTest.PlayIgnoringCost_RiftHeraldDeathknellTakesTheUnitOutOfHand, ExecutorReviewFixTest.PlayIgnoringCost_LeavesTrashBanishmentAndDeck
  - Card streams: the manual erase before `playIgnoringCost` is now redundant (harmless). Cards that erase before `playPayingCost` can drop the erase too: they all check `canPlayPayingCost` first, so the only exposure is a payment that fails after that check, which would leave the card in no zone [latent].
  - Rell, Magnetic 347 (Equipment on board and still in hand), Rift Herald 741. Fix in the executor rather than per card.
  - c0: 160 Dazzling Aurora — FIXED: the manual erase from banishment before `playIgnoringCost` is dropped (the unit stays banished while the location is picked); 242 Baited Hook banishes with `banishObject` and plays from there; test: DazzlingAurora tests (unchanged, green), C0Test.BaitedHook_PaysPowerStaysOnBoardAndPlaysThePickedUnit

- [x] **8.14 Off-board source or target** (extends 02 item 2.28, 07 §E) — FIXED: executor guard, see 02 item 2.28; test: ExecutorReviewFixTest.OffBoard_DeadUnitIsNotMovedBouncedBuffedOrStunned
  - Not covered by the guard, still card work (S2): attaching gear to an off-board unit (`attachFree` / hand-rolled attach), and effects the cards apply themselves without the executor. `giveTemporaryMight`, `giveTemporaryKeyword`, `exhaustObject` and `healObject` are left unguarded.
  - `objectExists` is the only guard on about 80 more sites found in this pass (the S2 list below). One on-board guard in the executor's move / bounce / damage / kill / buff / ready functions closes most of them.

- [x] **8.15 Equipped triggers run outside the chain** — FIXED: see 02 item 2.17; test: EquippedTriggerChainTest.YouMayIsARealChoice_Declined, EquippedTriggerChainTest.TheControllerChoosesWhichUnit
  - `onEquippedTrigger` is called directly from `fireEquippedTriggers`, so `chain.resuming` is empty and the choice helpers return defaults: Last Rites 471 always says yes and takes the first unit in trash (see 02 item 2.17).

- [ ] **8.16 Smaller engine gaps, one card each**
  - `xp_gained_this_turn` is never incremented: Wily Newtfish 670.
  - `PlayerState::bonus_damage_dealt` is never set: Rabadon's Deathcrown 508.
  - `may_play_from_trash` / `trash_goes_to_banish` are never read: Endless Riches 809.
  - A spell that banishes itself while resolving is then filed in trash as well: Arcane Shift 515; Time Warp 122 similar.
  - Gear `onPlay` exhaust is overwritten by `resolvePermanent` (`:2183-2186`): Iron Ballista 17 enters ready.
  - A hidden card has no battlefield location, so "hidden here" checks never match: Mushroom Pouch 101.
  - An ability declaring `targets.count = 1` without an `enumerateLegalTargets` overload is never offered: Heart of Dark Ice 375.
  - `untargetable_by_enemy` is ignored by hand-rolled choices: Falling Star 29, Switcheroo 466, Angle Shot 334, Whirlwind 187.
  - Temporary Deflect is permanent and stacks (`effect_executor.cpp:667`): Jayce, Hammer in Hand 875 (confirms 02 item 2.31).
  - Legends are skipped by sweeps that require a location: Battle Mistress 517 (02 item 2.13).
  - `[A][A]` Flow can only be paid from one domain: Death Mark 931.
  - "This turn" delayed death-watches never expire: Deadly Flourish 635, Siphoning Strike 933.
  - c0: 17 Iron Ballista — FIXED (`entersExhausted()`); 101 Mushroom Pouch — FIXED (`hidden_at`); 375 Heart of Dark Ice — FIXED (`enumerateLegalTargets` overload); test: C0Test.IronBallista_EntersExhausted, .MushroomPouch_SeesACardHiddenAtABattlefield, .HeartOfDarkIce_IsOfferedAndGivesTheChosenUnitMight
  - c3: 515 Arcane Shift — FIXED (`banish_on_leave_chain`; C3Test.ArcaneShift_ReplaysTheUnitOnceAndBanishesItself); 466 Switcheroo — FIXED (`chooseableUnits`; C3Test.Switcheroo_DoesNotOfferAUnitThatCannotBeChosenByEnemySpells); 635 Deadly Flourish, 933 Siphoning Strike — FIXED (`expires_on_turn`; C3Test.SiphoningStrike_DeathWatchExpiresWithTheTurn); 931 Death Mark — NOT DONE, engine ([A][A] from two domains)

## B. Cards with a missing or unimplemented effect

Each has a full entry in its batch file. Listed here so the scale is visible;
"documented" means the card's own comment admits the gap.

- **Blank or nearly blank:** Magma Wurm 11 (documented), Nine-Tailed Fox 252 (documented), Alpha Wildclaw 619 (documented), Maduli the Gatekeeper 706 (documented), Atakhan 732 (documented), Temporal Portal 401 (documented), Svellsongur 382, Spiderling 884, Undying Legion 587, Arachnoid Horror 679.
  - c0: 382 Svellsongur — BLOCKED: needs engine dispatch of the bearer's own abilities through the gear (cards/batch-00.md); the card documents it
- **A whole sentence missing:** Last Breath 257 (damage), Detonate 328 (draw 2), Eclipse 625 (Predict), Confront 129, Rally the Troops 487, Renekton, Brute 879 (empower at 10 Might), Ezreal, Prodigy 470, Red Brambleback 591, Blue Sentinel 649, Herald of Spring 596, Ashe, Focused 731, Sivir, Ambitious 441, Yeti Brawler 580, Karma, Channeler 548, Chem-Baroness 556, Swift Scout 259, Crescent Strike 634 (splash), Lillia, Protector of Dreams 620, Noxian Guillotine 251, Imperial Decree 221, Bushwhack 327, Relentless Pursuit 503.
- **Multi-target or "all" effect coded as one target:** Back to Back 206, Grand Strategem 233, Decisive Strike 322, Danger Zone 501, On the Hunt 518, Icathian Rain 247, Tricksy Tentacles 616, Against the Odds 323 (flat +2).
- **Wrong scope or zone:** Anivia, Primal 148 (every battlefield), Tibbers 316 (hits bases), Cannon Barrage 127 (not combat-only), Fox-Fire 253 (kills the caster's own units first), Ride the Wind 173 (base only), Unforgiven 256, Harpoon Squad 458, Gentle Gemdragon 666, Viktor, Leader 246, Right of Conquest 577, Overzealous Fan 449 (bounces instead of moving to base).
- **Missing gate:** Loose Cannon 249 (draws regardless of hand size), Prodigal Explorer 514 (no "chosen twice" gate, wrong timing), Grandmaster at Arms 554 (attaches Equipment still in deck/hand/trash), Baited Hook 242 (cost coded as recycle-self, Power never charged), Altar of Memories 490, Forgotten Signpost 607, Yordle Explorer 422 (never draws), Seal of Strength 542 (wrong domain).
  - c0: 242 Baited Hook — FIXED: cost is `[1]` + one Order Power + exhaust, no recycle-self; test: C0Test.BaitedHook_PaysPowerStaysOnBoardAndPlaysThePickedUnit

## C. Systemic lists

Cards affected by a defect already described elsewhere; one fix per class.
Full per-batch lists, with line numbers, are in each batch file's "Systemic"
section.

- [ ] **S1 Target-less triggers** — nothing new beyond the 26 in 07 item 7.1.
- [ ] **S2 Off-board source/target guarded only by `objectExists`** (see 8.14) — about 80 more cards, most low impact. Higher-impact ones: Baited Hook 242, Icathian Rain 247, Treasure Trove 186, Pit Crew 91 (no guard at all), and gear attached to an off-board unit by Armed Assailant 324, Combat Chef 414, Veteran Poro 421, Master Bingwen 448, Ornn, Forge God 407, Lucian, Merciless 435, Akshan, Mischievous 431.
  - c0: 242 Baited Hook, 186 Treasure Trove (and 272, 60, 143) — FIXED: on-board checks on the victim / source / subject; test: see cards/batch-00.md and cards/batch-08.md
- [ ] **S3 Aura multiplies with copies** (02 item 2.2) — about 45 more cards, including Captain Farron 15, Raging Soul 19, Wielder of Water 55, Wizened Elder 65, Dr. Mundo 109, Lee Sin, Centered 151, Fiora, Victorious 232, Sett, Kingpin 240, Yi, Meditative 302, Garen 311, Gearhead 391, Ornn 407, Ancient Warmonger 452, Sivir, Mercenary 464, Trusty Ramhound 480, Soraka 550, Vilemaw 622, Master Yi, Tempered 675, Crimson Pigeons 716, Brutalizer 365, Soul Sword 601.
  - Battlefields have the same problem when both players bring the same battlefield: Trifarian War Camp 289, Void Gate 291, Kinkou Temple 946, Forbidding Waste 766, Black Flame Altar 764, Ornn's Forge 527, Gardens of Becoming 769, Risen Altar 950.
  - c0: 365 Brutalizer — FIXED (each instance gives its own bearer +2 once); battlefields 289, 291, 946, 766, 764, 527, 769, 950 (and 292) — FIXED card-side with a per-slot guard; test: C0Test.SameBattlefieldTwice_EachCopyAppliesOnce, .SameBattlefieldTwice_VoidGateWasteAltarAndGardens, .SameBattlefieldTwice_ControllerScopedDiscounts, .Brutalizer_BonusAppliesHoweverItWasAttached
- [ ] **S4 Unlimited buff** (02 item 2.14) — Arena Bar 124, Stand United 53, Overt Operation 153, Adaptatron 56, Karma 235, Poro Herder 61, Pit Rookie 136, Trifarian Gloryseeker 217, Warmog's Armor 430, Blind Monk 254, Radiant Dawn 297, Aphelios 372, Red Brambleback 591, Ivern 613, Nami 614, Enthralling Protector 724, Voidreaver 787, Navori Fighting Pit 278, Spirit's Refuge 63.
  - Exempt when fixing: Lee Sin, Ascetic 78 ("I can have any number of buffs").
  - c0: 124 Arena Bar, 278 Navori Fighting Pit, 63 Spirit's Refuge — ALREADY FIXED by 02 item 2.14 (`buffUnit` gives a Buff only to a unit without one); the cards call `buffUnit`
- [ ] **S5 First match taken where the player (or opponent) should choose** — about 70 more cards. Each batch file lists them with what should be chosen.
  - c0: 282 Sigil of the Storm, 276 Hallowed Tomb, 771 Star Spring, 339 Recurve Bow, 99 Garbage Grabber, 342 Assembly Rig, 284 Targon's Peak — FIXED: each asks its controller (cards/batch-00.md Systemic S5)
  - c3: 503, 505, 594, 663, 665, 669, 755 — FIXED (plans/cards/batch-03.md Systemic)
- [ ] **S6 Helper ordering** (03 item 3.1) — nothing new beyond Thrill of the Hunt 745, Altar of Memories 490, Zenith Blade 258, Blast Cone 695; Keeper's Verdict 760 has it on a fallback path; Forgotten Signpost 607 has two `pickTarget` calls.
  - c3: 745 ALREADY FIXED (2e0f181), 760 FIXED (owner prompt; batch-03)
- [ ] **S7 `giveTemporaryKeyword(Temporary)` stripped the same turn** — Turn to Dust 632, with Shadow's Call 727 (07 item 7.9).
  - c3: 632 Turn to Dust, 727 Shadow's Call — FIXED (07 item 7.9)

## D. Corrections to earlier plan files

These have been applied to the files named.

- **07 "Read in full and clean" list** was true only for the narrow classes that pass checked. Under the full checklist these have findings: spells 0187, 0334, 0466 (untargetable filter), battlefield 0274, legend 0259, units 0440, 0470, 0726, 0728. Still clean: 0626, 0642, 0729, 0919.
- **07 item 7.1, "engine route fixes ~22 at once"** was too optimistic. Eight of the cards (76, 130, 132, 188, 191, 199, 238, 361) have target requirements that cannot express the printed restriction ("here", "another", "from a battlefield"), so engine-chosen targets would be wrong. Only 16, 51, 82, 92, 97, 234, 330 are correct as written; the rest need card work either way.
- **07 item 7.1, Yuumi 618 row:** the def also puts [Tank] on Yuumi herself.
- **07 item 7.6:** Legion Quartermaster 367's playability gate (`hasLegalTargets`) is never consulted for a unit play, so it is free with no gear on board, like Cruel Patron.
- **03 item 3.10, Overzealous Fan 449:** the entry treated the bounce as the intended effect; the printed effect is a move to base, so the bounce itself is wrong.
- **README open items:** Baccai Witherclaw 865 and Noxian Emissary 915 Deathknell triggers work (`onUnitDied` dispatches on `cardFiresOn(WhenIDie)`, not the keyword). Turn to Dust 632 is confirmed broken (S7).

## E. Checked and clean across all 955 files

- Timing keywords ([Action] / [Reaction]) match on every spell.
- Counter spells all use peek-and-pop; chain scans appear only in `hasLegalTargets` (520, 750, 826 can offer a counter that then fizzles).
- AoE spells damage first and kill in a second pass (Elder Dragon 680 is the one exception, 07 item 7.36).
- Flow costs match the text on all ten Flow cards; Burn and Empower go through the executor as designed.
- Champion units carry their champion tag, except Allay 603 (none) and Karma 235 (wrong tag). Region tags drift on the four champion reprints.
- No card was left unread.
