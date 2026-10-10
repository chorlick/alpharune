[Back to the full card review](../08-cards-full-review.md)

# Batch 04 - spells/0939_rebuttal.cpp .. units/0218_vanguard_captain.cpp

Files read in full: 89 of 89. (For 71 of them the uncheckable metadata lines - includes, def_id, set/collector codes, artist, rarity, image URL - were filtered out of the view; every line of class code, comments, and gameplay def data was read.)

## Findings
- [ ] **84 Eager Apprentice** - `units/0084_eager_apprentice.cpp:30-38` [confirmed] high
  - Text: "While I'm at a battlefield, the Energy costs for spells you play is reduced by [1], to a minimum of [1]."
  - Code: `applyPassiveAura` pushes a `CostModifier{energy_reduction=1, min_cost=1, this_turn_only=false}` onto `cost_modifiers` on every aura pass. Nothing ever removes a non-`this_turn_only`, non-`next_*` modifier (the only erases are `game_state.h:295` for `this_turn_only` and `game_engine.cpp:5877` for `next_*`; `recalculateAuras` does not reset the list). The modifier also has no spell filter (documented in the file), and one is pushed per on-board copy even if that copy is in base.
  - Scenario: Apprentice sits at a battlefield for one turn; every cleanup adds another -1. After a handful of cleanups every card the controller plays (units and gear too) costs [1] Energy, and it stays that way for the rest of the game after the Apprentice dies.
- [ ] **140 Herald of Scales** - `units/0140_herald_of_scales.cpp:29-37` [confirmed] high
  - Text: "Your Dragons' Energy costs are reduced by [2], to a minimum of [1]."
  - Code: same never-removed `CostModifier` pushed every aura pass (-2 each time), and with no tag filter (documented in the file) it applies to every card, not only Dragons.
  - Scenario: Herald in play for a turn -> all of the controller's cards cost [1] Energy permanently, including after Herald leaves the board.
- [ ] **106 Sprite Mother** - `units/0106_sprite_mother.cpp:43` [confirmed] high
  - Text: "When you play me, play a ready 3 [M] Sprite unit token with [Temporary] here."
  - Code: the def sets `Keyword::Temporary` on Sprite Mother herself; `GameEngine::beginningStep` (`game_engine.cpp:761`) kills every on-board permanent of the turn player whose `obj.keywords` has Temporary.
  - Scenario: play Sprite Mother; at the start of your next Beginning Phase she is killed along with her token.
- [ ] **218 Vanguard Captain** - `units/0218_vanguard_captain.cpp:18-28` [confirmed] high
  - Text: "[Legion] — When you play me, play two 1 [M] Recruit unit tokens here. (Get the effect if you've played another card this turn.)"
  - Code: relies on `requiresLegion()`, which no engine code reads (only declared in `card.h:639`); `onTrigger` has no `cards_played_this_turn` check (unlike 20 Scrapyard Champion and 217 Trifarian Gloryseeker, which check it themselves).
  - Scenario: Vanguard Captain played as the first card of the turn still makes two Recruits.
- [ ] **148 Anivia, Primal** - `units/0148_anivia_primal.cpp:21-27` [confirmed] high
  - Text: "When I attack, deal 3 to all enemy units here."
  - Code: collects every enemy unit with `isAtBattlefield()`; never compares against Anivia's own battlefield.
  - Scenario: Anivia attacks battlefield A; enemy units holding battlefield B also take 3 and die.
- [ ] **79 Leona, Zealot** - `units/0079_leona_zealot.cpp:17-18` (aura left to `game_engine.cpp:4609-4617`) [confirmed] high
  - Text: "Stunned enemy units here have -8 [M], to a minimum of 1 [M]."
  - Code: the card defers to the engine's text parser. Her text also contains the substring "units here have", so the unconditional generic pattern at `game_engine.cpp:4610-4611` (`here_only`, no friendly/enemy/stunned filter, not `other_only`) registers a second -8/min-1 aura in addition to the stunned-enemy one.
  - Scenario: Leona at a battlefield -> every unit there, including Leona herself, her allies and unstunned enemies, drops to max(1, Might-8); Leona fights at 1 Might.
- [ ] **65 Wizened Elder** - `units/0065_wizened_elder.cpp:20-30` [confirmed] high
  - Text: "While I'm buffed, I have an additional +1 [M]."
  - Code: `applyPassiveAura` adds a +1 aura, and the engine's text parser (`game_engine.cpp:4725-4737`, "while i'm buffed" + "additional +1 [m]") adds another +1 for the same text.
  - Scenario: a buffed Wizened Elder is 4 + 1 (buff) + 2 = 7 Might instead of 6.
- [ ] **11 Magma Wurm** - `units/0011_magma_wurm.cpp:17-21` [documented] high
  - Text: "Other friendly units enter ready."
  - Code: no implementation (the file notes the engine gap; no engine hook exists).
  - Scenario: units played while Magma Wurm is on board still enter exhausted; the card is a vanilla 8-Might unit.
