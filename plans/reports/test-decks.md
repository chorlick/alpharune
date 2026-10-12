# test-decks — final report

## 1. Result

- Branch `test/decks`, head `6df7eb3`, from master `55df656`. Committed: `scripts/testing/` (19 scripts) and
  `docs/testing-findings-decks-2026-10.md`. **No engine, card or test code was changed; nothing was fixed**
  (user decision: find and record). Not pushed, not merged; the main checkout was not touched.
- Build: lane B, `-j3`, once: the `riftbound` binary only (`-DRIFTBOUND_TEST_OPT_LEVEL=0`,
  `CCACHE_NAMESPACE=test-decks`), 0 warnings. **The test suite was not built and not run** — this stream
  changes no code and the task was "build once, then run games and read".
- Games: **1,116 fixed-seed traced games, all exit code 0, every one ends with a winner at 8 points; no engine
  error.** 324 ordered pairings of the 18 lists, 324 with each list's sideboard exchanged into its Main Deck,
  72 with `ismcts:sims=20` in one seat (the 720 the cards were read against); 396 more (second seed, ISMCTS with
  the post-sideboard lists) for the script checks and counts.
- **81 distinct findings, D1-D81**: 40 CERTAIN, 23 LIKELY, 4 CODE-ONLY, 2 QUESTION, 12 with parts of different
  confidence. 28 further reader findings were the same defect as another and are filed under it.

## 2. Coverage, said plainly

- The 18 lists hold **261 distinct cards** (18 legends, 104 units, 86 spells, 21 gear, 26 battlefields, 6
  runes). All 18 lists are legal by the validator and by the constructed rules. Every card was played, put in
  play or channelled in the traced games (sideboard cards through the post-sideboard lists).
- **Read against a trace: 198 in full, 63 in part, 0 not at all.** "In part" = at least one printed clause was
  not seen at work in any of the 720 games; the clause is named per card (document sections 4 and 8).
- Who read: **nine read-only sub-agents ("readers")**, one to three lists each, each card read once by the
  reader of the first list that holds it, with the printed text, the card's class and trace windows side by
  side. The lead (this agent) did not read cards clause by clause: it built the tools, ran the games and the
  script checks, read the engine-level sequences (D1-D13) and checked the readers' work — every quoted trace
  line was looked up mechanically in the cited game (662 fragments; the 24 not found literally are quotes
  shortened with "..." or written as a pattern; two wrong game names in the lead's own findings were caught
  that way), and 29 of the readers' 96 findings were read in full by the lead, four re-run against the log
  (the instances of D5, D20, D60, D79).
