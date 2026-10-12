# test-decks — interim report 1 (the first six lists are read; 16 of 18 in fact)

Branch `test/decks`, head `3c5c6ae` (scripts and the findings document only: no engine, card or test code
changed, nothing fixed, by instruction). Worktree `.worktrees/test-decks`.
Document: `docs/testing-findings-decks-2026-10.md` in that worktree (interim, committed): inventory, the
coverage table, every finding with quoted trace lines and a one-command replay, the checked-and-correct list,
the judge questions, the not-observed clauses.

## 1. Where the stream stands

| | |
|---|---|
| Build | lane B, `-j3`, `riftbound` binary only, tests at `-O0` setting, 0 warnings; built once |
| Games | 1,116 fixed-seed traced games, **all rc 0, all end with a winner at 8 points, no engine error**: 324 pairings of the 18 lists + 324 with every sideboard exchanged in + 72 with `ismcts:sims=20` in one seat (the 720 the cards are read against), and a second set of 396 for the script checks |
| Inventory | 261 distinct cards in the 18 lists (18 legends, 104 units, 86 spells, 21 gear, 26 battlefields, 6 runes); all 18 lists legal by the validator and by the constructed rules; every card played or put in play at least once |
| Cards read against a trace | **235 of 261: 175 in full, 60 in part, 0 not at all.** Lists akali, azir, irelia, diana, fiora, ezreal (the first six) and kaisa, khazix, leblanc, mel, reksai, rengar, teemo, vex, viktor are done. Still being read: the 20 cards first held by yasuao, yi_rebuild and zed, the six runes, the combat procedure |
| Who read | nine read-only readers (sub-agents), one or two lists each, with the card's text, class and trace windows side by side; the lead ran the games and the script checks, read the engine-level sequences (D1-D13) and checks the readers' findings: every quoted trace line is looked up mechanically in the cited game (596 fragments; the 21 not found literally are shortened quotes), 26 of the readers' findings read in full by the lead, 3 re-run by hand |
| Findings so far | **74 distinct** (24 more reader findings were the same defect as another and stand under it as further evidence): 40 CERTAIN, 22 LIKELY, 4 CODE-ONLY, 1 QUESTION, 7 mixed |
| Deck-level script checks | clean over all 1,116 games except where a finding explains it (section 3) |

## 2. What is most wrong