- [ ] **2 Brazen Buccaneer** - `units/0002_brazen_buccaneer.cpp:22-28` [confirmed] medium
  - Text: "As you play me, you may discard 1 as an additional cost. If you do, reduce my cost by [2]."
  - Code: the discount is only staged inside `executePlayCard` (`game_engine.cpp:1606-1637`); the play generator gates on `canAfford` (`game_engine.cpp:2676`), which uses the full printed cost.
  - Scenario: with 4 or 5 Energy available and a card to discard, the Buccaneer is never offered as a play; the discount only works when the player could already pay 6.
- [ ] **74 Taric, Protector** - `units/0074_taric_protector.cpp:28-32` [confirmed] medium
  - Text: "Other friendly units here have [Shield]."
  - Code: the aura sets `keyword = Shield, keyword_value = 1` but no `might_bonus`. Aura keywords only feed `aura_keywords`; `recomputeMight` adds `shield_value` only (`game_object.h:329-331`), which auras do not change. Compare Captain Farron, which sets `might_bonus` while attacking, and the `auraKeyword` helper, which does it for Shield while defending.
  - Scenario: an ally defending beside Taric gets no +1 Might from the granted Shield.
- [ ] **110 Ekko, Recurrent** - `units/0110_ekko_recurrent.cpp:19-33` [confirmed] medium
  - Text: "[Deathknell] — Recycle me to ready your runes."
  - Code: readies all runes but never recycles Ekko; he stays in the trash, and the effect is not gated on being able to recycle him.
  - Scenario: Ekko dies -> runes ready and Ekko remains in trash (available to trash-recursion, counted by Dr. Mundo / Rhasa) instead of going to the bottom of the deck; a banished or token-copy Ekko still readies runes.
- [ ] **100 Gemcraft Seer** - `units/0100_gemcraft_seer.cpp:20-25` [documented] medium
  - Text: "Other friendly units have [Vision]."
  - Code: grants Vision as an aura keyword, but the on-play Vision peek reads `obj.keywords.has(Keyword::Vision)` (`trigger_manager.cpp:317`), not aura keywords.
  - Scenario: a unit played while Gemcraft Seer is on board never gets its Vision look; the second sentence has no effect.
- [ ] **111 Heimerdinger, Inventor** - `units/0111_heimerdinger_inventor.cpp:35-43` [confirmed, index-0 limit documented] medium
  - Text: "I have all [E] abilities of all friendly legends, units, and gear."
  - Code: grants ability index 0 of every friendly card regardless of whether its cost includes exhaust, and never index 1+. The granted-ability generator (`game_engine.cpp:3597-3615`) checks neither `cost.exhaust` being present nor the granter's `canActivateAbility`.
  - Scenario: with Sett, Brawler on board a buffed Heimerdinger can use "Spend my buff: +4 [M]" (also Vi's "Recycle 1 from your trash", Udyr's modes) though none is an [E] ability; a multi-ability card's second [E] ability is not granted.
- [ ] **157 Udyr, Wildman** - `units/0157_udyr_wildman.cpp:110` [confirmed] medium
  - Text: "... Give me [Ganking] this turn." (the only Ganking on the card is the fourth mode)
  - Code: the def sets `Keyword::Ganking` as a printed keyword.
  - Scenario: Udyr can always move battlefield to battlefield without spending a buff; mode 4 is meaningless.
- [ ] **15 Captain Farron** - `units/0015_captain_farron.cpp:62-63` [confirmed] medium
  - Text: "Other friendly units here have [Assault]."
  - Code: the def gives Farron himself `Keyword::Assault` with `assault_value = 1`.
  - Scenario: Farron attacks at 6 Might instead of 5.
- [ ] **189 Kayn, Unleashed** - `units/0189_kayn_unleashed.cpp:27` [confirmed] medium
  - Text: "If I have moved twice this turn, I don't take damage."
  - Code: reads `moves_this_turn`, which is incremented only in `GameEngine::moveUnit` (`game_engine.cpp:6251`). `EffectExecutor::moveToBattlefield` / `moveToBase` (`effect_executor.cpp:735-770`) set the location directly and do not count.
  - Scenario: Kayn makes one standard move and is moved once by a spell or ability -> not immune.
- [ ] **150 Kraken Hunter** - `units/0150_kraken_hunter.cpp:18-25` [documented] medium
  - Text: "As you play me, you may spend any number of buffs as an additional cost. Reduce my cost by [O] for each buff you spend."
  - Code: not implemented.
  - Scenario: buffs can never be spent; the full [O][O] is always charged.