- Limits of "in full", as the readers reported them: for clauses that leave no trace line (a passive, "recycle
  the rest", a discount) the comparison is with the rendered board or with a count over all games; several rows
  rest on tallies over every sighting plus a few windows read line by line (the rows say which); some FULL rows
  are by absence (Ruin Runner: no opponent prompt ever lists it). No board was hand-built and no game steered;
  the second set of 396 games was not read for cards.

## 3. What is most wrong
  1. **Combat is not a Showdown State for the action generator** (D5): any spell — with neither [Action] nor
     [Reaction] — is offered and played at Focus in a combat, by the defender on the opponent's turn too.
     Found independently by six readers.
  2. **Triggered abilities are not processed where the rules process them.** A declined, unpayable or
     targetless trigger stays on the chain and opens a priority window for both players (D4: 2,249 of 2,279
     declined triggers). A trigger queued during a showdown or by a conquer is left pending while Focus is
     passed, the showdown closes and combat damage is dealt (D7, D29). "Becomes Mighty" is noticed only at
     top-level cleanups (D35, D36). A unit that joins a combat already under way never gets its "When I
     attack / defend" (D68). A condition printed with the trigger is tested when the ability resolves (D19,
     D27, D33, D51, D77). Simultaneous triggers are never ordered by their controller (D40).
  3. **An option that does nothing** (D6): with such a late trigger on the chain, a non-Reaction spell is
     offered at chain priority; choosing it is silently dropped and the same decision is asked again — 52 times
     in 18 games. An agent that prefers that option never leaves the decision.
  4. **No cleanup between the items of a chain**: a unit with lethal damage stays on the board through
     priority windows (D44), Equipment whose bearer died is not recalled (D10), and continuous effects lag — a
     combat that goes straight to its damage step is fought without Forbidding Waste's -2, Wuju Bladesman's
     +2, Trifarian War Camp's +1 on a unit just moved in, a granted Shield (D60, D76, D23, D42). D60 and D76
     changed who died.
  5. **Spells go on the chain without their targets** (D8, D17, D32, D57): Star-Crossed, Switcheroo,
     Facebreaker, Defiant Dance are offered and paid for with no legal pair; a spell replayed by Fizz,
     Trickster or played by Void Burrower / Void Rush resolves with no target chosen. Deflect is charged for a
     unit Switcheroo then does not choose, or twice (D79 — the `payDeflectForChoices` lead of the fix run).
  6. **[Add] abilities are ordinary chain items** (D2): both players get priority, the opponent can respond on
     top, the resource arrives late — it cannot pay the cost it was meant for (D30, D64) and an Add activated
     with Focus hands Focus over (D31).
  7. **Choices the rules make at finalization are made at resolution** (D3, D16, D22, D24, D41, D54, D67,
     D78): move destinations, Weaponmaster's "may" and Equipment, several "you may" + target abilities.
     Equip and [Quick-Draw] attach without a chain item at all (D14, D15). Counter spells never choose their
     spell: only the top item of the chain can be countered (D21).
  8. **Options withheld**: a facedown card is never offered in its controller's own Main Phase (D25); Rengar's
     Ambush to an enemy-held battlefield never at ordinary timing, never from the Champion Zone (D37, D63);
     the engine picks the runes for every payment but a card's base cost (D9); Accelerate is not discounted by
     Ezreal, Prodigy (D43) and is asked after the play abilities' choices (D1).
  9. **Hidden information** in five kinds of log lines whose audience is both players (D11).
  10. Card-level defects with a changed outcome in a traced game, among others: Falling Star against Zhonya's
     Hourglass (D20), Astral Heron's discount kept for a later turn (D18), Star Spring triggering twice (D52),
     Master of Shadows empowered by the opponent's banish (D75), Vex, Cheerless's tax (D26), Sprite Call and
     Windsinger from facedown (D69, D70), Thrill of the Hunt onto Rockfall Path (D62), Bellows Breath's
     Repeat (D45), Diana, Lunari never triggering in combat (D29).

## 4. Deck-level checks (scripts, all 1,116 games)

| check | result |
|---|---|
| every game ends with a winner and a stated reason; nothing decided after the winning point | clean (always "Pn reached 8 points") |
| score steps, never above 8, winning point by Conquer only with every battlefield scored that turn | clean: 13,247 score lines, 226 winning conquers, 192 refused; Baron Pit counted as a third battlefield |
| one score per battlefield per player per turn; holds = control at the Scoring Step | clean: 21,041 turns, 7,338 holds |
| a requested prompt is the next decision; chain priority to the top item's controller or after a pass | clean: 15,177 prompts; 76,301 priority decisions with 14 exceptions, all D6 |
| units enter exhausted | clean at the first top-level decision: 15,326 plays, 345 ready by Accelerate, 41 Xin Zhao, Vigilant; the transient ready state before is D1 |
| pools empty at the first Main Phase decision | clean on 24,816 readings; 15,490 more are cut off by the renderer's 80-column header |
| card counts add up to 40 per player; no name over its count | clean on 57,126 boards except Reflection copies rendered under the copied name (117 LeBlanc games) and one board one card short (D10) |
| hidden cards never in a line the other seat may read | **could not be checked from the binary** (stdout carries no audience). The emitting statements were read instead: D11, five line kinds that name hidden cards to both players |
| a card in two zones by object id; pools at end of turn; "no priority while an own item is being finalized" | not checkable from the rendered record; D7 is what the last one would have found |