Each of these is engine-wide and shows in real games of several lists; reader 9's findings will be numbered after D74, so every D number below is final.

  1. **Combat is not a Showdown State for the action generator** (D5): any spell — with neither [Action] nor
     [Reaction] — is offered and played at Focus in a combat, by the defender on the opponent's turn too.
  2. **Triggered abilities are not processed where the rules process them**: a declined, unpayable or
     targetless trigger stays on the chain and opens a priority window for both players (D4, 2,249 of 2,279
     declined triggers); a trigger queued during a showdown or by a conquer is left pending while Focus is
     passed, the showdown closes and combat damage is dealt (D7); abilities that watch a value ("becomes
     Mighty") are noticed only at top-level cleanups; a unit that joins a combat already under way never gets
     its "When I attack / defend"; conditions printed with a trigger are tested when it resolves.
  3. **An option that does nothing** (D6): with such a late trigger on the chain, a non-Reaction spell is
     offered at chain priority; choosing it is silently dropped and the same decision is asked again — 52 times
     in 18 games; an agent that prefers that option never leaves the decision.
  4. **Spells go on the chain without their targets** (D8 and the readers' findings on effect-played spells):
     Star-Crossed, Switcheroo, Facebreaker, Defiant Dance are offered and paid for with no legal pair; a spell
     replayed by Fizz, Trickster or played by Void Burrower / Void Rush resolves with no target chosen.
  5. **[Add] abilities are ordinary chain items** (D2): both players get priority, the opponent can respond on
     top, the resource arrives late and cannot pay the cost it was meant for.
  6. **Choices the rules make at finalization are made at resolution** (D3 and the readers' findings): move
     destinations, Weaponmaster's "may" and Equipment, several "you may" + target abilities; Equip and
     [Quick-Draw] attach without a chain item at all.
  7. **Continuous effects lag**: Might is not recomputed before a damage step that follows a combat's start
     directly (Forbidding Waste's -2, Trifarian War Camp's +1 on a unit moved in, a granted Shield).
  8. **Hidden information** in five kinds of log lines whose audience is both players (D11).

## 3. Deck-level checks (scripts in `scripts/testing/`, all 1,116 games)

- every game ends with a winner and the reason "Pn reached 8 points"; nothing is decided after the winning point;
- scores step by 1 and never pass 8; the winning point by Conquer only with every battlefield scored that turn
  (226 taken, 192 refused, Baron Pit counted as a third battlefield); one score per battlefield per player per turn;
- holds equal the battlefields controlled as the Scoring Step begins (21,041 turns);
- a requested prompt is the next decision asked (15,177); chain priority goes to the top item's controller or
  to the other player after a pass (76,301 decisions; the 14 exceptions are D6);
- units are exhausted on entry at the first top-level decision (15,326 plays; 345 ready by Accelerate, 41 Xin Zhao);
- pools empty at the first Main Phase decision (24,816 readings; 15,490 more are cut off by the 80-column header);
- 40 cards per player and no name over its count on 57,126 boards — except Reflection copies rendered under the
  copied name (LeBlanc games) and one board short by one card (D10).
- **Not checkable from the binary:** a seat's view of the log (no audience is printed). The emitting statements
  were read instead: D11 lists five line kinds that name hidden cards to both players.

## 4. Findings index (detail, quotes and replay commands in the document)

| id | finding | confidence | from |
|---|---|---|---|
| D1 | Every permanent with a play trigger; Accelerate units: "when you play" abilities are queued and finalized before the permanent has entered, and Accelerate is asked after them | CERTAIN | lead (+1 merged) |
| D2 | Daughter of the Void, Scorn of the Moon, Gold, Seal of Discord: an [Add] ability is an ordinary chain item — both players get priority before it resolves, the opponent can respond on top of it, and the resource arrives late | CERTAIN | lead |
| D3 | Ride the Wind, Unforgiven, Charm, Void Assault (and the cards the readers add: Shuriken Flip, Thrill of the Hunt): the destination of a move is chosen as the spell or ability resolves | CERTAIN | lead |
| D4 | Every "you may" trigger, and triggers with nothing to choose: a declined, unpayable or targetless triggered ability stays on the chain, both players get priority, and it "resolves" | CERTAIN | lead (+4 merged) |
| D5 | Combat is not a Showdown State for the action generator: spells with neither [Action] nor [Reaction] are offered and played at Focus in a combat, by the defender on the opponent's turn too | CERTAIN | lead (+5 merged) |
| D6 | Any non-Reaction spell: offered as a plain play at a chain-priority decision; choosing it does nothing and the same decision is asked again | CERTAIN | lead (+1 merged) |
| D7 | A triggered or delayed ability queued during a showdown is left pending: Focus is passed, the showdown closes and combat damage is dealt with the item still on the chain | CERTAIN | lead (+1 merged) |
| D8 | Star-Crossed, Switcheroo, Facebreaker, Defiant Dance: offered, paid for and put on the chain without a legal choice for each target | CERTAIN | lead (+1 merged) |
| D9 | Every payment that is not a card's base cost: the engine chooses which runes are exhausted and recycled | LIKELY | lead (+1 merged) |
| D10 | Equipment whose bearer dies is not recalled by the next cleanup: it lies unattached at the battlefield through priority windows, once through a Main Phase decision | LIKELY | lead |
| D11 | Hidden information in log lines whose audience is both players | CERTAIN | lead (+1 merged) |
| D12 | Deck validator: no rule for [Unique] | CODE-ONLY | lead |
| D13 | Combat damage: the attacker's damage is dealt before the defender has assigned | LIKELY | lead |
| D14 | Long Sword, Sterak's Gage ([Quick-Draw]): the attach is done inline as the gear lands, not as a triggered ability on the chain | CERTAIN | reader 1, R1-2 |
| D15 | Long Sword, Sterak's Gage ([Equip]): the Equip ability attaches at activation — no chain item, no priority | CERTAIN | reader 1, R1-3 (+1 merged) |
| D16 | Shuriken Flip: the enemy unit, the friendly unit and the move destination are all chosen as it resolves | CERTAIN | reader 1, R1-4 |
| D17 | Shuriken Flip: playable with no friendly unit to move | LIKELY | reader 1, R1-5 |
| D18 | Astral Heron: the discount is kept for a later turn | CERTAIN | reader 1, R1-6 |
| D19 | Astral Heron: the ability is put on the chain while the Heron is not at a battlefield | LIKELY | reader 1, R1-7 |
| D20 | Falling Star: kills between its two instructions, so a death replaced after the first hit is died again on the second | CERTAIN | reader 1, R1-8 |
| D21 | Defy, Not So Fast: only the item on top of the chain can be countered | CODE-ONLY | reader 1, R1-10 (+4 merged) |
| D22 | Emperor of the Sands / Sand Soldier [Weaponmaster]: the "may" and the Equipment are chosen at resolution, not at finalization | CERTAIN | reader 2, R2-1 |
| D23 | Trifarian War Camp / Azir, Sovereign (and other continuous effects): Might is not recomputed before the next window or the damage step — a token moved in by Azir's trigger fights without the +1 [M] | CERTAIN | reader 2, R2-3 |
| D24 | Azir, Sovereign: "you may move any number of your token units" is asked at resolution | LIKELY | reader 2, R2-4 |
| D25 | Hidden cards (Guards!, Hidden Blade, Edge of Night, Pyke, Returned): a facedown card is never offered in its controller's own Neutral Open State | LIKELY | reader 2, R2-6 (+1 merged) |
| D26 | Vex, Cheerless: an enemy spell played while she is in combat is not taxed | CERTAIN | reader 2, R2-7 |
| D27 | Pyke, Returned / Edge of Night: the trigger's condition is tested at resolution, so the ability goes on the chain when it should not trigger | LIKELY | reader 2, R2-8 |
| D28 | Deathgrip: playable with a single friendly unit (no "another friendly unit" to choose) | QUESTION | reader 2, R2-10 |
| D29 | Diana, Lunari: "When a showdown begins here" never triggers on a combat showdown | CERTAIN | reader 3, R3-1 |
| D30 | Scorn of the Moon cannot pay Diana, Lunari's [1]: the "may" is withheld with the legend ready (outcome of K2) | CERTAIN | reader 3, R3-4 |
| D31 | Scorn of the Moon activated with Focus passes Focus to the opponent (outcome of K2) | CERTAIN | reader 3, R3-5 |
| D32 | Fizz, Trickster: a spell whose target is taken from the play intent is replayed with no target (Stupefy, Eclipse, Smoke Screen, Turn to Dust) | CERTAIN | reader 3, R3-6 |
| D33 | Vex, Apathetic: the ability is put on the chain while Vex is in a base; "while I'm at a battlefield" is tested at resolution only | LIKELY / CODE-ONLY | reader 3, R3-8 (+1 merged) |
| D34 | Decree of Insight: its target is chosen after the cost is paid | CERTAIN | reader 3, R3-10 |
| D35 | Grand Duelist / Fiora, Worthy: "becomes [Mighty]" is only noticed at the engine's top-level cleanups, so the triggers arrive late | CERTAIN / LIKELY | reader 4, R4-1 |
| D36 | Sett, Brawler + Grand Duelist / Fiora, Worthy: a unit that stops being Mighty and becomes Mighty again inside one chain triggers nothing | CERTAIN | reader 4, R4-2 |
| D37 | Rengar, Trophy Hunter: the enemy-occupied battlefield is never a play location in a Neutral Open state | LIKELY | reader 4, R4-6 |
| D38 | Divining Shells: "Kill this" is paid when the ability resolves | CERTAIN | reader 4, R4-8 |
| D39 | Sacrifice + Rift Herald: an ability triggered by paying a spell's additional cost is put UNDER that spell | LIKELY | reader 4, R4-9 |
| D40 | Grand Duelist + Fiora, Worthy (and Akshan's two play triggers): the controller is never asked in which order simultaneous triggers go on the chain | CERTAIN | reader 4, R4-10 |
| D41 | Repulse, Forge of the Future: choices the rules place at finalization are made after RESOLVE (K3 kind) | LIKELY | reader 4, R4-11 |
| D42 | Fiora, Victorious: her [Shield] adds nothing to her Might while a chain of the combat showdown is running | LIKELY | reader 4, R4-12 |
| D43 | Ezreal, Prodigy + Thousand-Tailed Watcher (any Accelerate): Accelerate is not an "optional additional cost" for Ezreal's discount | CERTAIN | reader 5, R5-1 |
| D44 | Engine (Wages of Pain, Deadly Flourish, Singularity, ...): a unit with lethal damage stays on the board until the chain is empty | CERTAIN | reader 5, R5-3 (+1 merged) |
| D45 | Bellows Breath: the Repeat's choices ignore "at the same location", are forced to the first execution's count, and are not asked at all when the first execution chose nothing | CERTAIN | reader 5, R5-4 |
| D46 | The List: "As you play this, name a tag" is run as a triggered ability with priority windows, and only tags already public can be named | LIKELY / QUESTION | reader 5, R5-6 |
| D47 | Temporal Breach + Rockfall Path: the banished unit is played back onto a battlefield where "Units can't be played" | LIKELY | reader 5, R5-7 |
| D48 | Temporal Breach (and every effect that plays a unit "ignoring its cost"): Accelerate is never offered | LIKELY | reader 5, R5-8 |
| D49 | Void Gate: no Bonus Damage for a unit that arrived there earlier on the same chain (Akali, Deadly Weapon's move ability) | LIKELY | reader 5, R5-9 |
| D50 | Daughter of the Void: its "[A]. Use only to play spells" cannot pay the Deflect of a spell | CODE-ONLY / LIKELY | reader 5, R5-10 |
| D51 | Kha'Zix, Mutating Horror (also Evelynn, Entrancing; Tornado Warrior): the condition printed with the trigger is tested when the ability resolves, not when the trigger condition is met | CERTAIN | reader 6, R6-1 |
| D52 | Star Spring: "the first time ... each turn" triggers a second time when the same card is played there again that turn | CERTAIN | reader 6, R6-2 |
| D53 | [Temporary] (Deceiver / Mirror Image Reflections; Dusk Rose Lab): the kill is done directly at the start of the Beginning Step, never as a triggered ability on the chain | LIKELY | reader 6, R6-3 |
| D54 | Tornado Warrior (Evelynn, Entrancing by code): the leading "you may" and the target are decided after RESOLVE | CERTAIN / LIKELY | reader 6, R6-6 |
| D55 | LeBlanc, Everywhere at Once: the [Temporary] kill still happens at her battlefield, and the dying unit's other triggered abilities are suppressed instead | CERTAIN | reader 6, R6-7 |
| D56 | LeBlanc, Fragmented: Awaken Phase is counted as "your Beginning Phase" | CODE-ONLY | reader 6, R6-8 |
| D57 | Void Burrower / Void Rush + Cleave, Smite (Blood Rush): a spell played by an effect that needs a target resolves with none | CERTAIN | reader 7, R7-1 |
| D58 | Rek'Sai, Breacher + Void Burrower: a unit played from banishment is not offered Accelerate | CERTAIN | reader 7, R7-2 |
| D59 | Rek'Sai, Breacher: the grant marked in D58 (R7-2) stays on the card; a later play from hand without Rek'Sai on the board is offered Accelerate | CERTAIN | reader 7, R7-3 |
| D60 | Forbidding Waste: the -2 [M] is missing when the combat goes straight to the damage step | CERTAIN | reader 7, R7-4 |
| D61 | Inferna ([Assault 2]) played with [Ambush] into her side's attack is listed as an attacker without the Assault | LIKELY | reader 7, R7-5 |
| D62 | Thrill of the Hunt offers and plays to Rockfall Path | CERTAIN | reader 7, R7-6 |
| D63 | Rengar, Trophy Hunter in the Champion Zone is never offered as an [Ambush] play | LIKELY | reader 7, R7-7 |
| D64 | Undertitan "As I'm revealed from your deck, [Add] [2]" waits on the chain and arrives after the reveal effect is over (K2) | LIKELY | reader 7, R7-10 |
| D65 | Mel, Newly Awakened: the extra -1 [M] is added to every -[M] her controller's resolving item gives, chosen unit or not | CODE-ONLY | reader 7, R7-11 |
| D66 | Herald of the Arcane: the Recruit always goes to the base; no location is offered | LIKELY | reader 8, R8-1 |
| D67 | Swift Scout, Guerilla Warfare: the objects are chosen after RESOLVE; Swift Scout can be activated with nothing to choose | LIKELY | reader 8, R8-2 |
| D68 | Teemo, Strategist (every "When I defend / attack"): a unit that joins a combat already under way never triggers | CERTAIN | reader 8, R8-3 (+1 merged) |
| D69 | Sprite Call played from facedown: the Sprite may be put in the base or at another battlefield | CERTAIN | reader 8, R8-4 |
| D70 | Windsinger played from facedown: a unit at another battlefield is offered | CERTAIN | reader 8, R8-5 |
| D71 | Nocturne, Horrifying: the "As you look at or reveal me" replacement is a triggered ability on the chain | CERTAIN / QUESTION | reader 8, R8-6 |
| D72 | Scoring Step: battlefields are held one at a time, each hold's abilities resolved before the next hold | LIKELY | reader 8, R8-7 |
| D73 | Fortified Position: the [Shield 2] lasts the turn, not the combat | CERTAIN / CODE-ONLY | reader 8, R8-10 |
| D74 | Facebreaker from facedown: offered with no friendly + enemy pair at its battlefield (and as up to 20 identical intents) | CERTAIN | reader 8, R8-11 |

## 5. For planning the fix batches

- Few roots, many findings. The readers named these in the source: `runCombat` never sets the showdown state
  (D5); triggers queued outside the chain's finalize loop — by `cleanup()`, a conquer, the start of a showdown,
  during a resolution — are processed late (D4, D6, D7, D35, D36, D68); `recalculateAuras` is not run between a
  combat's start and its damage step (D23, D42, D60, probably D26 and D61); the effect-play path does not choose
  targets for spells that take them from the play intent (D32, D57); counters read `chain.items.back()` (D21);
  [Add] abilities use the ordinary chain (D2, D30, D31, D64); Equip and [Quick-Draw] bypass the chain (D14, D15).
- No leads of the fix run fired: no `DEFLECT_UNPAID`, no unpayable offer, no engine exception in 1,116 games.
- Remaining for the final report: reader 9 (yasuao, yi_rebuild, zed, runes, combat), the final coverage
  counts, and the closing pass over the document.