- [ ] **18 Noxus Saboteur** - `units/0018_noxus_saboteur.cpp:32,52` [confirmed data error; inert flag documented] medium
  - Text: "Your opponents' [Hidden] cards can't be revealed here."
  - Code: (a) the def sets `Keyword::Hidden`, so the hide generator (`game_engine.cpp:2860`) lets the Saboteur itself be hidden face down; (b) `opp_hidden_unrevealable` is only reset (`game_engine.cpp:4446`), never read.
  - Scenario: the opponent plays a card from Hidden at the Saboteur's battlefield unhindered, and the Saboteur can be hidden for [A].
- [ ] **167 Ember Monk** - `units/0167_ember_monk.cpp:40` [confirmed] low
  - Text: "When you play a card from [Hidden], give me +2 [M] this turn." (no Hidden keyword of its own)
  - Code: the def sets `Keyword::Hidden`.
  - Scenario: Ember Monk can be hidden face down at a battlefield and played later as a Reaction.
- [ ] **68 Caitlyn, Patrolling** - `units/0068_caitlyn_patrolling.cpp:33-41,63-71` [confirmed] low
  - Text: "Use this ability only while I'm at a battlefield."
  - Code: `canActivateAbility` is true if any friendly Caitlyn is at a battlefield; `onActivate` does not re-check the source's location.
  - Scenario: two copies (or a token copy), one at a battlefield and one in base -> the one in base can exhaust to deal its Might.
- [ ] **157 Udyr, Wildman / 164 Sett, Brawler** - `units/0157_udyr_wildman.cpp:23-31`, `units/0164_sett_brawler.cpp:32-38` [confirmed] low
  - Text: "Spend my buff: ..."
  - Code: `canActivateAbility` passes if any friendly copy has a buff, and the buff is spent at resolution, not as an activation cost.
  - Scenario: with two copies, the unbuffed one is offered the ability and the activation fizzles in `onActivate`.
- [ ] **157 Udyr, Wildman** - `units/0157_udyr_wildman.cpp:61-66` [confirmed] low
  - Text: "Deal 2 to a unit at a battlefield. / Stun a unit at a battlefield."
  - Code: `bf_units()` does not filter `untargetable_by_enemy`.
  - Scenario: Udyr can deal 2 to or stun an enemy unit that can't be chosen by enemy abilities.
- [ ] **27 Darius, Trifarian** - `units/0027_darius_trifarian.cpp:26` [plausible] low
  - Text: "When you play your second card in a turn, give me +2 [M] this turn and ready me."
  - Code: the "second card" test (`cards_played_this_turn != 2`) runs when the trigger resolves, not when it fires.
  - Scenario: play the second card, then a third in response before the trigger resolves -> the count is 3 and Darius gets nothing.
- [ ] **79 Leona, Zealot** - `units/0079_leona_zealot.cpp:21` [confirmed] low
  - Text: "If an opponent's score is within 3 points of the Victory Score, I enter ready."
  - Code: uses `state.mode.victory_score`, ignoring victory-score modifiers (`Card::victoryScoreModifier` / `EffectExecutor::effectiveVictoryScore`).
  - Scenario: with a +1 "points needed to win" battlefield in play and the opponent at 5, Leona enters ready though they are 4 away.
- [ ] **939 Rebuttal** - `spells/0939_rebuttal.cpp:24-28` [confirmed] low
  - Text: "Choose a spell with Energy cost no more than [4]."
  - Code: `hasLegalTargets` accepts any spell anywhere on the chain; `onResolve` needs the top item to be a spell costing 4 or less.
  - Scenario: Rebuttal is offered against a 5-cost spell, or when an ability sits above the spell, and resolves doing nothing.

## Systemic
- S2: 27 Darius Trifarian, 36 Vi Destructive, 59 Eclipse Herald, 61 Poro Herder, 78 Lee Sin Ascetic, 91 Pit Crew (no guard at all), 103 Ravenbloom Student, 131 Dune Drake, 139 Cithria of Cloudfield, 157 Udyr Wildman, 164 Sett Brawler, 167 Ember Monk, 197 Teemo Scout, 202 Jinx Rebel, 217 Trifarian Gloryseeker
- S3: 15 Captain Farron, 19 Raging Soul, 55 Wielder of Water, 65 Wizened Elder, 109 Dr. Mundo Expert, 151 Lee Sin Centered, 111 Heimerdinger Inventor (duplicate granted refs); keyword-only duplicates with no numeric effect: 68 Caitlyn Patrolling, 74 Taric Protector, 125 Bilgewater Bully
- S4: 61 Poro Herder, 136 Pit Rookie, 217 Trifarian Gloryseeker (78 Lee Sin Ascetic calls buffUnit unguarded on purpose: "I can have any number of buffs" - keep it exempt when S4 is fixed)
- S5: 136 Pit Rookie (which unit to buff), 36 Vi Destructive (which trash card to recycle), 109 Dr. Mundo Expert (which 3 trash cards to recycle)

## Not read
None