## 5. The two leads of the fix run

- `ChainManager::payDeflectForChoices`: it runs in `addSpell`, after the spell's own cost, so it does not have
  the fault 9.VV1 fixed; no `DEFLECT_UNPAID` and no unpayable offer in 1,116 games (485 Deflect payments). But
  the path is wrong in another way: **D79** — for Switcheroo and Facebreaker it charges the Deflect of the unit
  named by the play option, and the pair prompts then ask for both units again (charged without choosing it,
  or charged twice).
- `canAffordWith` treating Energy and Power as independent: no offer was left unpayable in any game.

## 6. Findings index

Detail — card text or rule, quoted trace lines with game and D, what should have happened, confidence, other
instances — is in `docs/testing-findings-decks-2026-10.md`, section 5. Replay of any citation:
`./build/riftbound --deck1 decks/<a>.txt --deck2 decks/<b>.txt --seed <seed> --agent1 <..> --agent2 <..> --trace --debug`
(a `<deck>_sb` list comes from `python3 scripts/testing/sideboarded_decks.py decks <dir>`).

| id | finding | confidence | from |
|---|---|---|---|
| D1 | Every permanent with a play trigger; Accelerate units: "when you play" abilities are queued and finalized before the permanent has entered, and Accelerate is asked after them | CERTAIN | lead (+1 merged) |
| D2 | Daughter of the Void, Scorn of the Moon, Gold, Seal of Discord: an [Add] ability is an ordinary chain item — both players get priority before it resolves, the opponent can respond on top of it, and the resource arrives late | CERTAIN | lead (+1 merged) |
| D3 | Ride the Wind, Unforgiven, Charm, Void Assault (and the cards the readers add: Shuriken Flip, Thrill of the Hunt): the destination of a move is chosen as the spell or ability resolves | CERTAIN | lead |
| D4 | Every "you may" trigger, and triggers with nothing to choose: a declined, unpayable or targetless triggered ability stays on the chain, both players get priority, and it "resolves" | CERTAIN / LIKELY | lead (+4 merged) |
| D5 | Combat is not a Showdown State for the action generator: spells with neither [Action] nor [Reaction] are offered and played at Focus in a combat, by the defender on the opponent's turn too | CERTAIN | lead (+6 merged) |
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
| D21 | Counter spells (Defy, Not So Fast, Hard Bargain, Riposte, Repulse, Rebuttal, Abandon, Wind Wall): the spell to counter is never chosen — only the top item of the chain can be countered | CODE-ONLY / LIKELY / CERTAIN | reader 1, R1-10 (+5 merged) |
| D22 | Emperor of the Sands / Sand Soldier [Weaponmaster]: the "may" and the Equipment are chosen at resolution, not at finalization | CERTAIN | reader 2, R2-1 |
| D23 | Trifarian War Camp / Azir, Sovereign (and other continuous effects): Might is not recomputed before the next window or the damage step — a token moved in by Azir's trigger fights without the +1 [M] | CERTAIN | reader 2, R2-3 |
| D24 | Azir, Sovereign: "you may move any number of your token units" is asked at resolution | LIKELY | reader 2, R2-4 |
| D25 | Hidden cards (Guards!, Hidden Blade, Edge of Night, Pyke, Returned, Wages of Pain, Temporal Breach): a facedown card is never offered in its controller's own Neutral Open State | LIKELY / CERTAIN | reader 2, R2-6 (+1 merged) |
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
| D44 | A unit with lethal damage stays on the board until the chain is empty: no cleanup between the items of a chain (Wages of Pain, Deadly Flourish, Singularity, Unchecked Power, Piercing Light) | CERTAIN / LIKELY | reader 5, R5-3 (+2 merged) |
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
| D68 | Teemo, Strategist (every "When I defend / attack"): a unit that joins a combat already under way never triggers | CERTAIN / LIKELY | reader 8, R8-3 (+1 merged) |
| D69 | Sprite Call played from facedown: the Sprite may be put in the base or at another battlefield | CERTAIN | reader 8, R8-4 |
| D70 | Windsinger played from facedown: a unit at another battlefield is offered | CERTAIN | reader 8, R8-5 |
| D71 | Nocturne, Horrifying: the "As you look at or reveal me" replacement is a triggered ability on the chain | CERTAIN / QUESTION | reader 8, R8-6 |
| D72 | Scoring Step: battlefields are held one at a time, each hold's abilities resolved before the next hold | LIKELY | reader 8, R8-7 |
| D73 | Fortified Position: the [Shield 2] lasts the turn, not the combat | CERTAIN / CODE-ONLY | reader 8, R8-10 |
| D74 | Facebreaker from facedown: offered with no friendly + enemy pair at its battlefield (and as up to 20 identical intents) | CERTAIN | reader 8, R8-11 |
| D75 | Master of Shadows: empowered when an OPPONENT banishes a card its controller owns | CERTAIN | reader 9, R9-1 |
| D76 | Wuju Bladesman - Starter: the +2 [M] is missing whenever nothing resolves between "COMBAT:" and the damage step | CERTAIN | reader 9, R9-2 |
| D77 | Yasuo, Windrider: an ability is put on the chain for every move, not only "the third time I move in a turn" (same pattern: Fiora, Peerless when not one on one) | LIKELY | reader 9, R9-5 |
| D78 | Kennen, Storm of Shuriken: the spell that gets [Flow] is chosen at resolution | LIKELY | reader 9, R9-6 |
| D79 | Switcheroo on a unit with Deflect (Master Yi, Tempered at Level 6): Deflect is charged for the play option's unit, then both units are asked again — charged without choosing it, or charged twice | CERTAIN | reader 9, R9-7 |
| D80 | Lonely Poro: "died alone" when every other friendly unit there died in the same damage step | QUESTION | reader 9, R9-10 |
| D81 | Minefield: no Burn Out when the Main Deck has fewer than 2 cards | CODE-ONLY | reader 9, R9-11 |

