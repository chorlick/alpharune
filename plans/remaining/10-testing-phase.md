# 10 — Findings of the testing phase

Open items only. Closed items move to `plans/completed/10-testing-phase.md` with their outcome.

Found after the fix run by two find-and-record streams: `test-decks` (the 18 constructed lists read against
card text and the rules; detail, quoted trace lines and one-command replays in
`docs/testing-findings-decks-2026-10.md`) and `test-fuzz` (random legal decks from the whole pool; detail in
`docs/testing-findings-fuzz-2026-10.md` and `plans/reports/test-fuzz-1.md`).

Confidence is the finding stream's own grading and has not been checked: every item is verified against the
card text and `rules/core-rules.md` before anything is changed. An item ends FIXED with a test shown red
first, or NOT A DEFECT with evidence, a pinning test and a note at the code site, or as a QUESTION written
up for the judges with the current behaviour pinned.

Fixes are batched by root (user decision): one branch, one rebuild and one gate per batch.

## Batch 1 — Triggers and the chain: where abilities are queued, finalized and resolved; cleanup between chain items; the fuzz findings

All 21 items closed: `plans/completed/10-testing-phase.md` (report: `plans/reports/batch-1.md`).

## Batch 2 — Combat timing and continuous effects

- [ ] **D5** Combat is not a Showdown State for the action generator: spells with neither [Action] nor [Reaction] are offered and played at Focus in a combat, by the defender on the opponent's turn too — CERTAIN
- [ ] **D13** Combat damage: the attacker's damage is dealt before the defender has assigned — LIKELY
- [ ] **D23** Trifarian War Camp / Azir, Sovereign (and other continuous effects): Might is not recomputed before the next window or the damage step — a token moved in by Azir's trigger fights without the +1 [M] — CERTAIN
- [ ] **D26** Vex, Cheerless: an enemy spell played while she is in combat is not taxed — CERTAIN
- [ ] **D42** Fiora, Victorious: her [Shield] adds nothing to her Might while a chain of the combat showdown is running — LIKELY
- [ ] **D49** Void Gate: no Bonus Damage for a unit that arrived there earlier on the same chain (Akali, Deadly Weapon's move ability) — LIKELY
- [ ] **D60** Forbidding Waste: the -2 [M] is missing when the combat goes straight to the damage step — CERTAIN
- [ ] **D61** Inferna ([Assault 2]) played with [Ambush] into her side's attack is listed as an attacker without the Assault — LIKELY
- [ ] **D72** Scoring Step: battlefields are held one at a time, each hold's abilities resolved before the next hold — LIKELY
- [ ] **D73** Fortified Position: the [Shield 2] lasts the turn, not the combat — CERTAIN / CODE-ONLY
- [ ] **D76** Wuju Bladesman - Starter: the +2 [M] is missing whenever nothing resolves between "COMBAT:" and the damage step — CERTAIN

## Batch 3 — Targets, finalization and what goes on the chain: spells without targets, choices at resolution, counters, [Add], Equip and Quick-Draw

- [ ] **D2** Daughter of the Void, Scorn of the Moon, Gold, Seal of Discord: an [Add] ability is an ordinary chain item — both players get priority before it resolves, the opponent can respond on top of it, and the resource arrives late — CERTAIN
- [ ] **D3** Ride the Wind, Unforgiven, Charm, Void Assault (and the cards the readers add: Shuriken Flip, Thrill of the Hunt): the destination of a move is chosen as the spell or ability resolves — CERTAIN
- [ ] **D8** Star-Crossed, Switcheroo, Facebreaker, Defiant Dance: offered, paid for and put on the chain without a legal choice for each target — CERTAIN
- [ ] **D14** Long Sword, Sterak's Gage ([Quick-Draw]): the attach is done inline as the gear lands, not as a triggered ability on the chain — CERTAIN
- [ ] **D15** Long Sword, Sterak's Gage ([Equip]): the Equip ability attaches at activation — no chain item, no priority — CERTAIN
- [ ] **D16** Shuriken Flip: the enemy unit, the friendly unit and the move destination are all chosen as it resolves — CERTAIN
- [ ] **D17** Shuriken Flip: playable with no friendly unit to move — LIKELY
- [ ] **D21** Counter spells (Defy, Not So Fast, Hard Bargain, Riposte, Repulse, Rebuttal, Abandon, Wind Wall): the spell to counter is never chosen — only the top item of the chain can be countered — CODE-ONLY / LIKELY / CERTAIN
- [ ] **D22** Emperor of the Sands / Sand Soldier [Weaponmaster]: the "may" and the Equipment are chosen at resolution, not at finalization — CERTAIN
- [ ] **D24** Azir, Sovereign: "you may move any number of your token units" is asked at resolution — LIKELY
- [ ] **D30** Scorn of the Moon cannot pay Diana, Lunari's [1]: the "may" is withheld with the legend ready (outcome of K2) — CERTAIN
- [ ] **D31** Scorn of the Moon activated with Focus passes Focus to the opponent (outcome of K2) — CERTAIN
- [ ] **D32** Fizz, Trickster: a spell whose target is taken from the play intent is replayed with no target (Stupefy, Eclipse, Smoke Screen, Turn to Dust) — CERTAIN
- [ ] **D34** Decree of Insight: its target is chosen after the cost is paid — CERTAIN
- [ ] **D38** Divining Shells: "Kill this" is paid when the ability resolves — CERTAIN
- [ ] **D41** Repulse, Forge of the Future: choices the rules place at finalization are made after RESOLVE (K3 kind) — LIKELY
- [ ] **D54** Tornado Warrior (Evelynn, Entrancing by code): the leading "you may" and the target are decided after RESOLVE — CERTAIN / LIKELY
- [ ] **D57** Void Burrower / Void Rush + Cleave, Smite (Blood Rush): a spell played by an effect that needs a target resolves with none — CERTAIN
- [ ] **D64** Undertitan "As I'm revealed from your deck, [Add] [2]" waits on the chain and arrives after the reveal effect is over (K2) — LIKELY
- [ ] **D67** Swift Scout, Guerilla Warfare: the objects are chosen after RESOLVE; Swift Scout can be activated with nothing to choose — LIKELY
- [ ] **D78** Kennen, Storm of Shuriken: the spell that gets [Flow] is chosen at resolution — LIKELY
- [ ] **D79** Switcheroo on a unit with Deflect (Master Yi, Tempered at Level 6): Deflect is charged for the play option's unit, then both units are asked again — charged without choosing it, or charged twice — CERTAIN
- [ ] **F5** "You may pay [N] to <choose a unit>" triggers (Kennen, Keeper of Balance; Sinister Poro; Icevale Archer): the chosen unit's Deflect is paid first and the [N] is then unpayable — the `canAffordWith` lead of the fix run, in triggered abilities (3 / 6 / 15 per million; once in the 18 deck lists, leblanc vs irelia) — CERTAIN (fuzz)
- [ ] **F6** "Kill a friendly unit" as an additional cost whose death is replaced for a price (Sacrifice with The Boss; Heedless Resurrection with Altar of Blood): the spell is left unpayable (0 / 5 / 9 per million) — CERTAIN (fuzz)
- [ ] **F7** Strike Down 429 is offered with no enemy unit to choose: `hasLegalTargets` checks half of the pair (4 / 3 / 4 per million) — CERTAIN (fuzz)

## Batch 4 — Cards, options offered or withheld, payments, hidden information, the validator

Batch 4 is split so that its card-level part can run beside the engine-flow batches:
- **4a (cards, validator, test tooling; started 2026-10-11 beside batch 1):** V1, D12, D18, D20, D45, D47, D52, D55, D56, D59, D62, D65, D66, D69, D70, D73, D75, D81, F3, F4, F9, A1, A2.
- **4b (generators, payments, log audience, questions; after the engine-flow batches):** D9, D11, D25, D28, D37, D43, D46, D48, D50, D58, D63, D71, D74, D80.

- [ ] **V1** `DeckValidator` does not enforce [Unique] (CR 825.3.a): Forgefire Cape 507, Rabadon's Deathcrown 508, Shurelya's Requiem 509 are accepted three times (same as D12) — CODE-ONLY (fuzz)
- [ ] **D9** Every payment that is not a card's base cost: the engine chooses which runes are exhausted and recycled — LIKELY
- [ ] **D11** Hidden information in log lines whose audience is both players — CERTAIN
- [ ] **D12** Deck validator: no rule for [Unique] — CODE-ONLY
- [ ] **D18** Astral Heron: the discount is kept for a later turn — CERTAIN
- [ ] **D20** Falling Star: kills between its two instructions, so a death replaced after the first hit is died again on the second — CERTAIN
- [ ] **D25** Hidden cards (Guards!, Hidden Blade, Edge of Night, Pyke, Returned, Wages of Pain, Temporal Breach): a facedown card is never offered in its controller's own Neutral Open State — LIKELY / CERTAIN
- [ ] **D28** Deathgrip: playable with a single friendly unit (no "another friendly unit" to choose) — QUESTION
- [ ] **D37** Rengar, Trophy Hunter: the enemy-occupied battlefield is never a play location in a Neutral Open state — LIKELY
- [ ] **D43** Ezreal, Prodigy + Thousand-Tailed Watcher (any Accelerate): Accelerate is not an "optional additional cost" for Ezreal's discount — CERTAIN
- [ ] **D45** Bellows Breath: the Repeat's choices ignore "at the same location", are forced to the first execution's count, and are not asked at all when the first execution chose nothing — CERTAIN
- [ ] **D46** The List: "As you play this, name a tag" is run as a triggered ability with priority windows, and only tags already public can be named — LIKELY / QUESTION
- [ ] **D47** Temporal Breach + Rockfall Path: the banished unit is played back onto a battlefield where "Units can't be played" — LIKELY
- [ ] **D48** Temporal Breach (and every effect that plays a unit "ignoring its cost"): Accelerate is never offered — LIKELY
- [ ] **D50** Daughter of the Void: its "[A]. Use only to play spells" cannot pay the Deflect of a spell — CODE-ONLY / LIKELY
- [ ] **D52** Star Spring: "the first time ... each turn" triggers a second time when the same card is played there again that turn — CERTAIN
- [ ] **D55** LeBlanc, Everywhere at Once: the [Temporary] kill still happens at her battlefield, and the dying unit's other triggered abilities are suppressed instead — CERTAIN
- [ ] **D56** LeBlanc, Fragmented: Awaken Phase is counted as "your Beginning Phase" — CODE-ONLY
- [ ] **D58** Rek'Sai, Breacher + Void Burrower: a unit played from banishment is not offered Accelerate — CERTAIN
- [ ] **D59** Rek'Sai, Breacher: the grant marked in D58 (R7-2) stays on the card; a later play from hand without Rek'Sai on the board is offered Accelerate — CERTAIN
- [ ] **D62** Thrill of the Hunt offers and plays to Rockfall Path — CERTAIN
- [ ] **D63** Rengar, Trophy Hunter in the Champion Zone is never offered as an [Ambush] play — LIKELY
- [ ] **D65** Mel, Newly Awakened: the extra -1 [M] is added to every -[M] her controller's resolving item gives, chosen unit or not — CODE-ONLY
- [ ] **D66** Herald of the Arcane: the Recruit always goes to the base; no location is offered — LIKELY
- [ ] **D69** Sprite Call played from facedown: the Sprite may be put in the base or at another battlefield — CERTAIN
- [ ] **D70** Windsinger played from facedown: a unit at another battlefield is offered — CERTAIN
- [ ] **D71** Nocturne, Horrifying: the "As you look at or reveal me" replacement is a triggered ability on the chain — CERTAIN / QUESTION
- [ ] **D74** Facebreaker from facedown: offered with no friendly + enemy pair at its battlefield (and as up to 20 identical intents) — CERTAIN
- [ ] **D75** Master of Shadows: empowered when an OPPONENT banishes a card its controller owns — CERTAIN
- [ ] **D80** Lonely Poro: "died alone" when every other friendly unit there died in the same damage step — QUESTION
- [ ] **D81** Minefield: no Burn Out when the Main Deck has fewer than 2 cards — CODE-ONLY
- [ ] **F3** Board state written to an object that has left the board (checked build only; 32 / 53 / 95 per 100,000): Green Father's battlefield replacement skips `leftBoard`; Fading Memories, Vi, Hotheaded and Pendulum Blade act on an object that has left (attribution exact for Green Father and Vi, read from one game each for the other two) — CERTAIN (fuzz)
- [ ] **F4** Emperor's Divide 366 re-offers a unit whose move to base is blocked: 1,024 prompts in a row, and the blocked unit is logged as moved (382 per million forced-play games) — CERTAIN (fuzz)
- [ ] **F9** Star Spring 771: its finalize pass ends early at two returns of its own before the marker (1 in 3 million; engine exception) — CERTAIN (fuzz)
- [ ] **A1** `random-player`: `isPassOption` treats the bare value 0 in a prompt of exactly the values 0 and 1 as a pass, so forced play never takes option 0 of a two-mode choice, an X of 0 or 1, "cost [1] less / [A] less" or a Hide cost's two payments. Tell a decline from a first mode by what the prompt is (the helper that asked: `confirmOptional` against `pickMode` / `pickXAmount`), not by its values — tooling (fuzz)
- [ ] **A2** Daisy! 754 is resolved 2-4 times per million fuzz games (its attack trigger needs four tags among your units; the next lowest card is at 552): add a constructed list or a guided seed deck that exercises it, so the stopping rule's per-card minimum can hold for every card — tooling (fuzz)


## Found by batch 1 (2026-10-11, `plans/reports/batch-1.md` sections 5 and 11) — each is in the batch named

- [ ] **B1** (batch 2) Throughput fell 29% with the one-loop chain (783 -> 550 games/s, 1 thread, fiora vs yi_rebuild): `GameEngine::cleanup` runs 268 times a game and 98% of the passes change nothing. Skip a Cleanup when nothing was emitted or logged since the last one that found nothing (an emission counter on `EventBus`), or an equivalent that is provably the same game; fixed-seed games must be identical before and after, and the number is measured — perf
- [ ] **B2** (batch 3) `GameEngine::executePlayFromHidden` never clears `__played_from_facedown`; a card played from facedown, returned and played from hand the same turn can read a stale mark. One helper for the three "from face down" cards and an engine-side clear — CODE-ONLY
- [ ] **B3** (batch 4b) Immortal Phoenix 37: `WhenYouKillAUnitWithASpell` is also dispatched to a Phoenix on the board, which is not "play me from your trash" — CODE-ONLY
- [ ] **B4** (batch 2) Yeti Brawler 580: `PlayerState::excess_combat_damage` / `last_combat_bf` are "most recent combat" values; a second combat in the turn overwrites what an earlier conquer's ability should read — CODE-ONLY
- [ ] **B5** (batch 3) Zed, From the Shadows 810: the token's destination is chosen at resolution (no finalize pass) — CODE-ONLY
- [ ] **B6** (batch 4b) "First time each turn" cards (Wraith of Echoes, Lucian, Merciless, Draven, Audacious) count from their own "performed" counter: one that enters after the event already happened this turn still triggers on the next — CODE-ONLY
- [ ] **B7** (batch 4b) `kMaxDamageOptions` 64 / `kMaxSearchNodes` 20,000 cap the combat-damage assignments offered: on a crowded board legal assignments are withheld — CODE-ONLY
- [ ] **B8** (batch 4b) `CardTestFixture::withEngine` keeps the first `turn_player` silently on a second call in one test: make it throw — tooling
- [ ] **B9** (batch 2) Verify against their cited games whether batch 1 closed D60, D76, D23 (Cleanup with designations before the combat showdown); (batch 4a) D55 first half; (batch 3) D57 and single-target D32 — close with a pinning test each or keep open

## Found by reading 19 ISMCTS games (2026-10-11, `plans/reports/test-games19.md`, detail in `docs/testing-findings-games19-2026-10.md` section 4) — seen on the binary BEFORE batch 1; each is first re-checked on the current tree

- [ ] **G1** (batch 4b) 1v1: the player going first is made to skip the first draw. CR 487.7 / 488.7 give 1v1 only the second player's extra rune; the no-draw sentence is in 489.7-491.7 (multiplayer). The engine cites "CR 482.7" (old numbering); `BurnOutScoringTest.FirstPlayerFirstTurn_DrawPhaseSkipped` pins it — CERTAIN (all 19)
- [ ] **G2** (batch 4b) Time Warp on the opponent's turn: the Additional Turn replaces the owner's regular turn instead of being inserted before it (CR 735, 737) — the spell does nothing — CERTAIN (g11)
- [ ] **G3** (batch 2) The turn player always gets Focus first, not the player who applied Contested / the attacker (CR 345, 466.2.c.1.a) — CERTAIN combat, LIKELY non-combat (g07 g10 g14 g19)
- [ ] **G4** (batch 4b) Setup: the battlefield is an agent decision; a Duel (CR 487.5) picks at random, a Match (488.5) chooses — which mode is an engine game? — QUESTION (to the project) (all)
- [ ] **G5** (batch 1b) A unit with lethal damage is still on the board at the Focus decision after the attack / defend triggers resolved (Forgefire Cape); Charm then chose and moved it — CERTAIN (g01)
- [ ] **G6** (batch 1b) A battlefield with no units keeps its controller through a Main Phase decision when the last unit's Deathknell was queued by the cleanup that killed it (CR 323.5) — CERTAIN (g13)
- [ ] **G7** (batch 2) Sivir, Mercenary: the turn-scoped +2 [M] / [Ganking] is still applied through the next turn's Beginning Step — CERTAIN (g12)
- [ ] **G8** (batch 2) Mechanized Menace "Your Mechs have [Shield]" is missing when a combat goes straight to damage (known root D42 / D60 / D76); it changed a conquer and the result of game 9 — CERTAIN (g09)
- [ ] **G9** (batch 1b) Eight more cards put an ability on the chain with their printed condition false (known root D51): Curator of the Sands (14 empty items in one game), Lady of Luminosity - Starter, Gutter Palace, Patched Porobot, Poppy, Paragon, Pyke, Dockside Butcher, Blitzcrank, Impassive, Vanguard Captain ([Legion]) — CERTAIN (Vanguard Captain LIKELY) (9 games)
- [ ] **G10** (batch 1b) A battlefield's start-of-Beginning-Phase ability is given to the turn player while the other player controls the battlefield (Frozen Fortress, Dusk Rose Lab; CR 190.6.a, 190.6.d) — LIKELY (g05 g17)
- [ ] **G11** (batch 1b) Heart of the Tempest: no trigger for a play from the Champion Zone or a play by an effect from the trash — CERTAIN (g06)
- [ ] **G12** (batch 1b) Heart of the Tempest triggers as the Flow spell goes on the chain, not when it has resolved (CR 420.4.a) — CERTAIN (g06)
- [ ] **G13** (batch 1b) Tornado Warrior "Disempower it at end of turn" is a flag flipped in the Expiration Step, no ability in the Ending Step (Sanction by code) — LIKELY (g07)
- [ ] **G26** (batch 1b) Ravenbloom Student played back by Temporal Breach triggers on that same Temporal Breach — QUESTION (g14)
- [ ] **G14** (batch 3) A chosen target that left the board and came back is still acted on: Amateur Recital moved a unit Temporal Breach had banished and replayed (CR 359.3.e.4 in terms) — CERTAIN (g14)
- [ ] **G15** (batch 3) Activated abilities with a target or mode pay their whole cost before the choice (Heart of the Tempest, Defender of Tomorrow, Matriarch of War, Bounty Hunter; CR 403-405) — CERTAIN (g06 g16 g18)
- [ ] **G16** (batch 3) A play-time-target spell played by Fizz, Trickster is paid for before its target is chosen (Punch First) — CERTAIN (g12)
- [ ] **G17** (batch 3) Emperor's Divide: the units to move are chosen after RESOLVE; the unit named at the play only stands for the battlefield — LIKELY (g10)
- [ ] **G19** (batch 3) Defender of Tomorrow is activated, paid and put on the chain with nothing to choose (known root D67) — CERTAIN (g16)
- [ ] **G20** (batch 3) Public Execution is offered, paid ([5] + 2 Power by Flow) and put on the chain with no legal pair (known root D8 / F7; the gate must test pairs) — CERTAIN (g16)
- [ ] **G18** (batch 4b) Defender of Tomorrow: "Ready a gear" refuses a ready gear (its pick list is narrower than its text) — CERTAIN (g16)
- [ ] **G21** (batch 4b) Kennen, Keeper of Balance played from facedown stuns a unit in the enemy base (known root D70; CR 811.1.d.3) — CERTAIN (g04)
- [ ] **G22** (batch 4b) Hidden units (Tornado Warrior, Tideturner) are offered and played onto Rockfall Path — CERTAIN (g08)
- [ ] **G23** (batch 4b) A Brush that replaced Vilemaw's Lair keeps "Units can't move from here to base" (slot flags survive the replacement; Rockfall Path / Forgotten Monument by code) — LIKELY (g02)
- [ ] **G24** (batch 4b) Green Father's "may" is withheld when the battlefield is already a Brush — LIKELY (g02)
- [ ] **G25** (batch 4b) Astral Heron: with the first card of the turn countered, no card of that turn triggers it — QUESTION (g08)
- [ ] **G27** (batch 3) Sightings that widen known items (report section "Known items seen again"): Gold's "Kill this" paid at resolution (D38), [Add] as a chain item on five more cards (D30/D31), a Flow replay offered at Focus (D5, batch 2), Astral Heron's discount spent on the opponent's turn (D18), Defy takes the top item only (D21), Relentless Pursuit joins D3 / D41 — each named card gets its own test when its item is fixed
- [ ] **G28** (after the batches) ISMCTS at 50 simulations plays near-uniformly (End Turn with ready runes, helpful effects on enemy units, option [0] of two-option prompts favoured, opponent's objects chosen at Targon's Peak): check whether the search tells own from opposing objects in a prompt and whether option order biases it — agents, observation
