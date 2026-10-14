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

All items closed: `plans/completed/10-testing-phase.md` (report: `plans/reports/batch-2.md`).

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
- **4a (cards, validator, test tooling; started 2026-10-11 beside batch 1):** all 23 items closed (V1, D12, D18, D20, D45, D47, D52, D55, D56, D59, D62, D65, D66, D69, D70, D73, D75, D81, F3, F4, F9, A1, A2): `plans/completed/10-testing-phase.md` (report: `plans/reports/batch-4a.md`). D73 was also listed under batch 2; it is closed here.
- **4b (generators, payments, log audience, questions; after the engine-flow batches):** D9, D11, D25, D28, D37, D43, D46, D48, D50, D58, D63, D71, D74, D80.

- [ ] **D9** Every payment that is not a card's base cost: the engine chooses which runes are exhausted and recycled — LIKELY
- [ ] **D11** Hidden information in log lines whose audience is both players — CERTAIN
- [ ] **D25** Hidden cards (Guards!, Hidden Blade, Edge of Night, Pyke, Returned, Wages of Pain, Temporal Breach): a facedown card is never offered in its controller's own Neutral Open State — LIKELY / CERTAIN
- [ ] **D28** Deathgrip: playable with a single friendly unit (no "another friendly unit" to choose) — QUESTION
- [ ] **D37** Rengar, Trophy Hunter: the enemy-occupied battlefield is never a play location in a Neutral Open state — LIKELY
- [ ] **D43** Ezreal, Prodigy + Thousand-Tailed Watcher (any Accelerate): Accelerate is not an "optional additional cost" for Ezreal's discount — CERTAIN
- [ ] **D46** The List: "As you play this, name a tag" is run as a triggered ability with priority windows, and only tags already public can be named — LIKELY / QUESTION
- [ ] **D48** Temporal Breach (and every effect that plays a unit "ignoring its cost"): Accelerate is never offered — LIKELY
- [ ] **D50** Daughter of the Void: its "[A]. Use only to play spells" cannot pay the Deflect of a spell — CODE-ONLY / LIKELY
- [ ] **D58** Rek'Sai, Breacher + Void Burrower: a unit played from banishment is not offered Accelerate — CERTAIN
- [ ] **D63** Rengar, Trophy Hunter in the Champion Zone is never offered as an [Ambush] play — LIKELY
- [ ] **D71** Nocturne, Horrifying: the "As you look at or reveal me" replacement is a triggered ability on the chain — CERTAIN / QUESTION
- [ ] **D74** Facebreaker from facedown: offered with no friendly + enemy pair at its battlefield (and as up to 20 identical intents) — CERTAIN
- [ ] **D80** Lonely Poro: "died alone" when every other friendly unit there died in the same damage step — QUESTION


## Found by batch 1 (2026-10-11, `plans/reports/batch-1.md` sections 5 and 11) — each is in the batch named

- [ ] **B2** (batch 3) `GameEngine::executePlayFromHidden` never clears `__played_from_facedown`; a card played from facedown, returned and played from hand the same turn can read a stale mark. One helper for the three "from face down" cards and an engine-side clear — CODE-ONLY
- [ ] **B3** (batch 4b) Immortal Phoenix 37: `WhenYouKillAUnitWithASpell` is also dispatched to a Phoenix on the board, which is not "play me from your trash" — CODE-ONLY
- [ ] **B5** (batch 3) Zed, From the Shadows 810: the token's destination is chosen at resolution (no finalize pass) — CODE-ONLY
- [ ] **B6** (batch 4b) "First time each turn" cards (Wraith of Echoes, Lucian, Merciless, Draven, Audacious) count from their own "performed" counter: one that enters after the event already happened this turn still triggers on the next — CODE-ONLY
- [ ] **B7** (batch 4b) `kMaxDamageOptions` 64 / `kMaxSearchNodes` 20,000 cap the combat-damage assignments offered: on a crowded board legal assignments are withheld — CODE-ONLY
- [ ] **B8** (batch 4b) `CardTestFixture::withEngine` keeps the first `turn_player` silently on a second call in one test: make it throw — tooling
- [ ] **B9** (batch 2, done: D60, D76, D23 closed by batch 1, pinned by `Batch2Combat.D60_*`, `.D76_*`, `.D23_*`) Verify against their cited games whether batch 1 closed D60, D76, D23; (batch 4a, done: pinned by `Batch4aTest.D55_LeBlanc_TheTemporaryKillDoesNotHappenAtHerBattlefield`) D55 first half; (batch 3) D57 and single-target D32 — close with a pinning test each or keep open