## 7. Also in the document

- Section 3: per-list inventory and every distinct card with its printed text and the lists that hold it.
- Section 4: per card, where it was played / triggered / activated / resolved (game, D), how it was read, the
  result; and the counts over the 720 games.
- Section 6: the surprising things checked and found correct, with the rule (175 entries).
- Section 7: the judge questions (among them: does a unit that enters with 5+ Might "become Mighty"; Deathgrip
  with one unit; is Star Spring's "first time" per player; Lonely Poro "alone" when the others died in the same
  step; The List naming a tag that is not public; Herald of the Arcane's token location; Rengar's Ambush at
  ordinary timing; whether holds are simultaneous). They carry card text, situation, question and the rules on
  either side, but more tersely than `docs/judge-questions-2026-10.md`.
- Section 8: the 84 clauses not observed, with the reason. Section 9: the deck interactions checked per list.
- Section 10: roots shared by several findings, as the readers named them in the source, for batching.

## 8. Notes

- Inventory: nothing the validator accepts in these lists is illegal. Outside the lists: the validator has no
  rule for [Unique] (D12, CODE-ONLY); a list names cards, so a reprint resolves to the first registered id
  (Sett, Brawler 164, Yasuo, Windrider 205, Lonely Poro 359, Plundering Poro 392, Seal of Discord 204) — the
  open "reprint region tags" question.
- Process: the readers were read-only; none edited the repository, built or ran games. Two small departures
  they reported themselves: reader 2 ran one read-only command that used shell variables; reader 4 left a
  helper parser `r4_scan.py` in the session scratchpad root (outside the repository; not removed, `rm` being
  disallowed). The document's parts (the readers' notes, the lead's findings) are in the session scratchpad
  under `test-decks/notes/`; only the assembled document is committed.
- Interim report: `.worktrees/reports/test-decks-1.md`. Its numbers D1-D74 are unchanged here; D75-D81 are new.
- Not done: no fix, no test, no suite run, no checked build (`-DRIFTBOUND_CHECK_BOARD_INDEX=ON`) run of the
  lists — the task for this stream was one optimised build.
