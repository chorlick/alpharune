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

- [ ] **F1** Viktor, Innovator 117 triggers on its own Recruit token and the chain never empties (686 of 100,000 fuzz games); and `ChainManager::processFEPR` gives up after 1,000 passes, drops the chain with a warning and plays on instead of failing the game — CERTAIN (fuzz)
- [ ] **F2** Alpha Strike 751 leaves its finalize pass before the marker when it is played by an effect or its choices are made again after a control change (21 fuzz games; engine exception) — CERTAIN (fuzz)
- [ ] **D1** Every permanent with a play trigger; Accelerate units: "when you play" abilities are queued and finalized before the permanent has entered, and Accelerate is asked after them — CERTAIN
- [ ] **D4** Every "you may" trigger, and triggers with nothing to choose: a declined, unpayable or targetless triggered ability stays on the chain, both players get priority, and it "resolves" — CERTAIN / LIKELY
- [ ] **D6** Any non-Reaction spell: offered as a plain play at a chain-priority decision; choosing it does nothing and the same decision is asked again — CERTAIN
- [ ] **D7** A triggered or delayed ability queued during a showdown is left pending: Focus is passed, the showdown closes and combat damage is dealt with the item still on the chain — CERTAIN
- [ ] **D10** Equipment whose bearer dies is not recalled by the next cleanup: it lies unattached at the battlefield through priority windows, once through a Main Phase decision — LIKELY
- [ ] **D19** Astral Heron: the ability is put on the chain while the Heron is not at a battlefield — LIKELY
- [ ] **D27** Pyke, Returned / Edge of Night: the trigger's condition is tested at resolution, so the ability goes on the chain when it should not trigger — LIKELY
- [ ] **D29** Diana, Lunari: "When a showdown begins here" never triggers on a combat showdown — CERTAIN
- [ ] **D33** Vex, Apathetic: the ability is put on the chain while Vex is in a base; "while I'm at a battlefield" is tested at resolution only — LIKELY / CODE-ONLY
- [ ] **D35** Grand Duelist / Fiora, Worthy: "becomes [Mighty]" is only noticed at the engine's top-level cleanups, so the triggers arrive late — CERTAIN / LIKELY
- [ ] **D36** Sett, Brawler + Grand Duelist / Fiora, Worthy: a unit that stops being Mighty and becomes Mighty again inside one chain triggers nothing — CERTAIN
- [ ] **D39** Sacrifice + Rift Herald: an ability triggered by paying a spell's additional cost is put UNDER that spell — LIKELY
- [ ] **D40** Grand Duelist + Fiora, Worthy (and Akshan's two play triggers): the controller is never asked in which order simultaneous triggers go on the chain — CERTAIN
- [ ] **D44** A unit with lethal damage stays on the board until the chain is empty: no cleanup between the items of a chain (Wages of Pain, Deadly Flourish, Singularity, Unchecked Power, Piercing Light) — CERTAIN / LIKELY
- [ ] **D51** Kha'Zix, Mutating Horror (also Evelynn, Entrancing; Tornado Warrior): the condition printed with the trigger is tested when the ability resolves, not when the trigger condition is met — CERTAIN
- [ ] **D53** [Temporary] (Deceiver / Mirror Image Reflections; Dusk Rose Lab): the kill is done directly at the start of the Beginning Step, never as a triggered ability on the chain — LIKELY
- [ ] **D68** Teemo, Strategist (every "When I defend / attack"): a unit that joins a combat already under way never triggers — CERTAIN / LIKELY
- [ ] **D77** Yasuo, Windrider: an ability is put on the chain for every move, not only "the third time I move in a turn" (same pattern: Fiora, Peerless when not one on one) — LIKELY
- [ ] **F8** Imposing Challenger and Irresistible Faefolk move each other without end (two "may"s answering each other; 9 per million forced-play games): the engine has no answer to a loop except the chain bound. Decide from `rules/core-rules.md` what a loop of optional actions and a loop of mandatory actions are (who must stop, when the game is a draw), implement that, and make an unbroken loop a stated game end rather than an engine error — LIKELY (fuzz)

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