## Found by reading 19 ISMCTS games (2026-10-11, `plans/reports/test-games19.md`, detail in `docs/testing-findings-games19-2026-10.md` section 4) — seen on the binary BEFORE batch 1; each is first re-checked on the current tree

- [ ] **G1** (batch 4b) 1v1: the player going first is made to skip the first draw. CR 487.7 / 488.7 give 1v1 only the second player's extra rune; the no-draw sentence is in 489.7-491.7 (multiplayer). The engine cites "CR 482.7" (old numbering); `BurnOutScoringTest.FirstPlayerFirstTurn_DrawPhaseSkipped` pins it — CERTAIN (all 19)
- [ ] **G2** (batch 4b) Time Warp on the opponent's turn: the Additional Turn replaces the owner's regular turn instead of being inserted before it (CR 735, 737) — the spell does nothing — CERTAIN (g11)
- [ ] **G4** (batch 4b) Setup: the battlefield is an agent decision; a Duel (CR 487.5) picks at random, a Match (488.5) chooses — which mode is an engine game? — QUESTION (to the project) (all)
- [ ] **G5** (batch 1b) A unit with lethal damage is still on the board at the Focus decision after the attack / defend triggers resolved (Forgefire Cape); Charm then chose and moved it — CERTAIN (g01)
- [ ] **G6** (batch 1b) A battlefield with no units keeps its controller through a Main Phase decision when the last unit's Deathknell was queued by the cleanup that killed it (CR 323.5) — CERTAIN (g13)
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

## Found by batch 4a (2026-10-14, `plans/reports/batch-4a.md` sections 8 and 9)

- [ ] **H1** (batch 4b) Fortified Position: its "this combat" Shield is applied by the card's `applyPassiveAura`, so it ends if the battlefield card is replaced (Green Father) mid-combat; an effect a resolved ability created should outlast its source — CODE-ONLY
- [ ] **H2** (batch 4b) `EffectExecutor::healObject` has no in-play guard: it emits `"healed"` for an object that has left the board — CODE-ONLY
- [ ] **H3** (batch 4b) Verify against their cited games and pin: G23 and G21 (probably closed by 4a: battlefield rules re-read on replacement; Kennen's play effect bound to his battlefield); (batch 3) B2's stale-mark half (closed by `Card::forgetPlayPayments`; the one-helper half stays open)

## Batch 5 — Trainer-facing engine work (owner, 2026-10-14: settled before v6 starts; trainer plan `../riftbound-trainer/plans/README.md` section 0)

- [ ] **T1** What a decision asks: every prompt carries a description — who asks, which card and ability (`ChainItem` / source), the kind of question (may / mode / X / target / cost / order / rune / location ...), and what each option means — exposed on `StepResult` and the decision API; a `MakeChoice` is no longer a bare integer to an observer. Covers every helper (`confirmOptional`, `pickMode`, `pickXAmount`, `pickTarget`, `pickTargetPair`, `selectSeveral`, `splitDamageAmong`, `confirmNow`, `chooseObjectNow`, engine prompts). Test: every prompt of every decision of a few hundred random and `random-player` games has a description, and two prompts with different meaning never share one — serious (trainer item 8)
- [ ] **T2** Hidden order must not reach a player: check how object ids are assigned (deck order? shuffle order?) and whether anything a player is shown — legal-action lists, option order, twin numbers, observation, log — depends on hidden order. Test: two positions that differ only in hidden order (deck order, hand order, id assignment of hidden cards) give identical observations, identical legal-action lists and identical option order — trainer item 11
- [ ] **T3** Determinized search: `IsMctsAgent` samples each simulation's world as a Clone of the live game ("no hidden-info determinization yet", `src/agents/mcts_agent.h`) and `MctsAgent` replays the live game — both see the opponent's hand and deck order. Give ISMCTS real determinization (hidden cards sampled consistent with `KnowledgeView`); keep perfect-info MCTS, labelled as such. Test: two true hands the searching player cannot tell apart give the same search result. Re-measure ISMCTS vs random / `random-player` afterwards (G28) — trainer items 6, 12
- [ ] **T4** Completeness of the observation: list every fact a player may know (trainer plan section 2) and check each reaches the observation (knowledge tracker, known top/bottom runes per observer, revealed cards still in a hand, public zones up to a whole deck); the reverse of the leak test: two states that differ in a fact the observer may know give different observations — trainer items 10, 14
- [ ] **T5** First-player edge and deck balance on the fixed engine (after G1): random, `random-player` and ISMCTS over the deck lists and random decks, by seat and by who went first; report only — trainer item 7
