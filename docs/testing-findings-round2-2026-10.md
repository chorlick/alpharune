# Testing phase 2026-10, round 2: stopping-rule pass, round robin, 19 ISMCTS games read

Stream `round2` (worktree `.worktrees/round2b`, branch `test/round2b`). It started from master `1f2230b`, which
has fix batches 1, 1b, 2, 3, 4a, 4b, 5b, 5c-perf, 5d, T3, M1 and 6 merged. This round finds and records only: no
engine, card or test code was changed.

What was added:
- `tools/prompt_trace.cpp`, target `prompt_trace` (not in `all`). It plays one game exactly as `riftbound --trace
  --debug` does and prints `describePrompt` for each decision.
- The coverage counter in `tools/fuzz/fuzz_game.h` now counts [Add] abilities (commit `893d8ef`, at the
  coordinator's request; it is measurement only).
- New scripts under `scripts/testing/round2_*.py` and `fuzz_merge.py`.

Fuzz result files are outside the repository, in `.worktrees/fuzz-results/round2/`.

Binaries:
- Every game ran on the optimised build `build/` or the checked build `build-checked/`, both built from `1f2230b`.
- `deck_fuzz` was rebuilt at 15:49 with the coverage fix. Games are identical across that rebuild: the three
  optimised millions were re-run with the same seeds and gave the same results, decisions and failures.
- Coverage figures below come from the new counter (`*_v2.json`), except where a run is marked "old counter".
  Under the old counter only the [Add] cards differ: they read 0.
- Batch 7-perf had not merged when the runs were made, so every result is from `1f2230b`.

## 1. Summary

- **Stopping rule: pass 1 is not clean, so pass 2 was not run.**
  - Optimised build: one failure group (U3, an engine error raised when a modal spell is offered with no legal
    choice for a required target) in each of the three millions, 3 games in 3,000,000.
  - Checked build (3 x 100,000) and the 39 lists with three agents on both builds (6 x 304,200): no failure.
  - No game hit the turn cap, ended without a winner or a reason, or hung.
  - The flagged games were replayed and each is explained (section 3.3).
  - Coverage: 6 non-banned, non-token cards are below the minimum in some optimised million: Daisy!, Yasuo,
    Windrider (205, 546), Eclipse Dragon, Swain, Visionary, Rell, Magnetic and Renekton, Rage Fueled. Each has a
    rare printed trigger condition. Daisy! meets the minimum through `decks/daisy.txt`.
- **Two regressions of a fixed item (D4), found by reading games:**
  - U1 (CERTAIN): a [Weaponmaster] unit with no Equipment.
  - U2 (LIKELY): Azir, Sovereign with no token to move.
  - In both, the ability stays on the chain, both players get priority, and it resolves doing nothing.
- **New findings: 21 (U1-U21).** By confidence: 8 CERTAIN, 10 LIKELY, 3 QUESTION. Section 4 has each one in full.
- **19 new ISMCTS games** (honest `ismcts:sims=200` in both seats) were read decision by decision against the card
  text and the rules, one reader per game.
  - The readers read all 5,328 decisions and every log tail.
  - Each decision now came with its prompt description.
  - I looked up all 321 trace lines the readers quoted: 318 were found as written, and 3 were elisions with
    "..." in the middle.
  - Every new finding was checked against the rule and, where a cause is named, against the code.
- **Flow focus (Kennen list).** 44 traced games had 104 Flow plays and 31 Kennen grants, and all were read. Timing,
  cost, banishing (also when countered), "this turn" and Heart of the Tempest are all correct. One LIKELY gap (U13).
- **Round robin of the 39 lists:**
  - random, random-player, forced random-player and mixed, 200 games per ordered pairing each: 1,521,000 games,
    no failure.
  - Honest ISMCTS-200: stopped at 1,809 of the planned 3,042 games to free the run lane (section 5). No engine
    failure.
  - Balance tables are in section 5.

## 2. Pass 1 (fresh seeds) against the stopping rule

`--known docs/testing-known-failures.txt` was used for every run; the file has no live entries.

| run | seeds | games | failures | chain 200+ | turn cap / no winner / no reason | hung | decisions mean / max | games/s |
|---|---|---|---|---|---|---|---|---|
| optimised, random | 21510101, 21510102 | 1,000,000 | 1 (U3) | 0 | 0/0/0 | 0 | 263 / 857 | 1894 |
| optimised, random-player | 21510111, 21510112 | 1,000,000 | 1 (U3) | 0 | 0/0/0 | 0 | 253 / 958 | 1914 |
| optimised, random-player:pass=0 | 21510121, 21510122 | 1,000,000 | 1 (U3) | 5 | 0/0/0 | 0 | 291 / 1542 | 1771 |
| checked, random | 21510131 | 100,000 | 0 | 0 | 0/0/0 | 0 | 263 / 779 | 243 |
| checked, random-player | 21510132 | 100,000 | 0 | 0 | 0/0/0 | 0 | 254 / 828 | 258 |
| checked, pass=0 | 21510133 | 100,000 | 0 | 0 | 0/0/0 | 0 | 292 / 847 | 243 |
| lists, optimised, random | 21510141 | 304,200 | 0 | 0 | 0/0/0 | 0 | 309 / 1343 | 1601 |
| lists, optimised, random-player | 21510142 | 304,200 | 0 | 0 | 0/0/0 | 0 | 312 / 1550 | 1544 |
| lists, optimised, pass=0 | 21510143 | 304,200 | 0 | 0 | 0/0/0 | 0 | 347 / 1615 | 1528 |
| lists, checked, random | 21510151, 21510152 | 304,200 | 0 | 0 | 0/0/0 | 0 | 309 / 1292 | 226 |
| lists, checked, random-player | 21510153, 21510154 | 304,200 | 0 | 0 | 0/0/0 | 0 | 312 / 1588 | 224 |
| lists, checked, pass=0 | 21510155, 21510156 | 304,200 | 0 | 0 | 0/0/0 | 0 | 347 / 2106 | 214 |

How the runs were split:
- Each optimised million is two chunks of 25,000 seed decks x 20 games, merged with `scripts/testing/fuzz_merge.py`.
- The checked list runs are two halves of 100 games per pairing.
- The list runs cover the 39 lists x 39 = 1,521 ordered pairings with 200 games each.
- The checked list runs did not use the optimised runs' seeds, so the two builds are not compared game by game here.
  Batches M1 and 6 have already shown on 10,647 games that they play the same games.

Per the rule's criteria:
1. **Failure groups: one.** It is U3, with 3 games in 3,000,000 (one per agent). The rule is not met.
2. **Flagged games.** All are explained by printed behaviour or by the rules (section 3.3).
3. **Coverage.** Section 3.1. The six cards below the minimum all have trigger conditions that seed decks rarely
   meet.

### 3.1 Coverage (new counter, the three optimised millions)

The full per-card table is in Appendix A: games in play / triggered / activated / resolved per million, and the
list runs. Banned cards and token cards are marked and not judged.

Below the minimum: 200 games per way per million, or 50 in the pass=0 million.

| card | way | random | random-player | pass=0 | lists (912,600 games) | why rare |
|---|---|---|---|---|---|---|
| Daisy! 754 | triggered / resolved | 2 | 2 | 4 | 3,223 / 3,178 | four tags among your units; covered by `decks/daisy.txt` |
| Yasuo, Windrider 205 | triggered / resolved | 155 | 160 | 285 | 2,258 | "the third time I move in a turn" |
| Yasuo, Windrider 546 | triggered / resolved | 143 | 169 | 335 | 0 (in no list) | the same |
| Eclipse Dragon 803 | triggered / resolved | 87 | 86 | 133 | 0 | "when I move, if you control 4 or fewer runes" (an [8] unit) |
| Swain, Visionary 852 | triggered / resolved | 61 | 63 | 33 | 0 | "when I conquer, if you've played a non-token unit, a non-token gear and a spell this turn" |
| Rell, Magnetic 347 | triggered / resolved | 195 | 265 | 302 | 0 | "When I attack, you may play an Equipment with Energy cost no more than [2]" (a may with a condition) |
| Renekton, Rage Fueled 806 | triggered / resolved | 429 | 193 | 144 | 0 | "When I attack, if you control 4 or fewer runes" (a [6] unit) |

These cards were not low in round 1. For example, in round 1's random million Yasuo, Windrider 205 triggered in
22,876 games and Eclipse Dragon in 10,259. The difference is the fix batches: the condition printed after a trigger
is now judged as the event happens (batch 1 D51/D77, batch 1b G9). Round 1 counted every move or attack, whether or
not the condition held; round 2 counts only real triggers.

The other way to apply the rule: every one of these cards except Daisy! is in no list, so only a list or a guided
deck (`--guide`) would lift them.

**Old counter vs new counter.** Under the old counter (`p1_opt_*.json`, before commit `893d8ef`), 29 cards with
[Add] abilities read 0 activated and 0 resolved: the Seals, Gold, Hextech Anomaly, Ancient Henge, Energy Conduit,
Honeyfruit, Daughter of the Void, Hand of Noxus, Fire Below the Mountain, Scorn of the Moon, Butcher of the Sands,
Malzahar, Fanatic, Lux, Crownguard, Dragonsoul Sage, Jhin, Murderous Artist's move and Threshold of the Gray.

The cause is in the counter, not the engine. Since batch 3, `GameEngine::resolveAddAbilityNow` announces an [Add]
ability's `ChainItemFinalizedEvent` while the item sits in `chain.resuming`, and the counter only looked in
`chain.items`.

The re-runs with the same seeds (`p1_opt_*_v2.json`) count these cards: every one is well above the minimum, for
example Hextech Anomaly 322 activations in 20,000 games of a test run. The results and decision counts are identical
to the first runs.

### 3.2 Failures

U3 (section 4). Replays:
- `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay 14214519941094446557 13745834397861778865 173746653 --trace --tail 30`
  (random; Curtain Call)
- `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay 5612263688910057979 8534218733791141378 530713274 --agent1 random-player --agent2 random-player --trace --tail 30`
  (Curtain Call)
- `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay 2104744255375938481 11230064011724214041 1172212382 --agent1 random-player:pass=0 --agent2 random-player:pass=0 --trace --tail 30`
  (Rocket Barrage)

### 3.3 Flagged and longest games, replayed

`scripts/testing/round2_replays.py` replayed every flagged game and the longest games that ended. It took the two
longest of each optimised million, list run and mixed run, and the longest of each checked run. It replayed some
with the full trace.

**Chain of 200+ items: 5 games**, all in the pass=0 million. All five are the Irresistible Faefolk / Imposing
Challenger loop, two "may"s answering each other (round 1 F8):
- 101-107 triggers of each in the game, with no other card on the chain.
- The loop was ended by Tournament Rules 505 (`ChainState::kMayLoopIterations` = 100 accepted "may"s), and then the
  game went on to a winner.
- This is the explanation batch M1 gave and the behaviour batch 1 chose for F8. Not a defect.
- Replay of one: `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay 13065530793963490200 13918368632898059358 1347945565 --agent1 random-player:pass=0 --agent2 random-player:pass=0 --trace --tail 30`.

**Longest games that ended (top of each run).** All ended with a winner at 8 and short chains (4-9 items). None hit
a cap.
- Seed decks, 815-958 decisions, 34-55 turns:
  - Teemo, Strategist v Zed, From the Shadows (818 decisions, 55 turns): P2 played Endless Riches on turn 9. From
    then on P2's draws were skipped (`DRAW PHASE: skipped (CR 444)`), and nobody scored from turn 26 to turn 44.
    This is the stall batches M1 and 6 explained by the card's printed text.
  - The others are close games, mostly 7-7 at the end. Random agents rarely meet the winning-point rule (CR 466.1.b:
    a conquer is the last point only if every battlefield was scored that turn; `SCORE_BLOCKED ... draw 1 instead`),
    and two ended by burn-out (CR 432.2.c, `BURN_OUT: P1 deck empty ... P2 gains 1 point`).
- List runs, 1,260-1,615 decisions, 38-69 turns: Jayce, Ezreal and Lux (control lists with few units) against each
  other.
  - The 69-turn Lux v Ezreal game (random-player, seed 1041523500) was replayed in full. It stayed 0-0 until turn 27:
    both sides had few units, Frozen Fortress dealt 1 to each unit there at every Beginning Step, and random-player
    ended turns with options left (`CHOSE: [P1] EndTurn` with 20 legal actions).
  - Time Warp gave P1 two turns in a row (turns 61-62). The game ended on a hold at 7-8.
  - The other list games end by a hold or a burn-out after a long 7-7.
  - Explained by the lists' text and the agents. No loop.
- Checked runs: the longest seed-deck games had 779-847 decisions, all ending with a winner. The longest list games
  had 1,211-2,106 decisions and are all Jayce or Ezreal mirrors and pairings with Lux, the same family.
  - The 2,106-decision game (Ezreal mirror, pass=0, 72 turns, seed in `p1_lists_chk_rp0_h2.json`) ended on a hold
    at 7-8 after a long 7-7.

## 4. New findings (U1-U21)

Regressions come first; the rest follow by root.

| id | finding | confidence | source |
|---|---|---|---|
| **U1** | **REGRESSION of D4**: a [Weaponmaster] unit with no Equipment puts its ability on the chain, gives priority and resolves with nothing | CERTAIN | g09, g11 |
| **U2** | **REGRESSION of D4**: Azir, Sovereign's attack trigger with no token to move (or none chosen) stays on the chain | LIKELY | g05 |
| U3 | Engine error: a modal spell (Curtain Call, Rocket Barrage) is offered, a mode is chosen whose target list then comes back empty, and `executePlaySpell` throws "offered with no legal choice for a required target" | CERTAIN | fuzz, 3 games / 3 M |
| U4 | Two triggered abilities of one play (a "When you play me" and a "when you play a card not from hand", or entering 5+ [M] and "becomes Mighty") are separate event batches, so their controller is never asked their order (CR 383.3.d) | CERTAIN | g03, g01 |
| U5 | A battlefield replaced by Green Father's Brush loses its own "here" / "this" (CR 439.1.a): Brush's accepted swap-back does nothing; Seat of Power draws for its own slot; Green Father after a swap-back does nothing with its cost paid | CERTAIN (two cases), LIKELY (third) | g04, g07 |
| U6 | Jhin, Meticulous Killer's "you may play me for [B]" is offered only from the Champion Zone, never from the hand (and only to base) | CERTAIN | g13 |
| U7 | A permanent played from facedown is never offered its optional additional cost (Pyke, Dockside Butcher's [R]) | CERTAIN | g15 |
| U8 | Piercing Light's [Repeat] execution goes through the generic `makeNewChoicesFor`: base units are offered for "a unit at a battlefield", "up to one" cannot be declined, and the repeat was lost | CERTAIN | g15 |
| U9 | Piercing Light played by Fizz, Trickster is offered "No target" for its required first unit | CERTAIN (offered, not taken) | g15 |
| U10 | [Repeat] is not offered when an effect plays the spell and the player pays a cost (Fizz, Trickster) | LIKELY | g15 |
| U11 | [Repeat] is decided, and its execution's choices made, after the base cost has been paid (CR 355.1.a, 820.2) | LIKELY | g03, g08, g11, g19 |
| U12 | Rek'Sai, Breacher's granted [Accelerate] is not offered to unit tokens played by an effect | LIKELY | g10 |
| U13 | Kennen, Storm of Shuriken's [Flow] grant is ignored for a spell that already prints [Flow] (CR 829.1.c.3) | LIKELY | Flow focus |
| U14 | Sacrifice's additional cost "kill a friendly [Mighty] unit" is asked and recorded as a target (`pickTarget`; CR 355.10.c) | LIKELY | g01, g08 |
| U15 | Nami, Headstrong: "if you paid the additional cost" is judged inside the effect, so the play trigger fires and is removed with a false reason | LIKELY | g18 |
| U16 | `describePrompt` words: a spell played by an effect is "its triggered ability"; an activated ability with a target and a reflexive trigger are "its play" with no question; a battlefield is named by its owner, not its controller | LIKELY | 11 games |
| U17 | Options a player cannot tell apart: order prompts for copies of one ability ("Order — Vilemaw" twice), and two different objects described with the same words (a token copy and the card) | QUESTION | g12, g13, g14, g16 |
| U18 | A trash play's payment (Flow, Kennen's grant) does not ask a forced rune pick and its rune prompts name no card, unlike a hand play's | LIKELY (low) | Flow focus |
| U19 | Log lines that misstate: Fizz's "then recycled it" before the spell resolves (and no line when it happens); Moonfall's "-> 1M" with the aura the unit just left; `TGT_NOT_OFFERED ... at resolve time` written at finalization | LIKELY (log only) | g02, g04, g17, g18, Flow |
| U20 | Two rules questions from the decks stream were never carried into the judge questions, and both met again: does a unit entering with 5+ [M] "become Mighty"; what counts as "chosen ... twice" for Prodigal Explorer | QUESTION | g01, g02 |
| U21 | Every Gold token is the Spiritforged card (326), also from Unleashed cards that name the Unleashed Gold (564); card 564 never enters play | QUESTION (cosmetic) | g07, g15, fuzz |

Confidence counts: CERTAIN 8 (U1, U3-U9; U5's third case is LIKELY), LIKELY 10 (U2, U10-U16, U18, U19),
QUESTION 3 (U17, U20, U21).

### U1 REGRESSION of D4: [Weaponmaster] with no Equipment stays on the chain
- Cards: Lucian, Merciless and Riven, Shattered (the [Weaponmaster] keyword: "When you play me, you may [Equip] one
  of your Equipment to me ... [A] less").
- Rule: CR 403.1.a and 403.4 (a triggered ability that is not performed leaves the chain as it would be finalized),
  and CR 383.3.e.2.b. Engine convention: `remove_at_finalize` when the leading "may" is declined or not offered.
  D4 (batch 1) is FIXED for this pattern, and round 1 cited exactly these cards under D4: g05's "Lucian's
  [Weaponmaster] with no Equipment on the board", g01's "Riven's Weaponmaster with no Equipment".
- Trace (g09, SNAP 125-127, idx 74-76):
  - `[D:064.02] TRIGGER: Lucian, Merciless (P2) fires ability`
  - `[D:064.03] FINALIZE-CHOICES: Lucian, Merciless (chain_item=13, ability=1, spell=0)`
  - `[D:065.00] DECISION #75 (P2 — chain priority): 1 legal actions`
  - `[D:066.00] DECISION #76 (P1 — chain priority): 1 legal actions`
  - `[D:066.02] RESOLVE: Lucian, Merciless (chain_item=13, ability=1, spell=0)`
- Also seen in g11, SNAP 73-75: Riven, Shattered, `[D:042.03]` to `[D:044.02]`.
- What should have happened: no Equipment means nothing to choose. The ability leaves the chain at finalization:
  there is no priority window and no resolution.
- Cause (code read): `src/cards/units/weaponmaster_base.h` `onTrigger`:
  `const int conf = still_legal() ? confirmOptional(...) : 0;`. With no Equipment, `confirmOptional` is never
  called, so `ChainItem::may_answer` stays -1 and the CR 403 removal does not apply. The file is unchanged since
  before batch 1, so D4's fix never reached it. Every Weaponmaster unit is affected.
- Confidence: CERTAIN.

### U2 REGRESSION of D4: Azir, Sovereign with no token to move
- Card: Azir, Sovereign (497): "When I attack, you may move any number of your token units to this battlefield."
- Rule: as U1, and CR 383.3.e.2 / 383.3.e.2.b. Round 1 listed exactly this Azir case under D4.
- Trace (g05, SNAP 243-245): every P1 token was already at that battlefield.
  - `[D:165.05] TRIGGER: Azir, Sovereign (P1) fires ability`
  - `[D:165.06] FINALIZE-CHOICES: Azir, Sovereign (chain_item=45, ability=1, spell=0)`
  - `[D:166.00] DECISION #177 (P1 — chain priority): 1 legal actions`
  - `[D:167.00] DECISION #178 (P2 — chain priority): 1 legal actions`
  - `[D:167.02] RESOLVE: Azir, Sovereign (chain_item=45, ability=1, spell=0)`
  - Again at `[D:243.05]` to `[D:245.02]`.
- For comparison, Tideturner in the same game is removed correctly:
  `[D:046.05] REMOVED: Tideturner (chain_item=14) leaves the chain unfinalized — not performed (CR 403)`.
- Cause (code read): D24's rewrite (batch 3) uses `selectSeveralTargets` with zero allowed and no leading
  `confirmOptional`. An empty list, or choosing none, is therefore a performed ability.
- What should have happened: with no movable token the ability is removed at finalization. Choosing none is the
  declined "may".
- Confidence: LIKELY. The only doubt is whether "any number" (zero included) should be read as the "may" itself.

### U3 Engine error: modal spells offered with no legal target for the mode chosen
- Cards:
  - Curtain Call (743): "[Repeat] — [1] / [A] / [1][A] ... Choose one you haven't already chosen — Draw 1. / Deal 2
    to a unit at a battlefield. / Deal 3 to a unit at a base. / Give a unit at a battlefield -4 [M] this turn."
  - Rocket Barrage (400): "[Repeat] [4][B] ... Choose one — ...".
- Rule: CR 355.8. An option that cannot be executed must not be offered (engine contract, CLAUDE.md "An offered
  option that cannot be executed throws too").
- Trace (first replay, P2's main phase after a combat; P2 had 10 floating Energy from Hextech Anomaly):
  - `[D:220.02] SPELL: Curtain Call (id=88) targets=[]`
  - `[D:220.04] MODE_PROMPT: Curtain Call [Draw 1/Deal 2 (BF)/Deal 3 (base)/-4M (BF)] (agent decides among 4 modes)`
  - `[D:221.03] MODE_PICKED: Curtain Call = mode#1 (Deal 2 (BF))`
  - `[D:221.04] TGT_NOT_OFFERED: Curtain Call (no legal targets at resolve time)`
  - Then the engine error. Rengar, Unseen (P1, 2 damage) stood at BF#1. The second Curtain Call game is the same
    pattern with mode#2 (a unit at a base), after Hextech Anomaly added 4 Energy.
- What is established: the card's mode mask is built from its unfiltered `chooseableUnits` lists. The target list
  `pickTarget` offers then passes through the pre-cost choice filter (`EffectExecutor::choiceAllowed`, set by
  `executePlaySpell`), and that filter refused every unit.
- What is not established: why the filter refused them (`canAffordChoosing` counts floating Energy). Both Curtain
  Call games had floating Energy from an [Add]. The Rocket Barrage game was against Scorn of the Moon.
- Should: the mode offered only if its target can be chosen, or the gate and the filter agreeing.
- Rate: 1 game per million on each agent.
- Confidence: CERTAIN (engine exception).

### U4 Two abilities triggered by one play are not ordered by their controller
- Cards: Kennen, Storm of Shuriken (900, "When you play me, [Burn 2]") played from the Champion Zone, with Heart of the
  Tempest (942, "When you play a card from anywhere other than your hand, empower me"). Both are P2's.
- Rule: CR 383.3.d. "If more than one Triggered Ability is Triggered simultaneously, then the player that controls
  the Abilities selects the order to place them on the Chain."
- Trace (g03, SNAP 136-137):
  - `[D:075.02] TRIGGER: Kennen, Storm of Shuriken (P2) fires ability`
  - `[D:075.03] TRIGGER: Heart of the Tempest (P2) fires ability`
  - `[D:075.04] FINALIZE-CHOICES: Kennen, Storm of Shuriken (chain_item=17, ability=1, spell=0)`
  - `[D:076.00] DECISION #86 (P2 — chain priority): 1 legal actions`
  - There was no order prompt.
- Cause (code read): `GameEngine::announcePermanentPlayed` emits `CardPlayedEvent` and then
  `ObjectStateChangedEvent "played_not_from_hand"`. Each is its own `TriggerEventScope` batch, and no
  `SimultaneousTriggers` spans the two.
- The same shape appears in g01 (SNAP 252-254): Harnessed Dragon's play trigger and Grand Duelist's "becomes
  [Mighty]" on its entry, with no order prompt. That case depends on U20's question.
- Not a regression of D40: D40 fixed one event's batch.
- Confidence: CERTAIN (both conditions are met by one event, a play).

### U5 A battlefield replaced by Green Father's Brush loses its "here"
- Cards:
  - Green Father (753): "When you conquer or hold, you may exhaust me to replace that battlefield with a Brush
    battlefield token."
  - Brush (561): "When you score here, you may replace this with the battlefield it replaced."
  - Seat of Power (531): "When you conquer here, draw 1 for each other battlefield you or allies control."
- Rule: CR 439.1.a, "The replacing token is treated as the same Game Object as the card or token it replaced for the
  purposes of Game Effects that target or reference that game object". Its own example is Green Father resolving
  before the conquered battlefield's own ability, where "here" still works. Also CR 439.7.a (swapping back is an
  extension of Replace).
- (a) g04, SNAP 89-95. **CERTAIN.**
  - `[D:054.03] MAY_ACCEPTED: Brush: replace this with the battlefield it replaced?`
  - `[D:057.03] BF_REPLACE: BF#1 replaced by 'Brush' token (card=110)`
  - `[D:059.03] MAY_INVALIDATED: Brush: replace this with the battlefield it replaced? (no longer possible at resolution)`
  - Should: swap back to Targon's Peak (eligible by CR 439.7.b.1).
- (b) g07, SNAP 404-411. **CERTAIN.**
  - `[D:248.03] BF_REPLACE: BF#1 replaced by 'Brush' token (card=114)`
  - `[D:250.03] EFFECT: P2 draws 1 (deck=24)`
  - `[D:250.04] SEAT OF POWER: conquer -> draw 1`
  - P2 controlled no other battlefield. Should: draw 0. In the same game without a replacement:
    `[D:040.03] SEAT OF POWER: conquer -> draw 0`.
- (c) g04, SNAP 143-150. **LIKELY.** Brush swapped back first.
  - `[D:088.03] BF_REPLACE: BF#1 is 'Targon's Peak' again (card=108)`
  - `[D:090.02] RESOLVE: Green Father (chain_item=21, ability=1, spell=0)`
  - Green Father's exhaust had been paid and nothing was replaced.
- Cause (code read): the battlefield cards find their slot with `bf.card_object_id == ctx.source`. After a
  replacement the slot holds the token. By code the same test is in Reckoner's Arena 281, Emperor's Dais 521,
  Abandoned Hall 761, Dusk Rose Lab 765 and Frozen Fortress 768 (not seen at work). Green Father gives up when its
  subject is no longer a battlefield's card.

### U6 Jhin, Meticulous Killer's [B] play is never offered from the hand
- Card: Jhin, Meticulous Killer (651): "[Vision] ... If you've spent [4] or more to play a spell this turn, you may
  play me for [B]." No zone is named.
- g13: Lux is P1's champion and all three Jhins are in P1's Main Deck. After Progress Day (`[D:057.03] PAY_COST:
  Progress Day E=6 P=1`) and Sprite Burst, Jhin was absent from 8 main-phase decisions (idx 85, 88, 106, 165, 199,
  206, 208, 211) while [B] was payable: Seal of Insight (1 Mind Power) was offered at the same decisions, or a
  floating [B] was in the pool (`[D:071.05] EFFECT: P1 adds 1 floating Mind Power (pool=1)`).
- Cause (code read): `game_engine.cpp` ~5232-5263 builds the `use_alt_play_cost` intent only from
  `ps.champion_zone`, and only to base.
- Should: offered from the hand, to every location the unit may be played to.
- Confidence: CERTAIN (a withheld option).

### U7 Optional additional costs are not offered for a play from facedown
- Card: Pyke, Dockside Butcher (590): "[Hidden] ... You may pay [R] as an additional cost to play me. When you play
  me, if you paid the additional cost, ready me and give me +2 [M] this turn."
- Rule: CR 811.1.b ("ignoring its base cost"); CR 356.1.b and its example (an optional additional cost is still paid
  when the costs are ignored).
- g15, SNAP 64: `[D:027.02] INTENT: PlayReaction by P2` is followed directly by `[D:028.00] DECISION #30 (P1 — chain
  priority): 1 legal actions`. P2 had two ready Fury runes. Nothing was asked.
- Cause (code read): `GameEngine::executePlayFromHidden` never calls `maybePayOptionalAdditionalCost` or
  `payAdditionalPlayCost`. [Accelerate] is offered later, at `resolvePermanent`, so it is not affected.
- Confidence: CERTAIN.

### U8 Piercing Light's [Repeat] execution ignores the card's target roles
- Card: Piercing Light (346): "[Repeat] [2][R] ... Deal 2 to a unit at a battlefield, then deal 2 to up to one other
  unit."
- Rule: CR 820.2 and 820.2.a (each execution's choices follow the card's requirements); CR 355.13 ("up to").
- g15, SNAP 197-198:
  - `[D:112.00] DECISION #122 (P2): make a new choice [4 options]` offered three units in base for the first unit.
  - `[D:113.00] DECISION #123 (P2): make a new choice [3 options]` had no "none".
  - P2 chose Traveling Merchant at base, then Tideturner.
  - `[D:121.06] RESOLVE: Piercing Light — 1 target(s) no longer legal (per-CR 359.3.e.5: unaffected, rest of spell still resolves)`
    and the repeat dealt no damage.
- Cause (code read): the repeat's choices go through `makeNewChoicesFor`'s generic branch (`enumerateLegalTargets`,
  the first execution's count). This is the same class as D45, which batch 4a fixed only for Bellows Breath.
- Confidence: CERTAIN.

### U9 Piercing Light played by an effect is offered "No target"
- g15, SNAP 316: `[D:193.00] DECISION #209 (P2): choose the targets for Piercing Light [7 options]`. Option `[6] No
  target` sat beside the six legal sets. It was not taken.
- Cause (code read): `game_engine.cpp` ~3765 adds "No target" when `req.optional`, and Piercing Light's second slot
  is optional.
- Rule: CR 355.8 (the first unit is required).
- Confidence: CERTAIN (an illegal option offered).

### U10 [Repeat] is not offered on an effect play
- Same game: Fizz, Trickster played Piercing Light (`[D:193.02] PAY_COST: Piercing Light E=0 P=1`,
  `[D:194.02] FIZZ: played Piercing Light from trash ...`) with no Repeat prompt. [2][R] was payable by the reader's
  count.
- Rule: CR 820.1.d (Repeat is an additional cost "as you play this"). An effect play is a play.
- Cause (code read): only `finishPlaySpell` offers Repeat; `EffectExecutor::playPayingCost` does not.
- Confidence: LIKELY (the payability is a hand count).

### U11 [Repeat] is decided after the base cost is paid
- Rule: CR 355.1.a, "This includes the choice of whether or not to pay an Optional Additional Cost" (in the Make
  Relevant Choices step, before 356/357). CR 820.2: an extra execution's choices are made "at the usual time during
  the Make Relevant Choices step".
- Seen with Hard Bargain (g03, SNAP 213-219):
  - `[D:128.04] PAY_COST: Hard Bargain E=2 P=0`
  - `[D:131.00] DECISION #141 (P2): pay Repeat tranche 1 for Hard Bargain? [decline|pay] [2 options]`
  - `[D:134.03] TGT_PICKED: Hard Bargain: counter a spell = Flash (chain_item=31)`
- Also seen with Blood Rush (g08, g11) and Bellows Breath (g19).
- CLAUDE.md documents the order ("the spell's choices, then cost prompts (additional costs, Repeat)") and says the
  Repeat's choices are "made with the first one's". The traces show them after the Repeat is paid.
- No outcome changed in these games. The difference matters when Deflect or a discount depends on what the repeat
  chooses, and for whether the total cost is judged together.
- Confidence: LIKELY (a documented design that reads against 355.1.a).

### U12 Rek'Sai, Breacher's granted [Accelerate] and tokens played by an effect
- Card: Rek'Sai, Breacher (352): "... Friendly units played from anywhere other than a player's hand have
  [Accelerate]."
- Up from the Deep: "Play two 1 [M] Tentacle unit tokens".
- g10, SNAP 122-124: `[D:063.03] TOKEN: created Tentacle (1M, id=111) for P1` and `[D:063.04] ...id=112`. No
  Accelerate prompt was asked, with 3 ready runes, and both tokens entered exhausted.
- Rule: CR 185.2.a (a token is played "following all the applicable steps for playing a card"); 805.1.b.2 (a token
  with no Domain pays the Power with [A]); the example at line 1312 of `rules/core-rules.md`.
- Next to D58, which was fixed for cards. Not a regression.
- Confidence: LIKELY.

### U13 Kennen's [Flow] grant on a spell that already prints [Flow]
- Card: Kennen, Storm of Shuriken: "give a spell in your trash [Flow] equal to its cost this turn". Lightning Rush:
  cost [1], "[Flow] [2][A]".
- Rule: CR 829.1.c.3 ("multiple instances of the Flow keyword with different costs, its controller may choose").
- The grant went to Lightning Rush four times (Flow focus g01 `[D:064.03]`, g17, g24, g39). All 64 Lightning Rush
  Flow plays paid [2] + 1 Power. In those four turns no ready rune was left, so no trace shows a [1] play being
  withheld.
- Cause (code read): `refreshFlowGrants` uses `flowCost()` when one is printed and reads the grant only otherwise.
- Confidence: LIKELY.

### U14 Sacrifice's additional cost is asked as a target
- Card: Sacrifice (735): "[Reaction] As an additional cost to play this, kill a friendly [Mighty] unit. Draw 2 and
  channel 1 rune exhausted."
- Rule: CR 355.10.c: "As an additional cost to play me, kill a friendly unit" doesn't target anything.
- g01: `[D:093.04] TGT_PROMPT: Sacrifice: kill a Mighty unit [Rengar, Trophy Hunter] (agent decides among 1 targets)`
  and `[D:094.03] TGT_PICKED: ...`. The prompt kind is Target. g08 is the same.
- `0735_sacrifice.cpp` uses `pickTarget`, which records the unit as chosen (feeding Deflect gates and "chosen"
  counts) where a cost pick should not.
- The payment itself (Energy, then the kill) was right.
- Confidence: LIKELY (no outcome changed).

### U15 Nami, Headstrong's condition is judged inside the effect
- Card: Nami, Headstrong (614): "You may pay [G] as an additional cost to play me. When you play me, if you paid the
  additional cost, [Stun] an enemy unit."
- Rule: CR 383.2.a.1, as CLAUDE.md applies it: a condition after the trigger condition belongs in `triggerCondition`.
- g18:
  - `[D:256.01] ADDITIONAL_COST: Nami, Headstrong declined additional cost`
  - `[D:259.02] TRIGGER: Nami, Headstrong (P1) fires ability`
  - `[D:259.04] TGT_NOT_OFFERED: Nami: stun an enemy unit (no legal targets at resolve time)`. Two enemy Rumbles were
    legal targets.
  - `[D:259.05] REMOVED: ...`
- Effect: the reason in the trace is wrong, and with another simultaneous play trigger of P1's there would be an
  order prompt that should not exist.
- Confidence: LIKELY.

### U16 `describePrompt` describes some prompts wrongly
What a player (or the trainer) is told; the game state was right in every case.
- A spell played by an effect is "its triggered ability":
  - g02 SNAP 198: `P2 You decide (target) for your Wages of Pain in the trash, its triggered ability, as it resolves`
    (Fizz).
  - The same in g04 (Stupefy), g11 (Hextech Ray and Void Rush through Void Burrower) and g15.
- An activated ability with a target, or a reflexive trigger, is "its play", with no question words:
  - Heart of the Tempest (g03), Voidreaver (g10), Matriarch of War (g17), Ivern, Nurturer's reflexive Buff (g04),
    Deceiver's reflexive copy (g16).
  - Example: `P2 You decide (target) for your Voidreaver in the Legend Zone, its play, as it is put on the chain`.
- A battlefield is named by its owner, not its controller (CR 190.6):
  - `P1 You decide (may) for the opponent's Star Spring at a battlefield ...` while P1 controls it.
  - Also g02 (Startipped Peak, Sigil of the Storm), g03 (Zaun Warrens), g06 (Emperor's Dais), g08, g16.
- Confidence: LIKELY.

### U17 Options a player cannot tell apart
- Order prompts for copies of one ability:
  - g12 idx 283: `0 value Order — Vilemaw` / `1 value Order — Vilemaw`.
  - g13: two Spirit Wheels.
  - g14: three Sprite [Temporary] instances. Here the order mattered: the Sprite carrying Brutalizer died first.
- g16 SNAP 121: the ready [Temporary] Reflection copy and the exhausted Karthus card are both "your Karthus, Eternal
  at battlefield 1". A combat-damage option pair has identical words, one killing the token and one the card.
- D40 asks only for "two or more DIFFERENT abilities"; copies are treated as different sources.
- Question: should copies of one ability be asked about at all? If yes, the words must tell them apart (location,
  status).
- Confidence: QUESTION.

### U18 Trash-play payments skip forced rune picks
- In the Flow focus games, a Flow play's payment goes through `EffectExecutor::payResources`:
  - It asks nothing when one kind of rune is left: `[D:105.04] PAY_ADD: P1 pays [3] + 0 power: exhausted 3,
    recycled 0`, with no prompt, in 26 of 26 Up from the Deep Flow plays.
  - Its rune prompts name no card: `[PRM] #99 P1 You decide (rune for energy): pick rune to exhaust for energy`.
- A hand play asks even a forced pick (the `payCardCost` comment: "training fidelity"). CR 829.1.c.1 makes the Flow
  cost the spell's cost.
- Confidence: LIKELY (low impact: training signal and play UI only).

### U19 Log lines that misstate what happened
- Fizz, Trickster (g02):
  - `[D:115.03] FIZZ: played Wages of Pain from trash (energy<=3, ignoring energy), then recycled it` is written
    before Wages of Pain resolves (`[D:120.02]`).
  - The recycle itself has no line.
- Moonfall (g04): `[D:081.04] EFFECT: give Stalwart Poro -2 [M] this turn -> 1M` while the board and the damage step
  show 0M. The value still includes the Brush +1 of the battlefield the unit left.
- `TGT_NOT_OFFERED ... (no legal targets at resolve time)` is written at finalization: Flow focus g31 `[D:042.03]`,
  g17 Pit Rookie `[D:054.04]`, g18 Nami (U15).
- Confidence: LIKELY (log only; state correct).

### U20 Two questions of the decks stream that were never tracked
`docs/testing-findings-decks-2026-10.md` QUESTIONS has both, but neither is in `docs/judge-questions-2026-10.md` and
neither is an item in `plans/`.
- (a) Does a unit entering the board with 5+ [M] "become Mighty"?
  - CR 709: "at the moment its Might changes from being less than 5". CR 711: in a non-board zone a unit is judged
    by its printed Might.
  - The engine says yes. g01: Grand Duelist triggered on Rengar entering (`[D:066.02]`) and on Harnessed Dragon
    entering (`[D:143.02]`), and the second gave P2 a 12th rune (`[D:151.05] GRAND DUELIST: exhausted -> channeled
    1 rune (exhausted)`).
- (b) Prodigal Explorer: "Use only if you've chosen enemy units and/or gear twice this turn". The engine counts each
  object chosen, so one Bellows Breath with its Repeat met the condition (g02 SNAP 325).
- Confidence: QUESTION (for the judges).

### U21 Gold tokens are always card 326
- `createGoldToken` (`card_helpers.h`) uses `kGoldGearCardDefId = 326` (Spiritforged). Unleashed cards that make
  Gold (Pyke, Bloodharbor Ripper) therefore make 326, and card 564 (Unleashed Gold) is never in play. A 20,000-game
  fuzz run had 0 games of 564 in play.
- The two texts differ only in how the [Reaction] timing is printed ("Kill this, [E]: [Reaction] — [Add] [A]" vs
  "[Reaction][>] Kill this, [E]: [Add] [A]").
- Confidence: QUESTION (cosmetic unless the printings ever differ).

## 5. Round robin of the 39 lists

**Fast agents.** Every ordered pairing (mirrors included), 200 games each, optimised build. The two mixed runs give
both seatings of random v random-player. No failures, no flagged games.

#### random v random

304200 games: P1 151443 (49.8%), P2 152757 (50.2%), no winner 0, engine failure 0; mean turns 17.2, mean decisions 309

| list | games | win % | as P1 | as P2 | mean turns |
|---|---|---|---|---|---|
| daisy | 15200 | 87.7 | 87.3 | 88.1 | 14.4 |
| rengar | 15200 | 73.3 | 72.7 | 74.0 | 15.5 |
| fiora | 15200 | 68.7 | 68.7 | 68.6 | 18.5 |
| ivern | 15200 | 65.5 | 65.2 | 65.8 | 16.3 |
| poppy | 15200 | 65.1 | 65.3 | 65.0 | 16.7 |
| lucian | 15200 | 64.3 | 64.6 | 64.0 | 16.2 |
| zed | 15200 | 64.0 | 63.9 | 64.0 | 16.1 |
| draven | 15200 | 62.0 | 62.2 | 61.9 | 16.6 |
| rumble | 15200 | 60.7 | 60.1 | 61.2 | 17.5 |
| khazix | 15200 | 58.6 | 58.2 | 58.9 | 15.8 |
| missfortune | 15200 | 57.8 | 57.7 | 57.9 | 15.9 |
| darius | 15200 | 56.3 | 55.9 | 56.7 | 16.8 |
| ahri | 15200 | 54.7 | 55.1 | 54.2 | 16.4 |
| vex | 15200 | 54.5 | 55.0 | 53.9 | 16.2 |
| yi_rebuild | 15200 | 53.8 | 53.1 | 54.5 | 16.0 |
| vi | 15200 | 53.6 | 53.2 | 53.9 | 17.2 |
| kennen | 15200 | 52.8 | 52.9 | 52.7 | 16.6 |
| kaisa | 15200 | 51.1 | 50.7 | 51.6 | 18.1 |
| sivir | 15200 | 50.7 | 49.6 | 51.8 | 17.0 |
| nasus | 15200 | 50.7 | 50.2 | 51.2 | 16.6 |
| reksai | 15200 | 50.4 | 50.3 | 50.4 | 17.3 |
| viktor | 15200 | 49.4 | 49.5 | 49.4 | 17.9 |
| ambessa | 15200 | 48.7 | 48.5 | 49.0 | 16.8 |
| akali | 15200 | 46.1 | 45.8 | 46.4 | 17.6 |
| yasuao | 15200 | 45.1 | 44.5 | 45.7 | 16.5 |
| leblanc | 15200 | 44.2 | 44.4 | 44.0 | 18.5 |
| teemo | 15200 | 43.8 | 43.7 | 44.0 | 17.2 |
| diana | 15200 | 42.4 | 42.3 | 42.4 | 17.4 |
| renata | 15200 | 42.3 | 42.6 | 42.1 | 18.4 |
| irelia | 15200 | 42.2 | 41.6 | 42.7 | 17.0 |
| mel | 15200 | 38.4 | 37.4 | 39.5 | 18.1 |
| lux | 15200 | 37.3 | 37.0 | 37.6 | 19.8 |
| leona | 15200 | 36.9 | 36.8 | 37.0 | 17.1 |
| ornn | 15200 | 36.2 | 35.6 | 36.7 | 16.8 |
| lillia | 15200 | 34.0 | 33.7 | 34.3 | 17.2 |
| pyke | 15200 | 32.7 | 32.3 | 33.0 | 18.1 |
| azir | 15200 | 29.5 | 29.5 | 29.4 | 17.7 |
| jayce | 15200 | 25.7 | 25.4 | 26.0 | 18.7 |
| ezreal | 15200 | 19.0 | 19.2 | 18.8 | 20.0 |

#### random-player v random-player

304200 games: P1 152105 (50.0%), P2 152095 (50.0%), no winner 0, engine failure 0; mean turns 18.4, mean decisions 312

| list | games | win % | as P1 | as P2 | mean turns |
|---|---|---|---|---|---|
| daisy | 15200 | 78.5 | 78.8 | 78.3 | 15.9 |
| fiora | 15200 | 74.9 | 74.4 | 75.4 | 19.5 |
| rengar | 15200 | 73.2 | 73.0 | 73.4 | 16.3 |
| poppy | 15200 | 66.2 | 66.3 | 66.0 | 17.8 |
| ivern | 15200 | 63.9 | 63.6 | 64.2 | 17.3 |
| draven | 15200 | 62.8 | 62.7 | 62.9 | 17.8 |
| lucian | 15200 | 61.3 | 60.9 | 61.7 | 17.2 |
| zed | 15200 | 61.2 | 61.1 | 61.2 | 17.3 |
| missfortune | 15200 | 60.9 | 61.1 | 60.8 | 17.0 |
| khazix | 15200 | 59.3 | 59.6 | 59.1 | 16.8 |
| rumble | 15200 | 58.5 | 59.1 | 57.9 | 18.7 |
| yi_rebuild | 15200 | 55.9 | 55.9 | 56.0 | 16.8 |
| vex | 15200 | 54.0 | 53.6 | 54.3 | 17.3 |
| darius | 15200 | 53.9 | 54.2 | 53.6 | 18.1 |
| ahri | 15200 | 53.4 | 53.9 | 53.0 | 17.4 |
| sivir | 15200 | 53.1 | 53.1 | 53.1 | 18.3 |
| reksai | 15200 | 52.4 | 52.7 | 52.1 | 18.6 |
| nasus | 15200 | 50.8 | 50.4 | 51.1 | 17.5 |
| kennen | 15200 | 50.2 | 50.3 | 50.0 | 18.0 |
| vi | 15200 | 49.5 | 49.8 | 49.1 | 18.6 |
| viktor | 15200 | 49.4 | 49.5 | 49.2 | 19.8 |
| kaisa | 15200 | 48.6 | 48.4 | 48.8 | 19.6 |
| ambessa | 15200 | 48.0 | 47.6 | 48.4 | 18.0 |
| yasuao | 15200 | 47.5 | 46.9 | 48.2 | 17.6 |
| diana | 15200 | 47.5 | 47.2 | 47.8 | 18.8 |
| teemo | 15200 | 47.2 | 46.7 | 47.6 | 18.8 |
| leblanc | 15200 | 44.7 | 44.7 | 44.7 | 20.2 |
| irelia | 15200 | 44.1 | 44.1 | 44.2 | 18.1 |
| renata | 15200 | 42.6 | 42.7 | 42.5 | 19.9 |
| akali | 15200 | 42.6 | 42.2 | 42.9 | 19.0 |
| leona | 15200 | 39.8 | 40.0 | 39.7 | 18.3 |
| mel | 15200 | 39.5 | 39.5 | 39.5 | 19.7 |
| pyke | 15200 | 35.7 | 36.2 | 35.3 | 19.5 |
| azir | 15200 | 34.9 | 35.3 | 34.6 | 19.1 |
| lillia | 15200 | 34.6 | 34.4 | 34.7 | 18.5 |
| lux | 15200 | 34.1 | 34.2 | 34.0 | 21.1 |
| ornn | 15200 | 33.6 | 34.2 | 33.0 | 18.0 |
| jayce | 15200 | 23.6 | 23.6 | 23.6 | 20.6 |
| ezreal | 15200 | 18.2 | 18.0 | 18.4 | 21.8 |

#### random-player pass 0 both seats

304200 games: P1 152182 (50.0%), P2 152018 (50.0%), no winner 0, engine failure 0; mean turns 16.1, mean decisions 347

| list | games | win % | as P1 | as P2 | mean turns |
|---|---|---|---|---|---|
| daisy | 15200 | 93.2 | 93.4 | 93.0 | 13.4 |
| fiora | 15200 | 79.1 | 79.7 | 78.6 | 17.0 |
| zed | 15200 | 68.6 | 68.6 | 68.6 | 14.9 |
| rengar | 15200 | 66.2 | 66.0 | 66.3 | 14.8 |
| lucian | 15200 | 65.2 | 65.1 | 65.3 | 15.1 |
| poppy | 15200 | 65.1 | 65.1 | 65.1 | 15.7 |
| ivern | 15200 | 65.0 | 64.4 | 65.6 | 15.4 |
| rumble | 15200 | 62.6 | 62.5 | 62.8 | 16.3 |
| draven | 15200 | 62.0 | 61.9 | 62.1 | 15.5 |
| darius | 15200 | 61.9 | 62.1 | 61.7 | 15.6 |
| kennen | 15200 | 57.2 | 57.6 | 56.9 | 15.6 |
| sivir | 15200 | 56.7 | 56.6 | 56.8 | 15.9 |
| ahri | 15200 | 54.8 | 54.4 | 55.2 | 15.2 |
| vex | 15200 | 53.6 | 53.9 | 53.2 | 15.1 |
| vi | 15200 | 53.5 | 53.2 | 53.7 | 16.1 |
| khazix | 15200 | 53.3 | 53.1 | 53.5 | 14.8 |
| reksai | 15200 | 51.5 | 51.7 | 51.2 | 16.3 |
| yi_rebuild | 15200 | 49.3 | 49.7 | 48.9 | 14.8 |
| kaisa | 15200 | 48.6 | 48.1 | 49.0 | 17.2 |
| missfortune | 15200 | 48.5 | 48.2 | 48.8 | 15.2 |
| viktor | 15200 | 48.4 | 48.8 | 48.0 | 17.1 |
| lillia | 15200 | 47.7 | 48.0 | 47.3 | 16.1 |
| ambessa | 15200 | 47.3 | 47.1 | 47.5 | 15.6 |
| nasus | 15200 | 46.7 | 46.7 | 46.6 | 15.5 |
| teemo | 15200 | 44.9 | 44.4 | 45.4 | 16.3 |
| diana | 15200 | 44.5 | 44.7 | 44.2 | 16.3 |
| yasuao | 15200 | 43.7 | 43.6 | 43.7 | 15.4 |
| akali | 15200 | 42.2 | 42.6 | 41.8 | 16.7 |
| leblanc | 15200 | 42.2 | 42.7 | 41.6 | 17.4 |
| renata | 15200 | 39.9 | 40.4 | 39.4 | 17.3 |
| lux | 15200 | 39.2 | 39.3 | 39.2 | 18.2 |
| irelia | 15200 | 38.1 | 37.9 | 38.3 | 15.8 |
| azir | 15200 | 37.8 | 38.2 | 37.3 | 16.5 |
| ornn | 15200 | 37.2 | 37.6 | 36.9 | 15.7 |
| leona | 15200 | 35.2 | 35.2 | 35.2 | 15.9 |
| mel | 15200 | 32.1 | 31.9 | 32.2 | 17.0 |
| jayce | 15200 | 28.3 | 28.0 | 28.5 | 17.7 |
| pyke | 15200 | 26.2 | 26.3 | 26.2 | 17.1 |
| ezreal | 15200 | 12.7 | 12.6 | 12.7 | 18.5 |

#### random (P1) v random-player (P2)

304200 games: P1 178522 (58.7%), P2 125678 (41.3%), no winner 0, engine failure 0; mean turns 17.7, mean decisions 309

| list | games | win % | as P1 | as P2 | mean turns |
|---|---|---|---|---|---|
| daisy | 15200 | 82.1 | 90.0 | 74.1 | 15.1 |
| rengar | 15200 | 72.6 | 78.3 | 66.9 | 15.8 |
| fiora | 15200 | 72.2 | 75.7 | 68.7 | 19.0 |
| poppy | 15200 | 65.0 | 70.6 | 59.4 | 17.2 |
| ivern | 15200 | 63.8 | 71.7 | 55.9 | 16.7 |
| zed | 15200 | 62.5 | 72.2 | 52.9 | 16.6 |
| lucian | 15200 | 62.3 | 71.2 | 53.5 | 16.6 |
| draven | 15200 | 62.2 | 71.2 | 53.2 | 17.1 |
| missfortune | 15200 | 59.8 | 65.9 | 53.7 | 16.3 |
| rumble | 15200 | 59.3 | 67.8 | 50.7 | 18.0 |
| khazix | 15200 | 58.7 | 65.8 | 51.6 | 16.2 |
| yi_rebuild | 15200 | 54.9 | 59.7 | 50.0 | 16.3 |
| darius | 15200 | 54.8 | 65.2 | 44.5 | 17.3 |
| vex | 15200 | 53.7 | 62.4 | 45.0 | 16.6 |
| ahri | 15200 | 53.6 | 62.0 | 45.2 | 16.7 |
| kennen | 15200 | 52.0 | 63.7 | 40.2 | 17.2 |
| sivir | 15200 | 51.9 | 59.7 | 44.0 | 17.5 |
| reksai | 15200 | 51.7 | 61.2 | 42.2 | 17.8 |
| vi | 15200 | 51.2 | 62.9 | 39.6 | 17.7 |
| nasus | 15200 | 50.2 | 57.5 | 42.8 | 17.0 |
| kaisa | 15200 | 49.4 | 60.1 | 38.6 | 18.7 |
| ambessa | 15200 | 48.7 | 56.9 | 40.6 | 17.3 |
| viktor | 15200 | 48.7 | 61.3 | 36.1 | 18.6 |
| yasuao | 15200 | 46.7 | 56.7 | 36.8 | 16.9 |
| teemo | 15200 | 45.6 | 56.4 | 34.9 | 17.7 |
| akali | 15200 | 45.2 | 55.6 | 34.7 | 18.2 |
| leblanc | 15200 | 45.0 | 55.9 | 34.1 | 19.0 |
| diana | 15200 | 44.9 | 53.6 | 36.2 | 18.0 |
| irelia | 15200 | 43.5 | 53.5 | 33.4 | 17.5 |
| renata | 15200 | 42.1 | 53.4 | 30.9 | 19.0 |
| mel | 15200 | 40.3 | 50.6 | 30.1 | 18.7 |
| leona | 15200 | 38.2 | 45.9 | 30.5 | 17.6 |
| ornn | 15200 | 35.5 | 43.9 | 27.1 | 17.3 |
| lux | 15200 | 35.4 | 44.5 | 26.2 | 20.3 |
| pyke | 15200 | 34.8 | 43.4 | 26.1 | 18.7 |
| lillia | 15200 | 34.0 | 43.3 | 24.7 | 17.7 |
| azir | 15200 | 32.3 | 39.7 | 24.9 | 18.3 |
| jayce | 15200 | 25.9 | 32.9 | 18.9 | 19.6 |
| ezreal | 15200 | 19.4 | 25.5 | 13.3 | 20.8 |

#### random-player (P1) v random (P2)

304200 games: P1 125460 (41.2%), P2 178740 (58.8%), no winner 0, engine failure 0; mean turns 17.7, mean decisions 309

| list | games | win % | as P1 | as P2 | mean turns |
|---|---|---|---|---|---|
| daisy | 15200 | 81.9 | 73.9 | 89.8 | 15.1 |
| rengar | 15200 | 72.6 | 67.3 | 77.9 | 15.8 |
| fiora | 15200 | 72.0 | 68.6 | 75.4 | 19.0 |
| poppy | 15200 | 64.8 | 59.4 | 70.2 | 17.2 |
| ivern | 15200 | 63.7 | 56.1 | 71.3 | 16.7 |
| lucian | 15200 | 63.0 | 54.5 | 71.5 | 16.6 |
| zed | 15200 | 61.9 | 51.3 | 72.4 | 16.5 |
| draven | 15200 | 61.6 | 52.8 | 70.4 | 17.1 |
| rumble | 15200 | 59.5 | 50.8 | 68.2 | 18.0 |
| missfortune | 15200 | 59.5 | 52.9 | 66.1 | 16.4 |
| khazix | 15200 | 58.5 | 51.8 | 65.3 | 16.3 |
| yi_rebuild | 15200 | 54.7 | 48.7 | 60.7 | 16.3 |
| darius | 15200 | 54.6 | 44.5 | 64.6 | 17.3 |
| ahri | 15200 | 54.5 | 45.7 | 63.2 | 16.7 |
| vex | 15200 | 54.2 | 45.0 | 63.5 | 16.6 |
| kennen | 15200 | 52.3 | 40.9 | 63.8 | 17.2 |
| vi | 15200 | 51.6 | 40.2 | 62.9 | 17.7 |
| reksai | 15200 | 51.6 | 41.8 | 61.3 | 17.8 |
| sivir | 15200 | 51.1 | 43.0 | 59.2 | 17.5 |
| nasus | 15200 | 50.1 | 42.9 | 57.2 | 17.0 |
| viktor | 15200 | 49.6 | 38.1 | 61.1 | 18.7 |
| kaisa | 15200 | 49.3 | 39.3 | 59.3 | 18.7 |
| ambessa | 15200 | 48.7 | 40.3 | 57.0 | 17.3 |
| yasuao | 15200 | 46.4 | 35.2 | 57.6 | 17.0 |
| teemo | 15200 | 45.9 | 35.3 | 56.5 | 17.8 |
| leblanc | 15200 | 45.3 | 34.3 | 56.3 | 19.1 |
| diana | 15200 | 45.0 | 35.7 | 54.2 | 18.0 |
| akali | 15200 | 44.5 | 33.6 | 55.5 | 18.2 |
| renata | 15200 | 43.2 | 31.6 | 54.9 | 19.0 |
| irelia | 15200 | 42.9 | 33.8 | 52.0 | 17.4 |
| mel | 15200 | 39.7 | 29.3 | 50.1 | 18.7 |
| leona | 15200 | 37.9 | 30.6 | 45.2 | 17.6 |
| ornn | 15200 | 35.7 | 26.6 | 44.7 | 17.3 |
| lux | 15200 | 35.5 | 26.0 | 44.9 | 20.3 |
| pyke | 15200 | 34.9 | 26.2 | 43.7 | 18.7 |
| lillia | 15200 | 34.0 | 25.5 | 42.6 | 17.7 |
| azir | 15200 | 32.5 | 24.5 | 40.6 | 18.3 |
| jayce | 15200 | 26.2 | 18.7 | 33.6 | 19.5 |
| ezreal | 15200 | 19.1 | 12.5 | 25.8 | 20.8 |

Under random play these tables are an anomaly detector, not a strength measure:
- `decks/daisy.txt` wins 88% / 79% / 93%. It is an all-unit, low-curve list made for coverage, and random agents
  favour units.
- random beats random-player 58.7% / 58.8% in both seatings (round 1: 40.7% for random-player as P1). random-player
  spends its resources on whatever it can, including effects that help the opponent.

Not measured: first-player advantage. `--rows` does not record who won the coin toss; `tools/balance.cpp` (batch 5b,
T5) measures it.

**Honest ISMCTS-200, both seats** (`scripts/testing/round2_ismcts_rr.py`, seed base 2151030000, game g of pairing p
has seed base + 2p + g):
- **Stopped at 1,809 of the planned 3,042 games.** Pairings were played in sorted order, so `ahri` to `nasus` are
  covered as deck1 (2 games for each of their pairings) and every list as deck2 (46 to 61 games).
- No engine failure. P1 48.1%, P2 51.9%. Mean 14.8 turns and 13.1 s per game.
- Why it was stopped: the coordinator reported that this stream had kept the run lane busy for hours. My own
  drivers (fuzz, re-runs, replays and the round robin) took turns at the lane between the round robin's holds, so
  batch 7-perf waited. I judged 1,809 games enough for the purpose: engine errors under search, plus a rough
  balance per list.

#### honest ISMCTS-200 both seats

1809 games: P1 870 (48.1%), P2 939 (51.9%), no winner 0, engine failure 0; mean turns 14.8

| list | games | win % | as P1 | as P2 | mean turns |
|---|---|---|---|---|---|
| daisy | 122 | 89.3 | 85.5 | 95.7 | 13.0 |
| fiora | 120 | 69.2 | 72.4 | 63.6 | 15.5 |
| lucian | 120 | 68.3 | 64.5 | 75.0 | 14.1 |
| draven | 121 | 67.8 | 69.7 | 64.4 | 14.2 |
| teemo | 46 | 67.4 | 0.0 | 67.4 | 13.5 |
| poppy | 46 | 63.0 | 0.0 | 63.0 | 14.7 |
| rengar | 46 | 63.0 | 0.0 | 63.0 | 13.4 |
| kennen | 120 | 62.5 | 64.5 | 59.1 | 14.8 |
| ivern | 120 | 61.7 | 63.2 | 59.1 | 14.6 |
| missfortune | 120 | 59.2 | 57.9 | 61.4 | 13.4 |
| sivir | 46 | 58.7 | 0.0 | 58.7 | 15.5 |
| yasuao | 46 | 58.7 | 0.0 | 58.7 | 13.6 |
| khazix | 120 | 56.7 | 56.6 | 56.8 | 14.2 |
| lillia | 120 | 55.8 | 52.6 | 61.4 | 13.8 |
| yi_rebuild | 46 | 54.3 | 0.0 | 54.3 | 13.6 |
| ahri | 122 | 52.5 | 52.6 | 52.2 | 13.7 |
| darius | 122 | 52.5 | 51.3 | 54.3 | 14.3 |
| vex | 46 | 52.2 | 0.0 | 52.2 | 14.0 |
| viktor | 46 | 52.2 | 0.0 | 52.2 | 15.4 |
| nasus | 61 | 50.8 | 46.7 | 52.2 | 15.3 |
| zed | 46 | 50.0 | 0.0 | 50.0 | 13.7 |
| akali | 122 | 49.2 | 48.7 | 50.0 | 15.0 |
| rumble | 46 | 47.8 | 0.0 | 47.8 | 15.8 |
| diana | 122 | 45.9 | 48.7 | 41.3 | 15.0 |
| kaisa | 120 | 45.0 | 40.8 | 52.3 | 15.1 |
| lux | 120 | 45.0 | 46.1 | 43.2 | 16.6 |
| reksai | 46 | 43.5 | 0.0 | 43.5 | 15.6 |
| renata | 46 | 43.5 | 0.0 | 43.5 | 15.2 |
| ambessa | 122 | 42.6 | 38.2 | 50.0 | 14.9 |
| leona | 120 | 41.7 | 32.9 | 56.8 | 15.2 |
| leblanc | 120 | 40.8 | 36.8 | 47.7 | 14.3 |
| irelia | 120 | 39.2 | 36.8 | 43.2 | 14.6 |
| pyke | 46 | 39.1 | 0.0 | 39.1 | 16.2 |
| vi | 46 | 32.6 | 0.0 | 32.6 | 14.3 |
| mel | 120 | 30.8 | 27.6 | 36.4 | 15.3 |
| ornn | 46 | 28.3 | 0.0 | 28.3 | 15.2 |
| jayce | 120 | 25.0 | 18.4 | 36.4 | 15.7 |
| azir | 122 | 23.8 | 22.4 | 26.1 | 15.0 |
| ezreal | 120 | 23.3 | 21.1 | 27.3 | 17.1 |

Under search the order changes from random play. The bottom is Ezreal, Azir and Jayce, at 23-25%. Daisy stays on
top at 89%.

## 6. The 19 games read

Pairing: `scripts/testing/round2_pairing.py decks plan.tsv --seed 20261015`. This is the games19 method on a pool of
39 lists: the last name of the shuffled order is left over, here `kaisa`. Game k has engine seed 2026101500 + k, and
`ismcts:sims=200` is in both seats. None of the 19 pairings was played in round 1.

| # | deck1 (P1) | deck2 (P2) | seed | winner | score | turns | decisions |
|---|---|---|---|---|---|---|---|
| 1 | viktor | fiora | 2026101501 | P1 | 8-2 | 12 | 170 |
| 2 | vex | ezreal | 2026101502 | P1 | 8-6 | 15 | 313 |
| 3 | yasuao | kennen | 2026101503 | P2 | 1-8 | 10 | 163 |
| 4 | ivern | diana | 2026101504 | P1 | 8-6 | 14 | 340 |
| 5 | azir | mel | 2026101505 | P2 | 6-8 | 13 | 282 |
| 6 | leona | rengar | 2026101506 | P2 | 7-8 | 17 | 396 |
| 7 | irelia | daisy | 2026101507 | P1 | 8-7 | 15 | 314 |
| 8 | darius | vi | 2026101508 | P2 | 7-8 | 15 | 343 |
| 9 | teemo | lucian | 2026101509 | P2 | 4-8 | 14 | 261 |
| 10 | draven | khazix | 2026101510 | P2 | 3-8 | 11 | 169 |
| 11 | reksai | ornn | 2026101511 | P2 | 5-8 | 17 | 382 |
| 12 | poppy | nasus | 2026101512 | P1 | 8-3 | 15 | 322 |
| 13 | lux | missfortune | 2026101513 | P2 | 3-8 | 12 | 226 |
| 14 | lillia | sivir | 2026101514 | P1 | 8-7 | 16 | 309 |
| 15 | zed | pyke | 2026101515 | P1 | 8-6 | 16 | 279 |
| 16 | leblanc | yi_rebuild | 2026101516 | P1 | 8-2 | 11 | 211 |
| 17 | akali | ambessa | 2026101517 | P1 | 8-4 | 16 | 271 |
| 18 | ahri | rumble | 2026101518 | P2 | 6-8 | 15 | 300 |
| 19 | renata | jayce | 2026101519 | P1 | 8-2 | 14 | 277 |

All 19 ended with a winner at 8 points, exit code 0. Replay a game with:

`RIFTBOUND_ROOT=. ./build/riftbound --deck1 decks/<a>.txt --deck2 decks/<b>.txt --agent1 ismcts:sims=200 --agent2 ismcts:sims=200 --seed <seed> --trace --debug`

To get the prompts too, run `./build/prompt_trace` with the same arguments.
`scripts/testing/round2_prompt_runs.py` checked that the two print the same trace lines for all 19 games.

**How they were read:**
- `run_deck_games.py` produced the logs and HTML replays, and `games19_reading.py` the reading files.
- `round2_add_prompts.py` put each decision's prompt description into the reading files. Phase-seam snapshots repeat
  a decision index and get none.
- Each reader had a brief, its game's card sheet, all 955 cards, the rules, CLAUDE.md, and the open and closed item
  lists. Readers ran three at a time.
- Every reader reports every SNAP and the log tail read: 5,328 decisions in all, with the setup decisions #1 and #2
  read from the prompt log.

**Checked and found correct, notable:**
- Fixed items that held: G1 (both players draw in their first Draw Phase), G10, G11, G12, G14, G15, G16, G18, G19,
  G24, D1, D3, D5, D11, D15, D21, D35, D37, D40, D43, D45, D48, D58, D72, D78.
- The winning-point rule in 12 games, including blocked conquers that drew instead (g08, g14, g17).
- Hidden plays bound to their battlefield (g05, g07, g14, g18).
- Zhonya's Hourglass replacing deaths (g07, g17).
- Vilemaw's damage rule in six combats (g12).
- Bashful Bloom's cost (g14).
- Sandswept Tomb's discount (g10, g12).
- Every combat-damage option list that was checked was complete.

**Known open items seen:**
- G28 / L1 (agent quality) in 17 games: helpful effects put on enemy units, own spells countered, turns ended with
  runes ready.
- L3 (setup battlefield not searched) in 17 games.
- Judge question 1 (readying an opponent's runes) taken in g04, g11, g12, g19.
- D28 (pinned) seen 6 times in g08.
- K2, P1, Q1, Q2, S1 could not be observed.

## 7. Flow focus (`decks/kennen.txt`)

Material: 44 traced games, made with `round2_prompt_runs.py` and `prompt_trace`.
- 38 games: Kennen with `random-player:pass=0` against every other list with `random-player`.
- 6 games: honest ISMCTS-200 both seats.
- Seeds 2151040001-044, plan in the scratch directory.

One reader went through all 104 `TRASH_REPLAY` lines and all 31 Kennen grants.

| spell | Flow | plays | paid | banished | countered |
|---|---|---|---|---|---|
| Lightning Rush | printed [2][A] | 64 | [2] + 1 any, 64 of 64 | yes | 2 |
| Up from the Deep | printed [3] | 26 | [3], 26 of 26 | yes | 1 |
| Dredge Up | printed [2] | 3 | [2] | yes | 0 |
| Death Mark 931 | printed [1][A][A] | 2 | [1] + 2 any | yes | 0 |
| Perfect Execution | printed [3][R] | 1 | [3] + 1 | yes | 0 |
| Gust, Hard Bargain (with its Repeat), Flash, Star-Crossed, Ride the Wind | Kennen's grant (equal to cost) | 8 | own cost | yes | 0 |

Correct throughout:
- The grant is chosen as the ability is finalized, from the controller's own trash spells (D78 held).
- A granted spell is offered only on the grant's turn (24 of 24).
- Each spell is offered from the trash only, once, at its own timing: Action in the main phase, Reaction also at
  chain priority.
- Targets are chosen before the cost.
- A spell is banished after its play, also the 3 countered ones (`-> banished (played for Flow)`).
- Heart of the Tempest fired after every resolved Flow play and for none of the countered ones (G11 and G12 held).
- Fizz, Trickster's trash plays recycle the spell instead of banishing it.

Gaps: U13 and U18; U19 (the log wording).

## 8. Judge questions to add

- U20 (a) and (b).
- U17: should copies of one ability be ordered?
- U11, if the project wants confirmation that 355.1.a puts the Repeat decision before the base cost.

## Appendix A. Per-card coverage, pass 1

Optimised millions: random / random-player / random-player:pass=0, with the new counter. For each: games in play /
triggered / activated / resolved, "-" where the way does not apply. The list runs are the three optimised list runs
together.

| id | card | type | random: play / trig / act / res | random-player | pass=0 | 3 lists runs (912,600 games) | below |
|---|---|---|---|---|---|---|---|
| 1 | Blazing Scorcher | Unit | 18872 / - / - / - | 18571 / - / - / - | 18468 / - / - / - | 0 / - / - / - |  |
| 2 | Brazen Buccaneer | Unit | 20829 / - / - / - | 19520 / - / - / - | 20996 / - / - / - | 0 / - / - / - |  |
| 3 | Chemtech Enforcer | Unit | 24132 / 24132 / - / 24132 | 22753 / 22753 / - / 22753 | 26806 / 26806 / - / 26806 | 0 / 0 / - / 0 |  |
| 4 | Cleave | Spell | 27315 / - / - / 27133 | 27280 / - / - / 26989 | 28425 / - / - / 28219 | 148486 / - / - / 145041 |  |
| 5 | Disintegrate | Spell | 25118 / 12950 / - / 24863 | 23410 / 12542 / - / 23061 | 23142 / 12487 / - / 22920 | 0 / 0 / - / 0 |  |
| 6 | Flame Chompers | Unit | 23948 / 736 / - / 736 | 22080 / 1230 / - / 1230 | 23485 / 1652 / - / 1652 | 0 / 0 / - / 0 |  |
| 7 | Fury Rune | Rune | 549415 / - / - / - | 542336 / - / - / - | 543777 / - / - / - | 442200 / - / - / - |  |
| 8 | Get Excited! | Spell | 25885 / - / - / 25594 | 27284 / - / - / 26950 | 26184 / - / - / 25931 | 0 / - / - / 0 |  |
| 9 | Hextech Ray | Spell | 25712 / - / - / 25473 | 26058 / - / - / 25740 | 27031 / - / - / 26813 | 52079 / - / - / 50860 |  |
| 10 | Legion Rearguard | Unit | 24179 / - / - / - | 23846 / - / - / - | 25782 / - / - / - | 30871 / - / - / - |  |
| 11 | Magma Wurm | Unit | 13853 / - / - / - | 12333 / - / - / - | 12229 / - / - / - | 0 / - / - / - |  |
| 12 | Noxus Hopeful | Unit | 23983 / - / - / - | 23143 / - / - / - | 25133 / - / - / - | 203284 / - / - / - |  |
| 13 | Pouty Poro | Unit | 24377 / - / - / - | 23948 / - / - / - | 25595 / - / - / - | 0 / - / - / - |  |
| 14 | Sky Splitter | Spell | 25822 / - / - / 25548 | 24358 / - / - / 24050 | 25889 / - / - / 25657 | 0 / - / - / 0 |  |
| 15 | Captain Farron | Unit | 21031 / - / - / - | 19319 / - / - / - | 21235 / - / - / - | 0 / - / - / - |  |
| 16 | Dangerous Duo | Unit | 22096 / 10250 / - / 10239 | 22014 / 9780 / - / 9769 | 23235 / 10142 / - / 10127 | 0 / 0 / - / 0 |  |
| 17 | Iron Ballista | Gear | 19996 / - / 13954 / 13932 | 20158 / - / 13991 / 13967 | 21722 / - / 18196 / 18188 | 0 / - / 0 / 0 |  |
| 18 | Noxus Saboteur | Unit | 22674 / - / - / - | 20565 / - / - / - | 24644 / - / - / - | 0 / - / - / - |  |
| 19 | Raging Soul | Unit | 21978 / - / - / - | 18580 / - / - / - | 21540 / - / - / - | 0 / - / - / - |  |
| 20 | Scrapyard Champion | Unit | 18881 / 6271 / - / 6271 | 17467 / 5845 / - / 5845 | 18648 / 5158 / - / 5158 | 0 / 0 / - / 0 |  |
| 21 | Sun Disc | Gear | 22407 / 5028 / 19583 / 19583 | 21552 / 4642 / 19368 / 19368 | 23806 / 7140 / 23579 / 23579 | 0 / 0 / 0 / 0 |  |
| 22 | Thermo Beam | Spell | 20832 / - / - / 20666 | 22661 / - / - / 22476 | 20516 / - / - / 20395 | 0 / - / - / 0 |  |
| 23 | Unlicensed Armory | Gear | 23874 / - / 19424 / 19424 | 22116 / - / 18764 / 18764 | 25810 / - / 23593 / 23593 | 0 / - / 0 / 0 |  |
| 24 | Void Seeker | Spell | 25164 / - / - / 24879 | 24482 / - / - / 24127 | 23770 / - / - / 23532 | 27512 / - / - / 26102 |  |
| 25 | Blind Fury | Spell | 22544 / - / - / 22400 | 23097 / - / - / 22881 | 21887 / - / - / 21751 | 0 / - / - / 0 |  |
| 26 | Brynhir Thundersong | Unit | 19135 / 19135 / - / 19135 | 15547 / 15547 / - / 15547 | 17074 / 17074 / - / 17074 | 64690 / 64690 / - / 64690 |  |
| 27 | Darius, Trifarian | Unit | 30206 / 25219 / - / 25219 | 28590 / 23573 / - / 23573 | 29913 / 23687 / - / 23687 | 71116 / 58175 / - / 58175 |  |
| 28 | Draven, Showboat | Unit | 35414 / - / - / - | 32609 / - / - / - | 35224 / - / - / - | 37205 / - / - / - |  |
| 29 | Falling Star | Spell | 20620 / - / - / 20452 | 20243 / - / - / 20009 | 22999 / - / - / 22831 | 191370 / - / - / 189969 |  |
| 30 | Jinx, Demolitionist | Unit | 40217 / 40217 / - / 40217 | 37617 / 37617 / - / 37617 | 41860 / 41860 / - / 41860 | 0 / 0 / - / 0 |  |
| 31 | Raging Firebrand | Unit | 17287 / 17287 / - / 17287 | 15533 / 15533 / - / 15533 | 16856 / 16856 / - / 16856 | 0 / 0 / - / 0 |  |
| 32 | Ravenborn Tome | Gear | 19724 / - / 16862 / 16862 | 19806 / - / 17438 / 17438 | 21950 / - / 21696 / 21696 | 0 / - / 0 / 0 |  |
| 33 | Shakedown | Spell | 25818 / - / - / 25509 | 27822 / - / - / 27462 | 28075 / - / - / 27836 | 0 / - / - / 0 |  |
| 34 | Tryndamere, Barbarian | Unit | 16070 / 1696 / - / 1695 | 13652 / 1438 / - / 1438 | 13849 / 1889 / - / 1889 | 0 / 0 / - / 0 |  |
| 35 | Vayne, Hunter | Unit | 20430 / 4074 / - / 4073 | 18562 / 5902 / - / 5902 | 20941 / 8871 / - / 8869 | 0 / 0 / - / 0 |  |
| 36 | Vi, Destructive | Unit | 35772 / - / 23562 / 23562 | 35385 / - / 24798 / 24798 | 37747 / - / 32077 / 32077 | 0 / - / 0 / 0 |  |
| 37 | Immortal Phoenix | Unit | 22216 / 1800 / - / 1800 | 20655 / 2482 / - / 2482 | 23925 / 3540 / - / 3540 | 0 / 0 / - / 0 |  |
| 38 | Kadregrin the Infernal | Unit | 11998 / 11998 / - / 11998 | 10346 / 10346 / - / 10346 | 8980 / 8980 / - / 8980 | 0 / 0 / - / 0 |  |
| 39 | Kai'Sa, Survivor | Unit | 40222 / 19067 / - / 19067 | 36105 / 19159 / - / 19158 | 38151 / 23535 / - / 23534 | 231561 / 124928 / - / 124866 |  |
| 40 | Seal of Rage | Gear | 23922 / - / 23342 / 23342 | 26243 / - / 25926 / 25926 | 27092 / - / 27089 / 27089 | 0 / - / 0 / 0 |  |
| 41 | Volibear, Furious | Unit | 18897 / 4755 / - / 4728 | 16136 / 3649 / - / 3636 | 16488 / 4254 / - / 4221 | 0 / 0 / - / 0 |  |
| 42 | Calm Rune | Rune | 548319 / - / - / - | 549756 / - / - / - | 554295 / - / - / - | 507000 / - / - / - |  |
| 43 | Charm | Spell | 23257 / - / - / 22759 | 22838 / - / - / 22268 | 24505 / - / - / 24032 | 184440 / - / - / 167010 |  |
| 44 | Clockwork Keeper | Unit | 26442 / 12884 / - / 12884 | 24484 / 17263 / - / 17263 | 25495 / 20764 / - / 20764 | 0 / 0 / - / 0 |  |
| 45 | Defy | Spell | 18310 / - / - / 18180 | 22188 / - / - / 22026 | 21399 / - / - / 21307 | 346531 / - / - / 345711 |  |
| 46 | En Garde | Spell | 26130 / - / - / 25540 | 28900 / - / - / 28088 | 28327 / - / - / 27589 | 158439 / - / - / 144041 |  |
| 47 | Find Your Center | Spell | 27279 / - / - / 26672 | 27028 / - / - / 26328 | 27666 / - / - / 27000 | 0 / - / - / 0 |  |
| 48 | Meditation | Spell | 27394 / - / - / 26920 | 27037 / - / - / 26413 | 28279 / - / - / 27665 | 0 / - / - / 0 |  |
| 49 | Playful Phantom | Unit | 21343 / - / - / - | 19409 / - / - / - | 21072 / - / - / - | 0 / - / - / - |  |
| 50 | Rune Prison | Spell | 27029 / - / - / 26431 | 28105 / - / - / 27326 | 28337 / - / - / 27536 | 0 / - / - / 0 |  |
| 51 | Solari Shieldbearer | Unit | 23457 / 23457 / - / 23434 | 23028 / 23028 / - / 22998 | 25280 / 25280 / - / 25254 | 0 / 0 / - / 0 |  |
| 52 | Stalwart Poro | Unit | 24707 / - / - / - | 25145 / - / - / - | 26059 / - / - / - | 64585 / - / - / - |  |
| 53 | Stand United | Spell | 26017 / - / - / 25447 | 26691 / - / - / 26033 | 27212 / - / - / 26600 | 0 / - / - / 0 |  |
| 54 | Sunlit Guardian | Unit | 23027 / - / - / - | 22948 / - / - / - | 25243 / - / - / - | 0 / - / - / - |  |
| 55 | Wielder of Water | Unit | 22977 / - / - / - | 23412 / - / - / - | 26302 / - / - / - | 0 / - / - / - |  |
| 56 | Adaptatron | Unit | 21820 / 3213 / - / 3207 | 20507 / 5097 / - / 5084 | 24033 / 8608 / - / 8596 | 14690 / 4363 / - / 4358 |  |
| 57 | Block | Spell | 27656 / - / - / 27032 | 27155 / - / - / 26407 | 26862 / - / - / 26172 | 38767 / - / - / 31816 |  |
| 58 | Discipline | Spell | 26217 / - / - / 25632 | 27547 / - / - / 26742 | 27409 / - / - / 26585 | 299397 / - / - / 271607 |  |
| 59 | Eclipse Herald | Unit | 17680 / 919 / - / 919 | 15404 / 879 / - / 879 | 16457 / 882 / - / 882 | 0 / 0 / - / 0 |  |
| 60 | Mask of Foresight | Gear | 23254 / 15298 / - / 15298 | 23153 / 12556 / - / 12556 | 25286 / 16508 / - / 16508 | 33640 / 24684 / - / 24684 |  |
| 61 | Poro Herder | Unit | 23735 / 2442 / - / 2442 | 22172 / 2189 / - / 2189 | 25892 / 2493 / - / 2493 | 0 / 0 / - / 0 |  |
| 62 | Reinforce | Spell | 18036 / - / - / 17787 | 16648 / - / - / 16311 | 18552 / - / - / 18208 | 11128 / - / - / 11084 |  |
| 63 | Spirit's Refuge | Gear | 23635 / 17837 / - / 17837 | 22674 / 16412 / - / 16412 | 26217 / 19094 / - / 19094 | 0 / 0 / - / 0 |  |
| 64 | Wind Wall | Spell | 14389 / - / - / 14308 | 18871 / - / - / 18735 | 15311 / - / - / 15250 | 0 / - / - / 0 |  |
| 65 | Wizened Elder | Unit | 23881 / - / - / - | 20767 / - / - / - | 23999 / - / - / - | 0 / - / - / - |  |
| 66 | Ahri, Alluring | Unit | 32777 / 15562 / - / 15551 | 28723 / 13782 / - / 13771 | 32428 / 14357 / - / 14344 | 13303 / 5079 / - / 5079 |  |
| 67 | Blitzcrank, Impassive | Unit | 20859 / 10400 / - / 10381 | 19787 / 10571 / - / 10554 | 21243 / 11394 / - / 11365 | 49731 / 24954 / - / 24928 |  |
| 68 | Caitlyn, Patrolling | Unit | 23841 / - / 5315 / 5307 | 22685 / - / 4247 / 4243 | 25034 / - / 5878 / 5875 | 0 / - / 0 / 0 |  |
| 69 | Last Stand | Spell | 25446 / - / - / 24854 | 24624 / - / - / 23951 | 26158 / - / - / 25427 | 0 / - / - / 0 |  |
| 70 | Mageseeker Warden | Unit | 19281 / - / - / - | 16900 / - / - / - | 19427 / - / - / - | 0 / - / - / - |  |
| 71 | Party Favors | Spell | 21063 / - / - / 20563 | 20518 / - / - / 19919 | 24492 / - / - / 23808 | 0 / - / - / 0 |  |
| 72 | Solari Shrine | Gear | 22385 / 351 / - / 351 | 21274 / 588 / - / 587 | 23880 / 928 / - / 928 | 0 / 0 / - / 0 |  |
| 73 | Sona, Harmonious | Unit | 23215 / 16684 / - / 16684 | 20392 / 14379 / - / 14379 | 23239 / 17021 / - / 17021 | 14389 / 9426 / - / 9426 |  |
| 74 | Taric, Protector | Unit | 23227 / - / - / - | 20441 / - / - / - | 23181 / - / - / - | 0 / - / - / - |  |
| 75 | Tasty Faefolk | Unit | 17992 / 6885 / - / 6884 | 15407 / 5410 / - / 5410 | 17008 / 7281 / - / 7278 | 17783 / 6936 / - / 6936 |  |
| 76 | Yasuo, Remorseful | Unit | 26812 / 9278 / - / 9228 | 24272 / 7366 / - / 7308 | 26199 / 10013 / - / 9932 | 0 / 0 / - / 0 |  |
| 77 | Zhonya's Hourglass | Gear | 25284 / - / - / - | 24515 / - / - / - | 28502 / - / - / - | 131833 / - / - / - |  |
| 78 | Lee Sin, Ascetic | Unit | 36791 / - / 14935 / 14935 | 32494 / - / 10727 / 10727 | 37150 / - / 15003 / 15003 | 0 / - / 0 / 0 |  |
| 79 | Leona, Zealot | Unit | 33966 / - / - / - | 30652 / - / - / - | 33336 / - / - / - | 0 / - / - / - |  |
| 80 | Mystic Reversal | Spell | 11794 / - / - / 11702 | 16344 / - / - / 16202 | 10547 / - / - / 10490 | 0 / - / - / 0 |  |
| 81 | Seal of Focus | Gear | 26815 / - / 26253 / 26253 | 25781 / - / 25422 / 25422 | 27241 / - / 27238 / 27238 | 0 / - / 0 / 0 |  |
| 82 | Whiteflame Protector | Unit | 14738 / 14738 / - / 14719 | 13487 / 13487 / - / 13453 | 13423 / 13423 / - / 13392 | 8233 / 8233 / - / 8198 |  |
| 83 | Consult the Past | Spell | 26868 / - / - / 26482 | 27224 / - / - / 26794 | 24932 / - / - / 24592 | 91763 / - / - / 88330 |  |
| 84 | Eager Apprentice | Unit | 25052 / - / - / - | 22122 / - / - / - | 25879 / - / - / - | 0 / - / - / - |  |
| 85 | Falling Comet | Spell | 24416 / - / - / 24067 | 26405 / - / - / 25925 | 24774 / - / - / 24395 | 0 / - / - / 0 |  |
| 86 | Jeweled Colossus | Unit | 21000 / - / - / - | 18891 / - / - / - | 20106 / - / - / - | 0 / - / - / - |  |
| 87 | Lecturing Yordle | Unit | 25219 / 25219 / - / 25219 | 22599 / 22599 / - / 22599 | 24581 / 24581 / - / 24581 | 34486 / 34486 / - / 34486 |  |
| 88 | Mega-Mech | Unit | 17020 / - / - / - | 15841 / - / - / - | 15453 / - / - / - | 0 / - / - / - |  |
| 89 | Mind Rune | Rune | 548934 / - / - / - | 546638 / - / - / - | 542394 / - / - / - | 567000 / - / - / - |  |
| 90 | Orb of Regret | Gear | 25721 / - / 24928 / 24923 | 25262 / - / 23830 / 23819 | 28053 / - / 27757 / 27751 | 0 / - / 0 / 0 |  |
| 91 | Pit Crew | Unit | 24579 / 9340 / - / 9340 | 22226 / 8597 / - / 8597 | 25086 / 9932 / - / 9932 | 0 / 0 / - / 0 |  |
| 92 | Riptide Rex | Unit | 18607 / 11592 / - / 11544 | 17382 / 10279 / - / 10191 | 17617 / 11103 / - / 11018 | 0 / 0 / - / 0 |  |
| 93 | Smoke Screen | Spell | 30741 / - / - / 30287 | 29454 / - / - / 28864 | 28158 / - / - / 27703 | 79926 / - / - / 76543 |  |
| 94 | Sprite Call | Spell | 26168 / - / - / 25823 | 27303 / - / - / 26910 | 26834 / - / - / 26520 | 63700 / - / - / 60054 |  |
| 95 | Stupefy | Spell | 28677 / - / - / 28228 | 29112 / - / - / 28617 | 28549 / - / - / 28092 | 360171 / - / - / 347357 |  |
| 96 | Watchful Sentry | Unit | 24511 / 16560 / - / 16559 | 22736 / 14190 / - / 14190 | 26689 / 19529 / - / 19525 | 98122 / 80584 / - / 80584 |  |
| 97 | Blastcone Fae | Unit | 26534 / 26534 / - / 26518 | 25148 / 25148 / - / 25114 | 26579 / 26579 / - / 26560 | 0 / 0 / - / 0 |  |
| 98 | Energy Conduit | Gear | 21202 / - / 20618 / 20618 | 19918 / - / 19539 / 19539 | 24670 / - / 24664 / 24664 | 0 / - / 0 / 0 |  |
| 99 | Garbage Grabber | Gear | 24107 / - / 15925 / 15925 | 23713 / - / 14689 / 14689 | 26239 / - / 21841 / 21841 | 28875 / - / 23201 / 23201 |  |
| 100 | Gemcraft Seer | Unit | 24363 / - / - / - | 22385 / - / - / - | 24377 / - / - / - | 0 / - / - / - |  |
| 101 | Mushroom Pouch | Gear | 24166 / 1223 / - / 1223 | 22484 / 1193 / - / 1193 | 25784 / 615 / - / 615 | 0 / 0 / - / 0 |  |
| 102 | Portal Rescue | Spell | 27177 / - / - / 26736 | 26670 / - / - / 26166 | 24748 / - / - / 24361 | 0 / - / - / 0 |  |
| 103 | Ravenbloom Student | Unit | 26526 / 14308 / - / 14308 | 24746 / 13605 / - / 13605 | 26674 / 13692 / - / 13692 | 131052 / 106768 / - / 106768 |  |
| 104 | Retreat | Spell | 28732 / - / - / 28300 | 29377 / - / - / 28911 | 27509 / - / - / 27102 | 0 / - / - / 0 |  |
| 105 | Singularity | Spell | 26854 / - / - / 26577 | 28538 / - / - / 28081 | 24914 / - / - / 24628 | 170172 / - / - / 169139 |  |
| 106 | Sprite Mother | Unit | 22996 / 22996 / - / 22996 | 19320 / 19320 / - / 19320 | 22333 / 22333 / - / 22333 | 15730 / 15730 / - / 15730 |  |
| 107 | Ava Achiever | Unit | 21603 / 471 / - / 471 | 18745 / 661 / - / 661 | 20194 / 719 / - / 719 | 0 / 0 / - / 0 |  |
| 108 | Convergent Mutation | Spell | 26536 / - / - / 26148 | 27509 / - / - / 27028 | 26141 / - / - / 25747 | 0 / - / - / 0 |  |
| 109 | Dr. Mundo, Expert | Unit | 15033 / 11322 / - / 11322 | 13180 / 9829 / - / 9826 | 12986 / 9614 / - / 9614 | 0 / 0 / - / 0 |  |
| 110 | Ekko, Recurrent | Unit | 0 / 0 / - / 0 | 0 / 0 / - / 0 | 0 / 0 / - / 0 | 0 / 0 / - / 0 | banned |
| 111 | Heimerdinger, Inventor | Unit | 22999 / - / - / - | 22307 / - / - / - | 23722 / - / - / - | 0 / - / - / - |  |
| 112 | Kai'Sa, Evolutionary | Unit | 33241 / 4876 / - / 4876 | 31852 / 7218 / - / 7214 | 33006 / 11437 / - / 11434 | 0 / 0 / - / 0 |  |
| 113 | Malzahar, Fanatic | Unit | 22541 / - / 10162 / 10162 | 20565 / - / 9891 / 9891 | 21760 / - / 8277 / 8277 | 0 / - / 0 / 0 |  |
| 114 | Progress Day | Spell | 15792 / - / - / 15597 | 14450 / - / - / 14255 | 15116 / - / - / 14920 | 41840 / - / - / 41725 |  |
| 115 | Promising Future | Spell | 17455 / - / - / 17249 | 15804 / - / - / 15549 | 17384 / - / - / 17115 | 28037 / - / - / 27989 |  |
| 116 | Thousand-Tailed Watcher | Unit | 16162 / 16162 / - / 16162 | 15304 / 15304 / - / 15304 | 15409 / 15409 / - / 15409 | 175664 / 175664 / - / 175660 |  |
| 117 | Viktor, Innovator | Unit | 40183 / 5814 / - / 5814 | 34098 / 7209 / - / 7209 | 39235 / 4650 / - / 4650 | 0 / 0 / - / 0 |  |
| 118 | Wraith of Echoes | Unit | 18604 / 11483 / - / 11480 | 17790 / 9926 / - / 9926 | 17797 / 10556 / - / 10553 | 0 / 0 / - / 0 |  |
| 119 | Ahri, Inquisitive | Unit | 35820 / 23349 / - / 23233 | 34206 / 19523 / - / 19354 | 36317 / 25304 / - / 25135 | 41322 / 26228 / - / 25933 |  |
| 120 | Seal of Insight | Gear | 25369 / - / 24756 / 24756 | 25992 / - / 25674 / 25674 | 28793 / - / 28789 / 28789 | 41117 / - / 40889 / 40889 |  |
| 121 | Teemo, Strategist | Unit | 44692 / 18442 / - / 18330 | 43289 / 16294 / - / 16124 | 46274 / 23314 / - / 23081 | 44666 / 20091 / - / 19838 |  |
| 122 | Time Warp | Spell | 7182 / - / - / 7121 | 6492 / - / - / 6406 | 5239 / - / - / 5204 | 31436 / - / - / 31289 |  |
| 123 | Unchecked Power | Spell | 13268 / - / - / 13110 | 12414 / - / - / 12232 | 12011 / - / - / 11866 | 0 / - / - / 0 |  |
| 124 | Arena Bar | Gear | 21503 / - / 18555 / 18555 | 21287 / - / 18175 / 18175 | 24886 / - / 24035 / 24035 | 0 / - / 0 / 0 |  |
| 125 | Bilgewater Bully | Unit | 18569 / - / - / - | 15994 / - / - / - | 18869 / - / - / - | 0 / - / - / - |  |
| 126 | Body Rune | Rune | 568999 / - / - / - | 572815 / - / - / - | 573078 / - / - / - | 408000 / - / - / - |  |
| 127 | Cannon Barrage | Spell | 26660 / - / - / 26310 | 28953 / - / - / 28515 | 26340 / - / - / 26050 | 0 / - / - / 0 |  |
| 128 | Challenge | Spell | 24746 / - / - / 24357 | 25868 / - / - / 25240 | 25128 / - / - / 24599 | 55503 / - / - / 51384 |  |
| 129 | Confront | Spell | 25632 / - / - / 25358 | 27876 / - / - / 27475 | 25964 / - / - / 25669 | 0 / - / - / 0 |  |
| 130 | Crackshot Corsair | Unit | 23969 / 7893 / - / 7842 | 23306 / 7115 / - / 7055 | 26092 / 10333 / - / 10271 | 0 / 0 / - / 0 |  |
| 131 | Dune Drake | Unit | 19992 / 6518 / - / 6518 | 19259 / 5591 / - / 5591 | 21093 / 7919 / - / 7919 | 0 / 0 / - / 0 |  |
| 132 | First Mate | Unit | 24088 / 22059 / - / 22030 | 24737 / 22535 / - / 22483 | 25528 / 23435 / - / 23399 | 153886 / 146838 / - / 146634 |  |
| 133 | Flurry of Blades | Spell | 28699 / - / - / 28395 | 29156 / - / - / 28798 | 26588 / - / - / 26268 | 63946 / - / - / 61353 |  |
| 134 | Mobilize | Spell | 23377 / - / - / 23107 | 22419 / - / - / 22122 | 25293 / - / - / 25055 | 67627 / - / - / 65458 |  |
| 135 | Pakaa Cub | Unit | 24881 / - / - / - | 25058 / - / - / - | 26476 / - / - / - | 14074 / - / - / - |  |
| 136 | Pit Rookie | Unit | 26371 / 19949 / - / 19949 | 24603 / 17501 / - / 17501 | 28092 / 19695 / - / 19695 | 86175 / 69557 / - / 69557 |  |
| 137 | Stormclaw Ursine | Unit | 17635 / 17635 / - / 17635 | 14817 / 14817 / - / 14817 | 17708 / 17708 / - / 17708 | 0 / 0 / - / 0 |  |
| 138 | Catalyst of Aeons | Spell | 20089 / - / - / 19761 | 18452 / - / - / 18156 | 20640 / - / - / 20337 | 30851 / - / - / 30361 |  |
| 139 | Cithria of Cloudfield | Unit | 25260 / 19371 / - / 19371 | 25986 / 19693 / - / 19693 | 27707 / 20610 / - / 20610 | 0 / 0 / - / 0 |  |
| 140 | Herald of Scales | Unit | 22476 / - / - / - | 21255 / - / - / - | 22784 / - / - / - | 0 / - / - / - |  |
| 141 | Kinkou Monk | Unit | 22112 / 22112 / - / 22112 | 20997 / 20997 / - / 20997 | 22398 / 22398 / - / 22398 | 0 / 0 / - / 0 |  |
| 142 | Mountain Drake | Unit | 13414 / - / - / - | 12514 / - / - / - | 11306 / - / - / - | 0 / - / - / - |  |
| 143 | Pirate's Haven | Gear | 21907 / 2405 / - / 2405 | 21062 / 2572 / - / 2572 | 23360 / 3757 / - / 3757 | 0 / 0 / - / 0 |  |
| 144 | Spoils of War | Spell | 26174 / - / - / 25845 | 25964 / - / - / 25561 | 25789 / - / - / 25475 | 15300 / - / - / 14816 |  |
| 145 | Unyielding Spirit | Spell | 28605 / - / - / 28274 | 28760 / - / - / 28378 | 27497 / - / - / 27153 | 0 / - / - / 0 |  |
| 146 | Wallop | Spell | 29652 / - / - / 29195 | 29103 / - / - / 28609 | 27879 / - / - / 27457 | 0 / - / - / 0 |  |
| 147 | Wildclaw Shaman | Unit | 21327 / 1847 / - / 1847 | 20754 / 2557 / - / 2557 | 22935 / 4099 / - / 4099 | 0 / 0 / - / 0 |  |
| 148 | Anivia, Primal | Unit | 16355 / 4973 / - / 4973 | 15112 / 3941 / - / 3941 | 16589 / 5599 / - / 5599 | 0 / 0 / - / 0 |  |
| 149 | Carnivorous Snapvine | Unit | 21043 / 12103 / - / 12029 | 18740 / 10132 / - / 10057 | 20895 / 12472 / - / 12395 | 0 / 0 / - / 0 |  |
| 150 | Kraken Hunter | Unit | 23528 / - / - / - | 22658 / - / - / - | 23316 / - / - / - | 0 / - / - / - |  |
| 151 | Lee Sin, Centered | Unit | 34281 / - / - / - | 30780 / - / - / - | 33811 / - / - / - | 0 / - / - / - |  |
| 152 | Mistfall | Gear | 22475 / 3881 / - / 3881 | 20966 / 4953 / - / 4953 | 22853 / 6832 / - / 6832 | 0 / 0 / - / 0 |  |
| 153 | Overt Operation | Spell | 22552 / - / - / 22297 | 24230 / - / - / 23889 | 21274 / - / - / 21062 | 0 / - / - / 0 |  |
| 154 | Primal Strength | Spell | 25956 / - / - / 25555 | 28404 / - / - / 27847 | 26049 / - / - / 25655 | 0 / - / - / 0 |  |
| 155 | Qiyana, Victorious | Unit | 22945 / 8787 / - / 8787 | 20359 / 8379 / - / 8378 | 22851 / 11065 / - / 11061 | 0 / 0 / - / 0 |  |
| 156 | Sabotage | Spell | 24154 / - / - / 23885 | 24540 / - / - / 24217 | 25669 / - / - / 25452 | 222428 / - / - / 216169 |  |
| 157 | Udyr, Wildman | Unit | 18778 / - / 1161 / 1160 | 17060 / - / 1150 / 1147 | 18774 / - / 1930 / 1929 | 0 / - / 0 / 0 |  |
| 158 | Volibear, Imposing | Unit | 10431 / 2552 / - / 2552 | 11056 / 2357 / - / 2356 | 8720 / 2122 / - / 2122 | 0 / 0 / - / 0 |  |
| 159 | Warwick, Hunter | Unit | 18544 / 7144 / - / 7144 | 17107 / 6020 / - / 6020 | 19589 / 8927 / - / 8927 | 0 / 0 / - / 0 |  |
| 160 | Dazzling Aurora | Gear | 9738 / 9513 / - / 9513 | 8983 / 8786 / - / 8786 | 9691 / 9453 / - / 9453 | 44291 / 43950 / - / 43950 |  |
| 161 | Deadbloom Predator | Unit | 17932 / - / - / - | 15491 / - / - / - | 14503 / - / - / - | 0 / - / - / - |  |
| 162 | Miss Fortune, Captain | Unit | 37089 / 21119 / - / 21113 | 33358 / 25337 / - / 25326 | 36911 / 35135 / - / 35121 | 0 / 0 / - / 0 |  |
| 163 | Seal of Strength | Gear | 25979 / - / 25404 / 25404 | 26037 / - / 25744 / 25744 | 28253 / - / 28249 / 28249 | 0 / - / 0 / 0 |  |
| 164 | Sett, Brawler | Unit | 30934 / 30934 / 23879 / 30934 | 29266 / 29266 / 23573 / 29266 | 32554 / 32554 / 31808 / 32554 | 24047 / 24047 / 16722 / 24047 |  |
| 165 | Cemetery Attendant | Unit | 23350 / 14511 / - / 14511 | 23276 / 14020 / - / 14020 | 24966 / 17006 / - / 17006 | 0 / 0 / - / 0 |  |
| 166 | Chaos Rune | Rune | 547636 / - / - / - | 543958 / - / - / - | 542256 / - / - / - | 537600 / - / - / - |  |
| 167 | Ember Monk | Unit | 22102 / 2485 / - / 2485 | 21143 / 2431 / - / 2431 | 22399 / 2992 / - / 2992 | 0 / 0 / - / 0 |  |
| 168 | Fight or Flight | Spell | 0 / - / - / 0 | 0 / - / - / 0 | 0 / - / - / 0 | 0 / - / - / 0 | banned |
| 169 | Gust | Spell | 26359 / - / - / 25898 | 27129 / - / - / 26614 | 27243 / - / - / 26817 | 219476 / - / - / 208994 |  |
| 170 | Morbid Return | Spell | 22574 / - / - / 22238 | 22245 / - / - / 21892 | 24339 / - / - / 23963 | 0 / - / - / 0 |  |
| 171 | Mystic Poro | Unit | 24816 / - / - / - | 23770 / - / - / - | 26595 / - / - / - | 0 / - / - / - |  |
| 172 | Rebuke | Spell | 25904 / - / - / 25496 | 25258 / - / - / 24776 | 25329 / - / - / 24923 | 137238 / - / - / 134715 |  |
| 173 | Ride the Wind | Spell | 26292 / - / - / 25865 | 25632 / - / - / 25157 | 25778 / - / - / 25356 | 220164 / - / - / 207213 |  |
| 174 | Sai Scout | Unit | 21790 / - / - / - | 19585 / - / - / - | 19772 / - / - / - | 0 / - / - / - |  |
| 175 | Shipyard Skulker | Unit | 23149 / - / - / - | 21950 / - / - / - | 23438 / - / - / - | 0 / - / - / - |  |
| 176 | Sneaky Deckhand | Unit | 24630 / - / - / - | 24250 / - / - / - | 26410 / - / - / - | 27895 / - / - / - |  |
| 177 | Stealthy Pursuer | Unit | 0 / 0 / - / 0 | 0 / 0 / - / 0 | 0 / 0 / - / 0 | 0 / 0 / - / 0 | banned |
| 178 | Undercover Agent | Unit | 20618 / 9366 / - / 9359 | 19480 / 7659 / - / 7656 | 20854 / 10205 / - / 10200 | 0 / 0 / - / 0 |  |
| 179 | Acceptable Losses | Spell | 26049 / - / - / 25691 | 27734 / - / - / 27375 | 27538 / - / - / 27249 | 93209 / - / - / 90646 |  |
| 180 | Fading Memories | Spell | 18934 / - / - / 18537 | 16334 / - / - / 15886 | 19764 / - / - / 19338 | 0 / - / - / 0 |  |
| 181 | Pack of Wonders | Gear | 24427 / - / 20955 / 20955 | 22603 / - / 19969 / 19969 | 25455 / - / 24984 / 24984 | 36988 / - / 34046 / 34046 |  |
| 182 | Scrapheap | Gear | 0 / 0 / - / 0 | 0 / 0 / - / 0 | 0 / 0 / - / 0 | 0 / 0 / - / 0 | banned |
| 183 | Stacked Deck | Spell | 0 / - / - / 0 | 0 / - / - / 0 | 0 / - / - / 0 | 0 / - / - / 0 | banned |
| 184 | The Syren | Gear | 23662 / - / 16171 / 16171 | 22254 / - / 14134 / 14134 | 24333 / - / 19312 / 19311 | 0 / - / 0 / 0 |  |
| 185 | Traveling Merchant | Unit | 24982 / 19286 / - / 19286 | 24439 / 18664 / - / 18664 | 25269 / 20882 / - / 20882 | 204936 / 165670 / - / 165670 |  |
| 186 | Treasure Trove | Gear | 23490 / 20928 / 20274 / 20929 | 22372 / 20355 / 19833 / 20355 | 25717 / 25415 / 25180 / 25415 | 29948 / 28736 / 27095 / 28736 |  |
| 187 | Whirlwind | Spell | 20089 / - / - / 19788 | 19090 / - / - / 18758 | 22950 / - / - / 22594 | 0 / - / - / 0 |  |
| 188 | Zaunite Bouncer | Unit | 20722 / 17416 / - / 17370 | 19064 / 15562 / - / 15497 | 21414 / 18055 / - / 18009 | 0 / 0 / - / 0 |  |
| 189 | Kayn, Unleashed | Unit | 18364 / - / - / - | 17352 / - / - / - | 17628 / - / - / - | 0 / - / - / - |  |
| 190 | Kog'Maw, Caustic | Unit | 24917 / 15929 / - / 15923 | 23178 / 13310 / - / 13305 | 22980 / 15737 / - / 15733 | 0 / 0 / - / 0 |  |
| 191 | Maddened Marauder | Unit | 20426 / 17206 / - / 17172 | 18777 / 15388 / - / 15344 | 19933 / 17165 / - / 17113 | 0 / 0 / - / 0 |  |
| 192 | Mindsplitter | Unit | 17050 / 17050 / - / 17050 | 14955 / 14955 / - / 14955 | 14664 / 14664 / - / 14664 | 0 / 0 / - / 0 |  |
| 193 | Miss Fortune, Buccaneer | Unit | 43261 / - / - / - | 40123 / - / - / - | 44122 / - / - / - | 42882 / - / - / - |  |
| 194 | Nocturne, Horrifying | Unit | 22321 / - / - / - | 19959 / - / - / - | 23491 / - / - / - | 32316 / - / - / - |  |
| 195 | Rhasa the Sunderer | Unit | 17841 / - / - / - | 15751 / - / - / - | 19741 / - / - / - | 61998 / - / - / - |  |
| 196 | Soulgorger | Unit | 13814 / 5870 / - / 5870 | 12915 / 8213 / - / 8213 | 11542 / 10216 / - / 10216 | 0 / 0 / - / 0 |  |
| 197 | Teemo, Scout | Unit | 43341 / 43341 / - / 43341 | 43464 / 43464 / - / 43464 | 44879 / 44879 / - / 44879 | 48883 / 48883 / - / 48883 |  |
| 198 | The Harrowing | Spell | 12635 / - / - / 12420 | 10932 / - / - / 10722 | 12955 / - / - / 12741 | 0 / - / - / 0 |  |
| 199 | Tideturner | Unit | 24369 / 6371 / - / 6371 | 23307 / 8827 / - / 8827 | 27589 / 12817 / - / 12817 | 323620 / 140214 / - / 140214 |  |
| 200 | Twisted Fate, Gambler | Unit | 22643 / 7748 / - / 7748 | 20720 / 6261 / - / 6261 | 22219 / 8852 / - / 8852 | 0 / 0 / - / 0 |  |
| 201 | Invert Timelines | Spell | 21287 / - / - / 20972 | 20410 / - / - / 20028 | 22649 / - / - / 22285 | 13295 / - / - / 12934 |  |
| 202 | Jinx, Rebel | Unit | 36222 / 8281 / - / 8281 | 33058 / 8239 / - / 8239 | 37137 / 9289 / - / 9289 | 0 / 0 / - / 0 |  |
| 203 | Possession | Spell | 15481 / - / - / 15168 | 17294 / - / - / 16852 | 12088 / - / - / 11908 | 0 / - / - / 0 |  |
| 204 | Seal of Discord | Gear | 25701 / - / 25183 / 25183 | 24634 / - / 24336 / 24336 | 27684 / - / 27684 / 27684 | 67752 / - / 67302 / 67302 |  |
| 205 | Yasuo, Windrider | Unit | 31310 / 155 / - / 155 | 28805 / 160 / - / 160 | 30381 / 285 / - / 285 | 39491 / 2258 / - / 2258 | random:resolved, random:triggered, rp:resolved, rp:triggered |
| 206 | Back to Back | Spell | 26501 / - / - / 26125 | 28319 / - / - / 27856 | 25839 / - / - / 25495 | 0 / - / - / 0 |  |
| 207 | Call to Glory | Spell | 28206 / - / - / 27802 | 29132 / - / - / 28675 | 29671 / - / - / 29224 | 46726 / - / - / 39815 |  |
| 208 | Cruel Patron | Unit | 21755 / - / - / - | 20912 / - / - / - | 22902 / - / - / - | 0 / - / - / - |  |
| 209 | Cull the Weak | Spell | 24328 / - / - / 24034 | 23016 / - / - / 22719 | 26102 / - / - / 25861 | 155040 / - / - / 152597 |  |
| 210 | Daring Poro | Unit | 24568 / - / - / - | 24318 / - / - / - | 27831 / - / - / - | 30042 / - / - / - |  |
| 211 | Faithful Manufactor | Unit | 24139 / 24139 / - / 24139 | 23508 / 23508 / - / 23508 | 25910 / 25910 / - / 25910 | 53758 / 53758 / - / 53758 |  |
| 212 | Forge of the Future | Gear | 24341 / 24341 / 21379 / 24341 | 24474 / 24474 / 21972 / 24474 | 26295 / 26295 / 26116 / 26295 | 0 / 0 / 0 / 0 |  |
| 213 | Hidden Blade | Spell | 25850 / - / - / 25458 | 27914 / - / - / 27432 | 28615 / - / - / 28206 | 256204 / - / - / 244400 |  |
| 214 | Order Rune | Rune | 566272 / - / - / - | 575694 / - / - / - | 573970 / - / - / - | 566999 / - / - / - |  |
| 215 | Petty Officer | Unit | 21117 / - / - / - | 20345 / - / - / - | 22136 / - / - / - | 0 / - / - / - |  |
| 216 | Soaring Scout | Unit | 26033 / 17734 / - / 17732 | 26359 / 16716 / - / 16715 | 26806 / 19575 / - / 19575 | 93998 / 69219 / - / 69218 |  |
| 217 | Trifarian Gloryseeker | Unit | 26063 / 14017 / - / 14017 | 25701 / 13312 / - / 13312 | 27848 / 14540 / - / 14540 | 36632 / 27230 / - / 27230 |  |
| 218 | Vanguard Captain | Unit | 24380 / 11359 / - / 11359 | 24114 / 10775 / - / 10775 | 26314 / 11555 / - / 11555 | 35027 / 23870 / - / 23870 |  |
| 219 | Vanguard Sergeant | Unit | 21814 / - / - / - | 21967 / - / - / - | 23650 / - / - / - | 0 / - / - / - |  |
| 220 | Facebreaker | Spell | 14176 / - / - / 13885 | 16320 / - / - / 15854 | 17566 / - / - / 17158 | 15394 / - / - / 14651 |  |
| 221 | Imperial Decree | Spell | 22165 / - / - / 21973 | 24751 / - / - / 24447 | 22518 / - / - / 22290 | 75811 / - / - / 75591 |  |
| 222 | Noxian Drummer | Unit | 25334 / 17724 / - / 17724 | 22589 / 15544 / - / 15544 | 26826 / 20457 / - / 20457 | 25837 / 19155 / - / 19155 |  |
| 223 | Peak Guardian | Unit | 19264 / 19264 / - / 19264 | 18386 / 18386 / - / 18386 | 19577 / 19577 / - / 19577 | 0 / 0 / - / 0 |  |
| 224 | Salvage | Spell | 27332 / - / - / 27016 | 27753 / - / - / 27342 | 27694 / - / - / 27344 | 59285 / - / - / 57204 |  |
| 225 | Solari Chief | Unit | 20704 / 18762 / - / 18704 | 20137 / 18273 / - / 18191 | 21963 / 20250 / - / 20184 | 0 / 0 / - / 0 |  |
| 226 | Spectral Matron | Unit | 21511 / 5808 / - / 5808 | 21679 / 8735 / - / 8735 | 22886 / 13354 / - / 13354 | 32481 / 20815 / - / 20815 |  |
| 227 | Symbol of the Solari | Gear | 25235 / - / - / - | 24031 / - / - / - | 28049 / - / - / - | 0 / - / - / - |  |
| 228 | Vanguard Helm | Gear | 25154 / 2262 / - / 2262 | 23644 / 1889 / - / 1888 | 26833 / 2697 / - / 2696 | 0 / 0 / - / 0 |  |
| 229 | Vengeance | Spell | 27293 / - / - / 26948 | 25478 / - / - / 25122 | 25868 / - / - / 25540 | 0 / - / - / 0 |  |
| 230 | Albus Ferros | Unit | 22217 / 22217 / - / 22217 | 21749 / 21749 / - / 21749 | 24716 / 24716 / - / 24716 | 0 / 0 / - / 0 |  |
| 231 | Commander Ledros | Unit | 18389 / - / - / - | 17506 / - / - / - | 18589 / - / - / - | 0 / - / - / - |  |
| 232 | Fiora, Victorious | Unit | 33341 / - / - / - | 32674 / - / - / - | 35335 / - / - / - | 0 / - / - / - |  |
| 233 | Grand Strategem | Spell | 19916 / - / - / 19694 | 21323 / - / - / 21038 | 20105 / - / - / 19921 | 22345 / - / - / 22251 |  |
| 234 | Harnessed Dragon | Unit | 15094 / 14246 / - / 14190 | 14007 / 13114 / - / 13022 | 12845 / 11950 / - / 11888 | 31601 / 27129 / - / 27035 |  |
| 235 | Karma, Channeler | Unit | 18558 / 11154 / - / 11154 | 18344 / 15384 / - / 15384 | 19100 / 18979 / - / 18979 | 0 / 0 / - / 0 |  |
| 236 | Karthus, Eternal | Unit | 25208 / - / - / - | 23103 / - / - / - | 26407 / - / - / - | 61357 / - / - / - |  |
| 237 | King's Edict | Spell | 15806 / - / - / 15640 | 14703 / - / - / 14501 | 15510 / - / - / 15343 | 0 / - / - / 0 |  |
| 238 | Leona, Determined | Unit | 40334 / 13996 / - / 13905 | 38492 / 11960 / - / 11847 | 41785 / 17222 / - / 17112 | 41680 / 17544 / - / 17430 |  |
| 239 | Machine Evangel | Unit | 21070 / 10934 / - / 10933 | 20433 / 9652 / - / 9652 | 21879 / 12324 / - / 12323 | 14522 / 8410 / - / 8409 |  |
| 240 | Sett, Kingpin | Unit | 34001 / - / - / - | 33291 / - / - / - | 35485 / - / - / - | 0 / - / - / - |  |
| 241 | Shen, Kinkou | Unit | 41441 / - / - / - | 44440 / - / - / - | 44545 / - / - / - | 21434 / - / - / - |  |
| 242 | Baited Hook | Gear | 22547 / - / 19528 / 19528 | 21527 / - / 17879 / 17879 | 25222 / - / 22921 / 22921 | 28386 / - / 26148 / 26148 |  |
| 243 | Darius, Executioner | Unit | 29147 / 7210 / - / 7210 | 27125 / 6944 / - / 6944 | 28377 / 5416 / - / 5416 | 0 / 0 / - / 0 |  |
| 244 | Divine Judgment | Spell | 14626 / - / - / 14456 | 13797 / - / - / 13630 | 13554 / - / - / 13414 | 0 / - / - / 0 |  |
| 245 | Seal of Unity | Gear | 25149 / - / 24607 / 24607 | 27017 / - / 26673 / 26673 | 28493 / - / 28486 / 28486 | 33699 / - / 33478 / 33478 |  |
| 246 | Viktor, Leader | Unit | 40960 / 23760 / - / 23760 | 37119 / 19339 / - / 19335 | 41746 / 23080 / - / 23078 | 37486 / 20023 / - / 20023 |  |
| 247 | Icathian Rain | Spell | 9574 / - / - / 9475 | 8700 / - / - / 8599 | 8877 / - / - / 8813 | 0 / - / - / 0 |  |
| 248 | Stormbringer | Spell | 8668 / - / - / 8623 | 7632 / - / - / 7565 | 9234 / - / - / 9200 | 0 / - / - / 0 |  |
| 249 | Loose Cannon | Legend | 40400 / 40400 / - / 40400 | 39340 / 39340 / - / 39340 | 41100 / 41100 / - / 41100 | 0 / 0 / - / 0 |  |
| 250 | Super Mega Death Rocket! | Spell | 15757 / 8667 / - / 15815 | 15178 / 10820 / - / 15355 | 17783 / 14595 / - / 17934 | 0 / 0 / - / 0 |  |
| 251 | Noxian Guillotine | Spell | 15826 / - / - / 15739 | 16564 / - / - / 16415 | 15039 / - / - / 14976 | 0 / - / - / 0 |  |
| 252 | Nine-Tailed Fox | Legend | 42800 / 34228 / - / 34228 | 40700 / 28987 / - / 28987 | 39600 / 33589 / - / 33589 | 46200 / 36124 / - / 36124 |  |
| 253 | Fox-Fire | Spell | 16914 / - / - / 16594 | 16392 / - / - / 16034 | 16220 / - / - / 15939 | 0 / - / - / 0 |  |
| 254 | Blind Monk | Legend | 41220 / - / 40162 / 40162 | 40280 / - / 38691 / 38691 | 40960 / - / 40747 / 40747 | 0 / - / 0 / 0 |  |
| 255 | Dragon's Rage | Spell | 11330 / 2911 / - / 11082 | 9636 / 2200 / - / 9410 | 12177 / 2782 / - / 11832 | 0 / 0 / - / 0 |  |
| 256 | Unforgiven | Legend | 40180 / - / 38249 / 38249 | 38980 / - / 35933 / 35933 | 39620 / - / 38478 / 38478 | 46200 / - / 44601 / 44601 |  |
| 257 | Last Breath | Spell | 12142 / - / - / 11844 | 13337 / - / - / 12932 | 12977 / - / - / 12575 | 0 / - / - / 0 |  |
| 258 | Zenith Blade | Spell | 12971 / - / - / 12800 | 14176 / - / - / 13873 | 15294 / - / - / 15075 | 25277 / - / - / 24976 |  |
| 259 | Swift Scout | Legend | 41800 / - / 36395 / 36395 | 40440 / - / 35683 / 35683 | 38240 / - / 36033 / 36033 | 46200 / - / 43198 / 43198 |  |
| 260 | Guerilla Warfare | Spell | 15165 / - / - / 14997 | 14496 / - / - / 14323 | 15312 / - / - / 15167 | 26033 / - / - / 25418 |  |
| 261 | Siphon Power | Spell | 17926 / - / - / 17820 | 17728 / - / - / 17619 | 16806 / - / - / 16751 | 0 / - / - / 0 |  |
| 262 | Bounty Hunter | Legend | 41900 / - / 41457 / 41454 | 39900 / - / 39689 / 39687 | 41320 / - / 41320 / 41320 | 46200 / - / 45982 / 45980 |  |
| 263 | Bullet Time | Spell | 15727 / - / - / 15607 | 16199 / - / - / 16043 | 16420 / - / - / 16248 | 0 / - / - / 0 |  |
| 264 | The Boss | Legend | 39180 / 37707 / - / 37707 | 42620 / 41153 / - / 41153 | 40240 / 39781 / - / 39781 | 0 / 0 / - / 0 |  |
| 265 | Showstopper | Spell | 13632 / - / - / 13550 | 14739 / - / - / 14659 | 15140 / - / - / 15081 | 0 / - / - / 0 |  |
| 266 | Recruit (DE) | Unit | 80694 / - / - / - | 81963 / - / - / - | 83017 / - / - / - | 0 / - / - / - |  |
| 267 | Recruit (NX) | Unit | 80068 / - / - / - | 81873 / - / - / - | 83623 / - / - / - | 0 / - / - / - |  |
| 268 | Recruit (ZN) | Unit | 80378 / - / - / - | 80208 / - / - / - | 84616 / - / - / - | 0 / - / - / - |  |
| 269 | Sprite | Unit | 81027 / - / - / - | 80016 / - / - / - | 83277 / - / - / - | 0 / - / - / - |  |
| 270 | Altar to Unity | Battlefield | 34452 / 33742 / - / 33732 | 33883 / 32919 / - / 32911 | 32491 / 32240 / - / 32231 | 15268 / 14727 / - / 14727 |  |
| 271 | Aspirant's Climb | Battlefield | 0 / - / - / - | 0 / - / - / - | 0 / - / - / - | 0 / - / - / - | banned |
| 272 | Back-Alley Bar | Battlefield | 33009 / 30484 / - / 30484 | 33535 / 29803 / - / 29803 | 33889 / 33527 / - / 33527 | 0 / 0 / - / 0 |  |
| 273 | Bandle Tree | Battlefield | 34514 / - / - / - | 32425 / - / - / - | 33352 / - / - / - | 15202 / - / - / - |  |
| 274 | Fortified Position | Battlefield | 34222 / 27259 / - / 27222 | 33767 / 23344 / - / 23272 | 34732 / 28773 / - / 28722 | 15398 / 11969 / - / 11958 |  |
| 275 | Grove of the God-Willow | Battlefield | 32394 / 31743 / - / 31737 | 32087 / 31211 / - / 31204 | 32651 / 32357 / - / 32351 | 60233 / 58793 / - / 58777 |  |
| 276 | Hallowed Tomb | Battlefield | 31698 / 6391 / - / 6386 | 33004 / 7376 / - / 7364 | 32392 / 10415 / - / 10402 | 0 / 0 / - / 0 |  |
| 277 | Monastery of Hirana | Battlefield | 33986 / 3006 / - / 3006 | 33628 / 4562 / - / 4561 | 33940 / 7184 / - / 7181 | 15282 / 5381 / - / 5381 |  |
| 278 | Navori Fighting Pit | Battlefield | 32678 / 32065 / - / 32055 | 32644 / 31720 / - / 31711 | 33418 / 33084 / - / 33077 | 0 / 0 / - / 0 |  |
| 279 | Obelisk of Power | Battlefield | 0 / 0 / - / 0 | 0 / 0 / - / 0 | 0 / 0 / - / 0 | 0 / 0 / - / 0 | banned |
| 280 | Reaver's Row | Battlefield | 0 / 0 / - / 0 | 0 / 0 / - / 0 | 0 / 0 / - / 0 | 0 / 0 / - / 0 | banned |
| 281 | Reckoner's Arena | Battlefield | 32943 / 32339 / - / 32334 | 32193 / 31230 / - / 31223 | 33096 / 32715 / - / 32713 | 15232 / 14925 / - / 14924 |  |
| 282 | Sigil of the Storm | Battlefield | 33614 / 33993 / - / 33993 | 33444 / 33682 / - / 33681 | 32835 / 33217 / - / 33217 | 60235 / 61182 / - / 61181 |  |
| 283 | Startipped Peak | Battlefield | 33085 / 26938 / - / 26924 | 33111 / 30888 / - / 30877 | 33921 / 33604 / - / 33601 | 45253 / 41064 / - / 41059 |  |
| 284 | Targon's Peak | Battlefield | 32817 / 33145 / - / 33145 | 33774 / 33970 / - / 33970 | 34346 / 34785 / - / 34785 | 74648 / 75898 / - / 75898 |  |
| 285 | The Arena's Greatest | Battlefield | 0 / 0 / - / 0 | 0 / 0 / - / 0 | 0 / 0 / - / 0 | 0 / 0 / - / 0 | banned |
| 286 | The Candlelit Sanctum | Battlefield | 33645 / 33951 / - / 33951 | 32397 / 32523 / - / 32523 | 33229 / 33719 / - / 33719 | 30196 / 30651 / - / 30650 |  |
| 287 | The Dreaming Tree | Battlefield | 0 / 0 / - / 0 | 0 / 0 / - / 0 | 0 / 0 / - / 0 | 0 / 0 / - / 0 | banned |
| 288 | The Grand Plaza | Battlefield | 33528 / 312 / - / 309 | 33347 / 385 / - / 385 | 32817 / 305 / - / 304 | 0 / 0 / - / 0 |  |
| 289 | Trifarian War Camp | Battlefield | 34646 / - / - / - | 33927 / - / - / - | 32732 / - / - / - | 30339 / - / - / - |  |
| 290 | Vilemaw's Lair | Battlefield | 31910 / - / - / - | 32911 / - / - / - | 32358 / - / - / - | 50477 / - / - / - |  |
| 291 | Void Gate | Battlefield | 33189 / - / - / - | 32763 / - / - / - | 32616 / - / - / - | 60061 / - / - / - |  |
| 292 | Windswept Hillock | Battlefield | 32414 / - / - / - | 32213 / - / - / - | 33771 / - / - / - | 30221 / - / - / - |  |
| 293 | Zaun Warrens | Battlefield | 33486 / 33792 / - / 33792 | 32981 / 33169 / - / 33169 | 32927 / 33392 / - / 33392 | 118072 / 119984 / - / 119984 |  |
| 294 | Daughter of the Void | Legend | 42760 / - / 42760 / 42760 | 41440 / - / 41440 / 41440 | 39540 / - / 39540 / 39540 | 46200 / - / 46200 / 46200 |  |
| 295 | Relentless Storm | Legend | 40760 / 29239 / - / 29239 | 39780 / 33684 / - / 33684 | 39280 / 35603 / - / 35603 | 0 / 0 / - / 0 |  |
| 296 | Hand of Noxus | Legend | 39880 / - / 39880 / 39880 | 40980 / - / 40980 / 40980 | 39540 / - / 39540 / 39540 | 46200 / - / 46200 / 46200 |  |
| 297 | Radiant Dawn | Legend | 40020 / 19558 / - / 19558 | 40800 / 20316 / - / 20316 | 41360 / 23275 / - / 23275 | 46200 / 35474 / - / 35474 |  |
| 298 | Herald of the Arcane | Legend | 42040 / - / 41083 / 41083 | 40720 / - / 39780 / 39780 | 42080 / - / 42045 / 42045 | 46200 / - / 45495 / 45495 |  |
| 299 | Annie, Fiery | Unit | 35991 / - / - / - | 32251 / - / - / - | 35422 / - / - / - | 0 / - / - / - |  |
| 300 | Firestorm | Spell | 15255 / - / - / 15133 | 12797 / - / - / 12639 | 14632 / - / - / 14534 | 0 / - / - / 0 |  |
| 301 | Incinerate | Spell | 27304 / - / - / 27043 | 26252 / - / - / 25902 | 26555 / - / - / 26316 | 0 / - / - / 0 |  |
| 302 | Yi, Meditative | Unit | 38958 / - / - / - | 33985 / - / - / - | 38714 / - / - / - | 0 / - / - / - |  |
| 303 | Zephyr Sage | Unit | 20783 / - / - / - | 18219 / - / - / - | 18043 / - / - / - | 0 / - / - / - |  |
| 304 | Lux, Illuminated | Unit | 33568 / 6322 / - / 6322 | 30155 / 5719 / - / 5719 | 30719 / 5044 / - / 5044 | 0 / 0 / - / 0 |  |
| 305 | Garen, Rugged | Unit | 32606 / - / - / - | 30358 / - / - / - | 33451 / - / - / - | 0 / - / - / - |  |
| 306 | Gentlemen's Duel | Spell | 19699 / - / - / 19395 | 20688 / - / - / 20276 | 19554 / - / - / 19215 | 0 / - / - / 0 |  |
| 307 | Yi, Honed | Unit | 30860 / - / - / - | 28568 / - / - / - | 31525 / - / - / - | 0 / - / - / - |  |
| 308 | Annie, Stubborn | Unit | 38548 / 24812 / - / 24812 | 35810 / 23434 / - / 23434 | 39873 / 26764 / - / 26764 | 0 / 0 / - / 0 |  |
| 309 | Flash | Spell | 28498 / - / - / 28117 | 28408 / - / - / 28020 | 27914 / - / - / 27595 | 122333 / - / - / 114287 |  |
| 310 | Blast of Power | Spell | 25345 / - / - / 25012 | 25826 / - / - / 25405 | 25125 / - / - / 24807 | 0 / - / - / 0 |  |
| 311 | Garen, Commander | Unit | 33393 / - / - / - | 32917 / - / - / - | 32905 / - / - / - | 0 / - / - / - |  |
| 312 | Lux, Crownguard | Unit | 37878 / - / 23860 / 23860 | 36415 / - / 25570 / 25570 | 41803 / - / 26187 / 26187 | 40340 / - / 28244 / 28244 |  |
| 313 | Recruit the Vanguard | Spell | 22317 / - / - / 22065 | 23620 / - / - / 23320 | 22465 / - / - / 22239 | 0 / - / - / 0 |  |
| 314 | Vanguard Attendant | Unit | 20763 / - / - / - | 17840 / - / - / - | 19293 / - / - / - | 0 / - / - / - |  |
| 315 | Dark Child - Starter | Legend | 40640 / 40640 / - / 40640 | 38940 / 38940 / - / 38940 | 41080 / 41080 / - / 41080 | 0 / 0 / - / 0 |  |
| 316 | Tibbers | Unit | 9132 / 9132 / - / 9132 | 8206 / 8206 / - / 8206 | 8181 / 8181 / - / 8181 | 0 / 0 / - / 0 |  |
| 317 | Wuju Bladesman - Starter | Legend | 40980 / - / - / - | 42380 / - / - / - | 42280 / - / - / - | 46200 / - / - / - |  |
| 318 | Highlander | Spell | 18432 / - / - / 18033 | 18882 / - / - / 18431 | 18339 / - / - / 17922 | 0 / - / - / 0 |  |
| 319 | Lady of Luminosity - Starter | Legend | 40860 / 25301 / - / 25301 | 39900 / 24254 / - / 24254 | 39840 / 23626 / - / 23626 | 46200 / 45361 / - / 45361 |  |
| 320 | Final Spark | Spell | 15760 / - / - / 15659 | 15188 / - / - / 15040 | 13313 / - / - / 13249 | 0 / - / - / 0 |  |
| 321 | Might of Demacia - Starter | Legend | 38100 / 2611 / - / 2611 | 40260 / 4067 / - / 4067 | 40280 / 6362 / - / 6362 | 0 / 0 / - / 0 |  |
| 322 | Decisive Strike | Spell | 13394 / - / - / 13332 | 15397 / - / - / 15277 | 14531 / - / - / 14493 | 0 / - / - / 0 |  |
| 323 | Against the Odds | Spell | 25248 / - / - / 25029 | 25518 / - / - / 25275 | 26212 / - / - / 26002 | 29644 / - / - / 29082 |  |
| 324 | Armed Assailant | Unit | 17191 / 15603 / - / 15603 | 16776 / 16165 / - / 16165 | 16965 / 16965 / - / 16965 | 0 / 0 / - / 0 |  |
| 325 | Blood Rush | Spell | 26306 / - / - / 26066 | 27526 / - / - / 27222 | 27849 / - / - / 27675 | 72720 / - / - / 71034 |  |
| 326 | Gold | Gear | 299006 / - / 267411 / 267411 | 310160 / - / 282753 / 282753 | 336241 / - / 312050 / 312050 | 384917 / - / 356672 / 356672 | token card |
| 327 | Bushwhack | Spell | 24060 / - / - / 23844 | 24469 / - / - / 24248 | 25040 / - / - / 24831 | 0 / - / - / 0 |  |
| 328 | Detonate | Spell | 21529 / - / - / 21327 | 21098 / - / - / 20866 | 22847 / - / - / 22694 | 0 / - / - / 0 |  |
| 329 | Eager Drakehound | Unit | 22342 / - / - / - | 22316 / - / - / - | 21907 / - / - / - | 0 / - / - / - |  |
| 330 | Gem Jammer | Unit | 23569 / 23569 / - / 23547 | 23512 / 23512 / - / 23477 | 26384 / 26384 / - / 26362 | 68145 / 68145 / - / 68060 |  |
| 331 | Sentinel Adept | Unit | 23488 / 21632 / - / 21632 | 21902 / 21257 / - / 21257 | 24491 / 24491 / - / 24491 | 0 / 0 / - / 0 |  |
| 332 | Serrated Dirk | Gear | 24119 / - / - / - | 24334 / - / - / - | 25883 / - / - / - | 0 / - / - / - |  |
| 333 | Void Drone | Unit | 23643 / - / - / - | 21593 / - / - / - | 23160 / - / - / - | 0 / - / - / - |  |
| 334 | Angle Shot | Spell | 15091 / - / - / 14955 | 13786 / - / - / 13580 | 14907 / - / - / 14788 | 0 / - / - / 0 |  |
| 335 | Battering Ram | Unit | 23155 / - / - / - | 19245 / - / - / - | 19939 / - / - / - | 0 / - / - / - |  |
| 336 | Blast Corps Cadet | Unit | 24023 / 6941 / - / 6925 | 24875 / 9892 / - / 9854 | 26236 / 12177 / - / 12133 | 0 / 0 / - / 0 |  |
| 337 | Minotaur Reckoner | Unit | 20232 / - / - / - | 17586 / - / - / - | 18798 / - / - / - | 0 / - / - / - |  |
| 338 | Perched Grimwyrm | Unit | 6164 / - / - / - | 4994 / - / - / - | 8414 / - / - / - | 0 / - / - / - |  |
| 339 | Recurve Bow | Gear | 23222 / 14032 / - / 20706 | 22442 / 12084 / - / 19566 | 24401 / 16993 / - / 22626 | 0 / 0 / - / 0 |  |
| 340 | Sudden Storm | Spell | 25104 / - / - / 24856 | 23977 / - / - / 23631 | 25537 / - / - / 25218 | 0 / - / - / 0 |  |
| 341 | Void Hatchling | Unit | 25003 / - / - / - | 24785 / - / - / - | 25239 / - / - / - | 0 / - / - / - |  |
| 342 | Assembly Rig | Gear | 19361 / - / 13188 / 13188 | 16851 / - / 10813 / 10813 | 20297 / - / 17230 / 17230 | 0 / - / 0 / 0 |  |
| 343 | Draven, Vanquisher | Unit | 0 / 0 / - / 0 | 0 / 0 / - / 0 | 0 / 0 / - / 0 | 0 / 0 / - / 0 | banned |
| 344 | Ferrous Forerunner | Unit | 17325 / 6878 / - / 6874 | 16510 / 5872 / - / 5869 | 16601 / 7245 / - / 7241 | 122861 / 53624 / - / 53623 |  |
| 345 | Long Sword | Gear | 26688 / - / - / - | 27185 / - / - / - | 26617 / - / - / - | 87635 / - / - / - |  |
| 346 | Piercing Light | Spell | 27215 / - / - / 26990 | 26458 / - / - / 26126 | 26928 / - / - / 26705 | 18141 / - / - / 17177 |  |
| 347 | Rell, Magnetic | Unit | 21528 / 195 / - / 195 | 19376 / 265 / - / 265 | 20395 / 302 / - / 302 | 0 / 0 / - / 0 | random:resolved, random:triggered |
| 348 | Rengar, Pouncing | Unit | 39418 / - / - / - | 40309 / - / - / - | 39195 / - / - / - | 0 / - / - / - |  |
| 349 | Rumble, Hotheaded | Unit | 39516 / 920 / - / 920 | 35054 / 1363 / - / 1362 | 39700 / 2914 / - / 2913 | 73840 / 19353 / - / 19353 |  |
| 350 | Dunebreaker | Unit | 16614 / 7684 / - / 7677 | 14009 / 6402 / - / 6396 | 14634 / 6217 / - / 6214 | 0 / 0 / - / 0 |  |
| 351 | Lucian, Gunslinger | Unit | 41105 / 13593 / - / 13503 | 37412 / 11491 / - / 11390 | 41882 / 16730 / - / 16638 | 0 / 0 / - / 0 |  |
| 352 | Rek'Sai, Breacher | Unit | 41205 / - / - / - | 39612 / - / - / - | 42423 / - / - / - | 73710 / - / - / - |  |
| 353 | Skyfall of Areion | Gear | 21153 / - / - / - | 20337 / - / - / - | 21778 / - / - / - | 26620 / - / - / - |  |
| 354 | Desert's Call | Spell | 23348 / - / - / 22942 | 22275 / - / - / 21929 | 24891 / - / - / 24536 | 0 / - / - / 0 |  |
| 355 | Disarming Rake | Unit | 23451 / 8725 / - / 8711 | 23223 / 13159 / - / 13140 | 24051 / 17531 / - / 17513 | 16417 / 5362 / - / 5343 |  |
| 356 | Doran's Shield | Gear | 24252 / - / - / - | 23681 / - / - / - | 28161 / - / - / - | 35198 / - / - / - |  |
| 357 | Feral Strength | Spell | 27348 / - / - / 26803 | 28843 / - / - / 28150 | 26340 / - / - / 25819 | 0 / - / - / 0 |  |
| 358 | Guardian of the Passage | Unit | 18878 / 4843 / - / 4834 | 17741 / 5976 / - / 5957 | 19784 / 7898 / - / 7879 | 0 / 0 / - / 0 |  |
| 359 | Lonely Poro | Unit | 24336 / 15163 / - / 15155 | 24165 / 13810 / - / 13810 | 27439 / 18553 / - / 18551 | 127945 / 86076 / - / 86076 |  |
| 360 | Navori Scout | Unit | 22033 / - / - / - | 20637 / - / - / - | 22651 / - / - / - | 0 / - / - / - |  |
| 361 | Ribbon Dancer | Unit | 23450 / 15591 / - / 15591 | 21780 / 14242 / - / 14242 | 26196 / 18990 / - / 18990 | 0 / 0 / - / 0 |  |
| 362 | Royal Entourage | Unit | 24133 / 24133 / - / 24133 | 21659 / 21659 / - / 21659 | 23529 / 23529 / - / 23529 | 0 / 0 / - / 0 |  |
| 363 | Thwonk! | Spell | 16454 / - / - / 16035 | 18367 / - / - / 17884 | 19533 / - / - / 18976 | 0 / - / - / 0 |  |
| 364 | Apprentice Smith | Unit | 26129 / 20302 / - / 20302 | 24916 / 19249 / - / 19249 | 26347 / 21786 / - / 21786 | 33605 / 27630 / - / 27629 |  |
| 365 | Brutalizer | Gear | 22404 / - / - / - | 23116 / - / - / - | 24414 / - / - / - | 125624 / - / - / - |  |
| 366 | Emperor's Divide | Spell | 26228 / - / - / 25767 | 27811 / - / - / 27217 | 27652 / - / - / 27057 | 44148 / - / - / 38810 |  |
| 367 | Legion Quartermaster | Unit | 14751 / - / - / - | 13651 / - / - / - | 16725 / - / - / - | 0 / - / - / - |  |
| 368 | Not So Fast | Spell | 6893 / - / - / 6779 | 10104 / - / - / 9942 | 7460 / - / - / 7322 | 12806 / - / - / 11875 |  |
| 369 | Poro Snax | Gear | 24936 / 24936 / 19525 / 24936 | 24135 / 24135 / 18484 / 24135 | 27393 / 27393 / 24412 / 27393 | 33707 / 33707 / 29055 / 33707 |  |
| 370 | Simian Ancestor | Unit | 20718 / 1431 / - / 1431 | 19042 / 1327 / - / 1327 | 21620 / 1820 / - / 1820 | 0 / 0 / - / 0 |  |
| 371 | Stellacorn Herder | Unit | 22310 / 16291 / - / 16291 | 21916 / 15508 / - / 15508 | 24269 / 18989 / - / 18989 | 82949 / 66344 / - / 66344 |  |
| 372 | Aphelios, Exalted | Unit | 23868 / 2371 / - / 2371 | 20372 / 1935 / - / 1935 | 24208 / 2907 / - / 2907 | 27416 / 14591 / - / 14591 |  |
| 373 | Azir, Ascendant | Unit | 33234 / - / 27903 / 27903 | 30977 / - / 27214 / 27214 | 34989 / - / 32626 / 32626 | 0 / - / 0 / 0 |  |
| 374 | Guardian Angel | Gear | 24220 / - / - / - | 22994 / - / - / - | 25954 / - / - / - | 60416 / - / - / - |  |
| 375 | Heart of Dark Ice | Gear | 20955 / - / 17903 / 17883 | 20931 / - / 18373 / 18354 | 23894 / - / 23599 / 23588 | 0 / - / 0 / 0 |  |
| 376 | Janna, Savior | Unit | 27365 / 27365 / - / 27361 | 26925 / 26925 / - / 26923 | 28606 / 28606 / - / 28606 | 20070 / 20070 / - / 20070 |  |
| 377 | Jax, Unmatched | Unit | 34541 / - / - / - | 33680 / - / - / - | 37646 / - / - / - | 25120 / - / - / - |  |
| 378 | Needlessly Large Yordle | Unit | 13090 / - / - / - | 12064 / - / - / - | 11567 / - / - / - | 0 / - / - / - |  |
| 379 | Sterak's Gage | Gear | 26882 / - / - / - | 27607 / - / - / - | 25390 / - / - / - | 19380 / - / - / - |  |
| 380 | Irelia, Fervent | Unit | 36021 / 32102 / - / 32102 | 34700 / 31329 / - / 31329 | 36713 / 32081 / - / 32081 | 71673 / 63773 / - / 63773 |  |
| 381 | Ornn, Blacksmith | Unit | 36881 / 36881 / - / 36881 | 33412 / 33412 / - / 33412 | 37572 / 37572 / - / 37572 | 34222 / 34222 / - / 34222 |  |
| 382 | Svellsongur | Gear | 21300 / - / - / - | 21161 / - / - / - | 22291 / - / - / - | 0 / - / - / - |  |
| 383 | Tianna Crownguard | Unit | 16809 / - / - / - | 15428 / - / - / - | 17066 / - / - / - | 0 / - / - / - |  |
| 384 | Aspiring Engineer | Unit | 24166 / 3626 / - / 3626 | 22140 / 3745 / - / 3745 | 24082 / 4253 / - / 4253 | 0 / 0 / - / 0 |  |
| 385 | Bubble Bot | Unit | 25118 / 5399 / - / 5399 | 22943 / 4577 / - / 4577 | 24801 / 4884 / - / 4884 | 32925 / 27835 / - / 27835 |  |
| 386 | Chemtech Cask | Gear | 24572 / 2594 / - / 2594 | 27042 / 5636 / - / 5636 | 28438 / 3615 / - / 3615 | 0 / 0 / - / 0 |  |
| 387 | Cloth Armor | Gear | 28207 / - / - / - | 28662 / - / - / - | 27584 / - / - / - | 49167 / - / - / - |  |
| 388 | Forecaster | Unit | 25238 / - / - / - | 23632 / - / - / - | 25962 / - / - / - | 33872 / - / - / - |  |
| 389 | Frigid Touch | Spell | 28537 / - / - / 28100 | 29733 / - / - / 29313 | 28378 / - / - / 28042 | 30958 / - / - / 28135 |  |
| 390 | Frostcoat Cub | Unit | 24954 / 12047 / - / 12041 | 23121 / 16740 / - / 16706 | 24743 / 20751 / - / 20716 | 0 / 0 / - / 0 |  |
| 391 | Gearhead | Unit | 21190 / - / - / - | 18303 / - / - / - | 20354 / - / - / - | 0 / - / - / - |  |
| 392 | Plundering Poro | Unit | 25187 / 10385 / - / 10383 | 23208 / 10610 / - / 10608 | 25457 / 13472 / - / 13472 | 74304 / 38183 / - / 38183 |  |
| 393 | Wages of Pain | Spell | 26314 / - / - / 25822 | 27965 / - / - / 27445 | 26500 / - / - / 26065 | 101819 / - / - / 99430 |  |
| 394 | Breakneck Mech | Unit | 15714 / - / - / - | 13111 / - / - / - | 13093 / - / - / - | 0 / - / - / - |  |
| 395 | Dropboarder | Unit | 22973 / 4787 / - / 4787 | 20862 / 4247 / - / 4247 | 22346 / 5317 / - / 5317 | 0 / 0 / - / 0 |  |
| 396 | Experimental Hexplate | Gear | 27095 / - / - / - | 26138 / - / - / - | 27413 / - / - / - | 0 / - / - / - |  |
| 397 | Pickpocket | Unit | 24516 / 5344 / - / 5340 | 22384 / 7635 / - / 7631 | 23692 / 10684 / - / 10670 | 0 / 0 / - / 0 |  |
| 398 | Prize of Progress | Unit | 22487 / 6171 / - / 6171 | 19227 / 5539 / - / 5539 | 21915 / 7152 / - / 7152 | 0 / 0 / - / 0 |  |
| 399 | Production Surge | Spell | 19735 / - / - / 19399 | 18550 / - / - / 18218 | 20510 / - / - / 20201 | 30992 / - / - / 30445 |  |
| 400 | Rocket Barrage | Spell | 19489 / - / - / 19138 | 17797 / - / - / 17444 | 20400 / - / - / 20130 | 38600 / - / - / 37727 |  |
| 401 | Temporal Portal | Gear | 22048 / - / 18913 / 18913 | 20408 / - / 17815 / 17815 | 24418 / - / 24153 / 24153 | 0 / - / 0 / 0 |  |
| 402 | Bellows Breath | Spell | 28152 / - / - / 27770 | 27519 / - / - / 27154 | 28180 / - / - / 27883 | 205167 / - / - / 200359 |  |
| 403 | Card Sharp | Unit | 23890 / 23890 / - / 23890 | 23134 / 23134 / - / 23134 | 24662 / 24662 / - / 24662 | 33708 / 33708 / - / 33708 |  |
| 404 | Ezreal, Dashing | Unit | 39692 / 24920 / 34973 / 36730 | 36529 / 20149 / 33308 / 34029 | 38790 / 26431 / 37391 / 38083 | 0 / 0 / 0 / 0 |  |
| 405 | Hextech Anomaly | Gear | 21203 / - / 20532 / 20532 | 20413 / - / 19987 / 19987 | 24030 / - / 24026 / 24026 | 0 / - / 0 / 0 |  |
| 406 | Jayce, Man of Progress | Unit | 33707 / 8576 / - / 8576 | 31315 / 12452 / - / 12452 | 32995 / 17234 / - / 17234 | 25577 / 12438 / - / 12438 |  |
| 407 | Ornn, Forge God | Unit | 34698 / 30705 / - / 30705 | 29318 / 27998 / - / 27998 | 34184 / 34184 / - / 34184 | 0 / 0 / - / 0 |  |
| 408 | World Atlas | Gear | 22377 / 11747 / - / 20194 | 20805 / 10525 / - / 18167 | 24363 / 14043 / - / 23231 | 15726 / 8346 / - / 12603 |  |
| 409 | Premonition | Spell | 25395 / - / - / 25052 | 27875 / - / - / 27473 | 23900 / - / - / 23627 | 0 / - / - / 0 |  |
| 410 | Renata Glasc, Mastermind | Unit | 36864 / - / 14230 / 14230 | 36023 / - / 13385 / 13385 | 36347 / - / 19832 / 19832 | 36881 / - / 12983 / 12983 |  |
| 411 | Rumble, Scrapper | Unit | 35835 / 18190 / - / 18172 | 34175 / 17393 / - / 17385 | 35648 / 16986 / - / 16965 | 0 / 0 / - / 0 |  |
| 412 | The Zero Drive | Gear | 22869 / 8904 / 19259 / 19301 | 20298 / 7061 / 16059 / 16116 | 23476 / 11132 / 20868 / 20888 | 0 / 0 / 0 / 0 |  |
| 413 | Buhru Captain | Unit | 23661 / 23661 / - / 23661 | 23087 / 23087 / - / 23087 | 24429 / 24429 / - / 24429 | 0 / 0 / - / 0 |  |
| 414 | Combat Chef | Unit | 21052 / 18982 / - / 18982 | 18962 / 18268 / - / 18268 | 21059 / 21059 / - / 21059 | 0 / 0 / - / 0 |  |
| 415 | Dauntless Vanguard | Unit | 23629 / - / - / - | 22572 / - / - / - | 24648 / - / - / - | 0 / - / - / - |  |
| 416 | Direwing | Unit | 17774 / - / - / - | 16343 / - / - / - | 16644 / - / - / - | 0 / - / - / - |  |
| 417 | Doran's Blade | Gear | 23009 / - / - / - | 23320 / - / - / - | 27310 / - / - / - | 28494 / - / - / - |  |
| 418 | Laurent Bladekeeper | Unit | 24269 / - / - / - | 23775 / - / - / - | 25488 / - / - / - | 0 / - / - / - |  |
| 419 | Punch First | Spell | 26399 / - / - / 26132 | 26259 / - / - / 25927 | 26498 / - / - / 26191 | 238774 / - / - / 237924 |  |
| 420 | Sea Monkey | Unit | 26325 / 10869 / - / 10869 | 25964 / 15556 / - / 15556 | 28104 / 19451 / - / 19451 | 0 / 0 / - / 0 |  |
| 421 | Veteran Poro | Unit | 25997 / 24116 / - / 24116 | 24850 / 24221 / - / 24221 | 25568 / 25568 / - / 25568 | 0 / 0 / - / 0 |  |
| 422 | Yordle Explorer | Unit | 21528 / 5456 / - / 5456 | 21557 / 5218 / - / 5218 | 23851 / 5520 / - / 5520 | 0 / 0 / - / 0 |  |
| 423 | Fae Dragon | Unit | 17246 / 17246 / - / 17246 | 15273 / 15273 / - / 15273 | 17680 / 17680 / - / 17680 | 0 / 0 / - / 0 |  |
| 424 | Hexdrinker | Gear | 24292 / - / - / - | 24119 / - / - / - | 25171 / - / - / - | 0 / - / - / - |  |
| 425 | Jaull-Fish | Unit | 19788 / - / - / - | 19763 / - / - / - | 21133 / - / - / - | 0 / - / - / - |  |
| 426 | Petricite Monument | Gear | 23759 / - / - / - | 23995 / - / - / - | 25682 / - / - / - | 0 / - / - / - |  |
| 427 | Ruin Runner | Unit | 18643 / - / - / - | 17868 / - / - / - | 18905 / - / - / - | 39276 / - / - / - |  |
| 428 | Show of Strength | Spell | 27111 / - / - / 26813 | 28637 / - / - / 28244 | 27299 / - / - / 26986 | 0 / - / - / 0 |  |
| 429 | Strike Down | Spell | 4432 / - / - / 4327 | 4433 / - / - / 4328 | 5799 / - / - / 5665 | 0 / - / - / 0 |  |
| 430 | Warmog's Armor | Gear | 24207 / 11906 / - / 21741 | 25189 / 13404 / - / 22080 | 26062 / 17656 / - / 24413 | 0 / 0 / - / 0 |  |
| 431 | Akshan, Mischievous | Unit | 23020 / 21052 / - / 21051 | 21484 / 20820 / - / 20819 | 23349 / 23349 / - / 23349 | 0 / 0 / - / 0 |  |
| 432 | Fiora, Peerless | Unit | 37114 / 6336 / - / 6336 | 35247 / 4648 / - / 4648 | 38275 / 7278 / - / 7278 | 0 / 0 / - / 0 |  |
| 433 | Here to Help | Spell | 25360 / - / - / 25040 | 29068 / - / - / 28722 | 27499 / - / - / 27128 | 0 / - / - / 0 |  |
| 434 | Kato the Arm | Unit | 22424 / 13979 / - / 13979 | 20730 / 12712 / - / 12712 | 22646 / 15199 / - / 15199 | 0 / 0 / - / 0 |  |
| 435 | Lucian, Merciless | Unit | 44474 / 42357 / - / 42357 | 41686 / 40988 / - / 40988 | 44677 / 44677 / - / 44677 | 41959 / 38888 / - / 38877 |  |
| 436 | Marching Orders | Spell | 20987 / - / - / 20524 | 22459 / - / - / 21905 | 21539 / - / - / 21136 | 0 / - / - / 0 |  |
| 437 | Trinity Force | Gear | 19769 / 10221 / - / 17465 | 18741 / 9623 / - / 16062 | 21150 / 12232 / - / 20091 | 25615 / 15026 / - / 23391 |  |
| 438 | Ancient Henge | Gear | 23897 / - / 23271 / 23271 | 22219 / - / 21884 / 21884 | 26418 / - / 26416 / 26416 | 0 / - / 0 / 0 |  |
| 439 | Boneshiver | Gear | 22720 / 9555 / - / 18933 | 20927 / 8854 / - / 16516 | 23699 / 13769 / - / 20985 | 0 / 0 / - / 0 |  |
| 440 | Jax, Unrelenting | Unit | 38931 / 35990 / - / 35990 | 36430 / 35392 / - / 35392 | 40944 / 40944 / - / 40944 | 0 / 0 / - / 0 |  |
| 441 | Sivir, Ambitious | Unit | 31246 / 1293 / - / 1289 | 29326 / 2035 / - / 2027 | 30494 / 3559 / - / 3548 | 0 / 0 / - / 0 |  |
| 442 | Black Market Broker | Unit | 25111 / 2711 / - / 2711 | 22381 / 2768 / - / 2768 | 24383 / 3507 / - / 3507 | 0 / 0 / - / 0 |  |
| 443 | Called Shot | Spell | 0 / - / - / 0 | 0 / - / - / 0 | 0 / - / - / 0 | 0 / - / - / 0 | banned |
| 444 | Corrupt Enforcer | Unit | 22610 / 16933 / - / 16932 | 22456 / 16589 / - / 16587 | 23874 / 18985 / - / 18985 | 0 / 0 / - / 0 |  |
| 445 | Doran's Ring | Gear | 23001 / 11037 / - / 21063 | 24570 / 12865 / - / 22175 | 26650 / 18407 / - / 25714 | 0 / 0 / - / 0 |  |
| 446 | Fae Porter | Unit | 22579 / 7051 / - / 7051 | 20210 / 7828 / - / 7828 | 21745 / 10344 / - / 10344 | 0 / 0 / - / 0 |  |
| 447 | Loyal Pup | Unit | 22452 / 4398 / - / 4398 | 22691 / 5046 / - / 5046 | 24337 / 7898 / - / 7898 | 0 / 0 / - / 0 |  |
| 448 | Master Bingwen | Unit | 19343 / 17709 / - / 17709 | 17284 / 16669 / - / 16669 | 17379 / 17379 / - / 17379 | 0 / 0 / - / 0 |  |
| 449 | Overzealous Fan | Unit | 24886 / 5160 / - / 5116 | 24973 / 6798 / - / 6708 | 27264 / 11255 / - / 11124 | 25316 / 7216 / - / 7069 |  |
| 450 | Temptation | Spell | 14381 / - / - / 14057 | 11621 / - / - / 11327 | 16709 / - / - / 16422 | 0 / - / - / 0 |  |
| 451 | Treasure Hunter | Unit | 25101 / 19117 / - / 19117 | 24188 / 18371 / - / 18371 | 25672 / 20950 / - / 20950 | 90360 / 69650 / - / 69650 |  |
| 452 | Ancient Warmonger | Unit | 20660 / - / - / - | 18635 / - / - / - | 21224 / - / - / - | 0 / - / - / - |  |
| 453 | Beast Below | Unit | 16985 / 16885 / - / 16804 | 14413 / 14292 / - / 14218 | 14518 / 14422 / - / 14364 | 0 / 0 / - / 0 |  |
| 454 | Boots of Swiftness | Gear | 21857 / - / - / - | 20988 / - / - / - | 24024 / - / - / - | 49362 / - / - / - |  |
| 455 | Cull | Gear | 26117 / 12461 / - / 23911 | 24325 / 12815 / - / 21964 | 27602 / 18824 / - / 26559 | 0 / 0 / - / 0 |  |
| 456 | Factory Recall | Spell | 25257 / - / - / 24834 | 23858 / - / - / 23491 | 25968 / - / - / 25629 | 0 / - / - / 0 |  |
| 457 | Hard Bargain | Spell | 17498 / - / - / 17401 | 21585 / - / - / 21469 | 19304 / - / - / 19232 | 74558 / - / - / 72906 |  |
| 458 | Harpoon Squad | Unit | 22310 / 8281 / - / 8281 | 21293 / 7308 / - / 7308 | 22254 / 9744 / - / 9744 | 0 / 0 / - / 0 |  |
| 459 | Windsinger | Unit | 25738 / 7427 / - / 7420 | 24638 / 10403 / - / 10375 | 27619 / 15136 / - / 15100 | 27294 / 15313 / - / 15274 |  |
| 460 | Edge of Night | Gear | 24008 / 9807 / - / 22357 | 23713 / 10670 / - / 22049 | 25172 / 13874 / - / 24514 | 50846 / 22778 / - / 48149 |  |
| 461 | Fizz, Trickster | Unit | 23229 / 7174 / - / 7174 | 22633 / 10700 / - / 10700 | 23111 / 13466 / - / 13466 | 209412 / 148505 / - / 148505 |  |
| 462 | Irelia, Graceful | Unit | 41282 / - / - / - | 34683 / - / - / - | 38414 / - / - / - | 0 / - / - / - |  |
| 463 | Jae Medarda | Unit | 19731 / 2040 / - / 2040 | 17250 / 1923 / - / 1923 | 18656 / 2035 / - / 2035 | 0 / 0 / - / 0 |  |
| 464 | Sivir, Mercenary | Unit | 38912 / - / - / - | 36181 / - / - / - | 40729 / - / - / - | 41346 / - / - / - |  |
| 465 | Spirit Wheel | Gear | 22747 / 9825 / - / 9825 | 22841 / 12571 / - / 12571 | 25072 / 15789 / - / 15789 | 27498 / 20576 / - / 20576 |  |
| 466 | Switcheroo | Spell | 21227 / - / - / 20899 | 22578 / - / - / 22162 | 21554 / - / - / 21233 | 216807 / - / - / 213583 |  |
| 467 | Vex, Cheerless | Unit | 30604 / - / - / - | 28052 / - / - / - | 29654 / - / - / - | 0 / - / - / - |  |
| 468 | Downwell | Spell | 10617 / - / - / 10473 | 9803 / - / - / 9621 | 9139 / - / - / 9009 | 4947 / - / - / 4919 |  |
| 469 | Draven, Audacious | Unit | 31915 / 19289 / - / 19289 | 30709 / 15902 / - / 15902 | 30035 / 18515 / - / 18515 | 0 / 0 / - / 0 |  |
| 470 | Ezreal, Prodigy | Unit | 41130 / 41130 / - / 41130 | 40391 / 40391 / - / 40391 | 41692 / 41692 / - / 41692 | 121948 / 121948 / - / 121947 |  |
| 471 | Last Rites | Gear | 21326 / 5233 / - / 17972 | 20001 / 6468 / - / 16186 | 23822 / 10557 / - / 21844 | 57217 / 30652 / - / 52593 |  |
| 472 | Bonds of Strength | Spell | 27563 / - / - / 27197 | 27104 / - / - / 26703 | 27251 / - / - / 26973 | 0 / - / - / 0 |  |
| 473 | Eminent Benefactor | Unit | 20046 / 8685 / - / 8677 | 17976 / 8016 / - / 7998 | 19601 / 7822 / - / 7809 | 0 / 0 / - / 0 |  |
| 474 | Eye of the Herald | Gear | 24595 / 18916 / - / 22858 | 25006 / 18729 / - / 22867 | 27054 / 23087 / - / 26126 | 70680 / 58032 / - / 66156 |  |
| 475 | Guards! | Spell | 24286 / 11892 / - / 23981 | 23882 / 17722 / - / 23528 | 26123 / 22643 / - / 25814 | 33977 / 23082 / - / 31014 |  |
| 476 | Honest Broker | Unit | 25825 / 16792 / - / 16792 | 25254 / 15143 / - / 15142 | 28033 / 19605 / - / 19605 | 218542 / 168429 / - / 168426 |  |
| 477 | Laurent Duelist | Unit | 22478 / - / - / - | 20779 / - / - / - | 24367 / - / - / - | 0 / - / - / - |  |
| 478 | Royal Guard | Unit | 23815 / 23815 / - / 23815 | 22553 / 22553 / - / 22553 | 24350 / 24350 / - / 24350 | 0 / 0 / - / 0 |  |
| 479 | Sandshifter | Unit | 19984 / 13235 / - / 13194 | 20451 / 13609 / - / 13525 | 20519 / 13251 / - / 13188 | 0 / 0 / - / 0 |  |
| 480 | Trusty Ramhound | Unit | 25954 / - / - / - | 25507 / - / - / - | 28210 / - / - / - | 121078 / - / - / - |  |
| 481 | Zaun Punk | Unit | 25004 / 4025 / - / 4019 | 23758 / 5666 / - / 5656 | 25894 / 8315 / - / 8306 | 0 / 0 / - / 0 |  |
| 482 | B.F. Sword | Gear | 20088 / - / - / - | 19259 / - / - / - | 21188 / - / - / - | 67739 / - / - / - |  |
| 483 | Blood Money | Spell | 20283 / - / - / 19962 | 21755 / - / - / 21307 | 21673 / - / - / 21284 | 35086 / - / - / 34060 |  |
| 484 | Deathgrip | Spell | 28863 / - / - / 28496 | 29280 / - / - / 28804 | 29443 / - / - / 29062 | 97762 / - / - / 93454 |  |
| 485 | Drag Under | Spell | 25164 / - / - / 24798 | 26193 / - / - / 25753 | 25323 / - / - / 24995 | 31778 / - / - / 31490 |  |
| 486 | Glasc Mixologist | Unit | 21177 / 3682 / - / 3679 | 20445 / 4879 / - / 4878 | 21640 / 8341 / - / 8339 | 29559 / 19282 / - / 19280 |  |
| 487 | Rally the Troops | Spell | 27222 / - / - / 26907 | 28554 / - / - / 28220 | 27427 / - / - / 27146 | 0 / - / - / 0 |  |
| 488 | Unsung Hero | Unit | 26497 / 17063 / - / 17062 | 25630 / 15288 / - / 15286 | 27430 / 19437 / - / 19435 | 0 / 0 / - / 0 |  |
| 489 | Vanguard Armory | Gear | 14452 / - / 11533 / 11533 | 13270 / - / 10966 / 10966 | 14109 / - / 13871 / 13871 | 24808 / - / 22170 / 22170 |  |
| 490 | Altar of Memories | Gear | 23274 / 16163 / - / 16161 | 23682 / 17675 / - / 17672 | 27305 / 22780 / - / 22779 | 0 / 0 / - / 0 |  |
| 491 | Rek'Sai, Swarm Queen | Unit | 36943 / 6245 / - / 6245 | 33727 / 7997 / - / 7997 | 36371 / 13412 / - / 13412 | 0 / 0 / - / 0 |  |
| 492 | Renata Glasc, Industrialist | Unit | 39687 / - / - / - | 38195 / - / - / - | 41014 / - / - / - | 14959 / - / - / - |  |
| 493 | Sacred Shears | Gear | 23101 / 11373 / - / 20775 | 24055 / 10873 / - / 21188 | 25240 / 15093 / - / 23854 | 0 / 0 / - / 0 |  |
| 494 | Trove Golem | Unit | 15730 / 15730 / - / 15730 | 13322 / 13322 / - / 13322 | 13755 / 13755 / - / 13755 | 0 / 0 / - / 0 |  |
| 495 | Undertitan | Unit | 18917 / 20145 / - / 20145 | 18466 / 20036 / - / 20036 | 18104 / 19989 / - / 19989 | 15656 / 20593 / - / 20593 |  |
| 496 | Xin Zhao, Vigilant | Unit | 25215 / - / - / - | 23445 / - / - / - | 26700 / - / - / - | 31654 / - / - / - |  |
| 497 | Azir, Sovereign | Unit | 41361 / 14778 / - / 14778 | 39026 / 13359 / - / 13359 | 42902 / 19809 / - / 19809 | 57356 / 19942 / - / 19942 |  |
| 498 | Blade of the Ruined King | Gear | 21622 / - / - / - | 21439 / - / - / - | 24276 / - / - / - | 0 / - / - / - |  |
| 499 | Corina Veraza | Unit | 17927 / 12193 / - / 12193 | 15706 / 11312 / - / 11312 | 16577 / 14034 / - / 14034 | 21311 / 14408 / - / 14408 |  |
| 500 | Fiora, Worthy | Unit | 36532 / 17145 / - / 17145 | 35607 / 20875 / - / 20875 | 37616 / 23366 / - / 23366 | 45529 / 33282 / - / 33282 |  |
| 501 | Danger Zone | Spell | 17326 / - / - / 17242 | 17252 / - / - / 17154 | 16573 / - / - / 16521 | 0 / - / - / 0 |  |
| 502 | Purifier | Legend | 41880 / - / - / - | 38600 / - / - / - | 40740 / - / - / - | 46200 / - / - / - |  |
| 503 | Relentless Pursuit | Spell | 15928 / - / - / 15809 | 14897 / - / - / 14787 | 15898 / - / - / 15839 | 31401 / - / - / 30635 |  |
| 504 | Spinning Axe | Gear | 16233 / - / - / - | 18092 / - / - / - | 16328 / - / - / - | 34304 / - / - / - |  |
| 505 | Void Rush | Spell | 14530 / - / - / 14456 | 15287 / - / - / 15195 | 17354 / - / - / 17298 | 32654 / - / - / 32198 |  |
| 506 | Fire Below the Mountain | Legend | 41340 / - / 41340 / 41340 | 39320 / - / 39320 / 39320 | 41660 / - / 41660 / 41660 | 46200 / - / 46200 / 46200 |  |
| 507 | Forgefire Cape | Gear | 4404 / 2484 / - / 3902 | 3852 / 1885 / - / 3252 | 5052 / 3273 / - / 4856 | 14649 / 8225 / - / 11419 |  |
| 508 | Rabadon's Deathcrown | Gear | 4470 / - / - / - | 3795 / - / - / - | 4952 / - / - / - | 14614 / - / - / - |  |
| 509 | Shurelya's Requiem | Gear | 4170 / 4170 / - / 4170 | 3839 / 3839 / - / 3839 | 5099 / 5099 / - / 5099 | 14692 / 14692 / - / 14692 |  |
| 510 | Counter Strike | Spell | 15598 / - / - / 15353 | 16957 / - / - / 16620 | 16524 / - / - / 16233 | 0 / - / - / 0 |  |
| 511 | Defiant Dance | Spell | 16538 / - / - / 16184 | 17043 / - / - / 16588 | 17481 / - / - / 17091 | 37356 / - / - / 34844 |  |
| 512 | Emperor of the Sands | Legend | 41380 / - / 5837 / 5837 | 41520 / - / 6614 / 6614 | 42660 / - / 10112 / 10112 | 46200 / - / 41176 / 41176 |  |
| 513 | Arise! | Spell | 10062 / 2339 / - / 9975 | 9139 / 2195 / - / 9024 | 10186 / 2463 / - / 10084 | 25054 / 24396 / - / 24966 |  |
| 514 | Prodigal Explorer | Legend | 39980 / - / 7600 / 7600 | 40820 / - / 8904 / 8904 | 40980 / - / 9484 / 9484 | 46200 / - / 36544 / 36544 |  |
| 515 | Arcane Shift | Spell | 12771 / - / - / 12533 | 13743 / - / - / 13400 | 14109 / - / - / 13808 | 0 / - / - / 0 |  |
| 516 | Hostile Takeover | Spell | 9117 / - / - / 9015 | 9167 / - / - / 8981 | 10464 / - / - / 10262 | 9728 / - / - / 9469 |  |
| 517 | Battle Mistress | Legend | 41340 / 41223 / - / 41223 | 40740 / 40683 / - / 40683 | 40800 / 40799 / - / 40799 | 46200 / 46176 / - / 46176 |  |
| 518 | On the Hunt | Spell | 15120 / - / - / 14973 | 14658 / - / - / 14523 | 16551 / - / - / 16461 | 0 / - / - / 0 |  |
| 519 | Grand Duelist | Legend | 39300 / 33303 / - / 33303 | 38340 / 36016 / - / 36016 | 38800 / 38197 / - / 38197 | 46200 / 45528 / - / 45528 |  |
| 520 | Riposte | Spell | 11372 / - / - / 11338 | 11836 / - / - / 11805 | 11733 / - / - / 11710 | 26064 / - / - / 26031 |  |
| 521 | Emperor's Dais | Battlefield | 33406 / 22487 / - / 22484 | 34591 / 29226 / - / 29225 | 32172 / 30789 / - / 30787 | 30453 / 23740 / - / 23740 |  |
| 522 | Forge of the Fluft | Battlefield | 33056 / - / 8599 / 8599 | 32607 / - / 7375 / 7375 | 33827 / - / 11546 / 11545 | 45466 / - / 22675 / 22675 |  |
| 523 | Forgotten Monument | Battlefield | 32844 / - / - / - | 33002 / - / - / - | 33108 / - / - / - | 45083 / - / - / - |  |
| 524 | Hall of Legends | Battlefield | 31813 / 21361 / - / 21359 | 33575 / 28305 / - / 28305 | 32334 / 30995 / - / 30995 | 15164 / 12225 / - / 12222 |  |
| 525 | Marai Spire | Battlefield | 33613 / - / - / - | 33678 / - / - / - | 34311 / - / - / - | 0 / - / - / - |  |
| 526 | Minefield | Battlefield | 33372 / 33740 / - / 33740 | 33665 / 33937 / - / 33937 | 33464 / 33860 / - / 33860 | 30275 / 30806 / - / 30806 |  |
| 527 | Ornn's Forge | Battlefield | 32744 / - / - / - | 33371 / - / - / - | 33117 / - / - / - | 15313 / - / - / - |  |
| 528 | Power Nexus | Battlefield | 33781 / 26789 / - / 26774 | 32903 / 30176 / - / 30164 | 32497 / 31227 / - / 31220 | 0 / 0 / - / 0 |  |
| 529 | Ravenbloom Conservatory | Battlefield | 34027 / 27208 / - / 27208 | 33197 / 23046 / - / 23046 | 32666 / 26992 / - / 26992 | 0 / 0 / - / 0 |  |
| 530 | Rockfall Path | Battlefield | 34467 / - / - / - | 32896 / - / - / - | 33566 / - / - / - | 74685 / - / - / - |  |
| 531 | Seat of Power | Battlefield | 32738 / 33127 / - / 33127 | 34211 / 34416 / - / 34416 | 34041 / 34454 / - / 34454 | 151410 / 163434 / - / 163434 |  |
| 532 | Sunken Temple | Battlefield | 31918 / 11333 / - / 11332 | 34359 / 17149 / - / 17143 | 32075 / 21781 / - / 21778 | 89448 / 46382 / - / 46341 |  |
| 533 | The Papertree | Battlefield | 33068 / 32471 / - / 32461 | 33315 / 32296 / - / 32286 | 32364 / 32038 / - / 32034 | 0 / 0 / - / 0 |  |
| 534 | Treasure Hoard | Battlefield | 33697 / 22691 / - / 22691 | 33209 / 28075 / - / 28074 | 33201 / 31710 / - / 31710 | 15260 / 13108 / - / 13108 |  |
| 535 | Veiled Temple | Battlefield | 33358 / 14342 / - / 14341 | 33015 / 18809 / - / 18806 | 32628 / 24317 / - / 24317 | 15283 / 10333 / - / 10333 |  |
| 536 | Seal of Rage | Gear | 24235 / - / 23665 / 23665 | 24427 / - / 24138 / 24138 | 26287 / - / 26281 / 26281 | 0 / - / 0 / 0 |  |
| 537 | Vayne, Hunter | Unit | 21105 / 4069 / - / 4067 | 18207 / 5717 / - / 5717 | 22107 / 9526 / - / 9523 | 0 / 0 / - / 0 |  |
| 538 | Seal of Focus | Gear | 25326 / - / 24743 / 24743 | 25474 / - / 25187 / 25187 | 27487 / - / 27483 / 27483 | 0 / - / 0 / 0 |  |
| 539 | Ahri, Inquisitive | Unit | 35848 / 23575 / - / 23417 | 33736 / 19502 / - / 19330 | 35742 / 24999 / - / 24785 | 0 / 0 / - / 0 |  |
| 540 | Bard, Mercurial | Unit | 22635 / 10618 / - / 10618 | 20833 / 14634 / - / 14634 | 21671 / 17787 / - / 17787 | 0 / 0 / - / 0 |  |
| 541 | Seal of Insight | Gear | 25363 / - / 24795 / 24795 | 26472 / - / 26130 / 26130 | 27325 / - / 27325 / 27325 | 0 / - / 0 / 0 |  |
| 542 | Seal of Strength | Gear | 25217 / - / 24663 / 24663 | 25798 / - / 25498 / 25498 | 28907 / - / 28903 / 28903 | 0 / - / 0 / 0 |  |
| 543 | Sett, Brawler | Unit | 31317 / 31317 / 23919 / 31317 | 28819 / 28819 / 23179 / 28819 | 30811 / 30811 / 30134 / 30811 | 0 / 0 / 0 / 0 |  |
| 544 | Yone, Blademaster | Unit | 19380 / 17916 / - / 17916 | 18661 / 18110 / - / 18108 | 20946 / 20946 / - / 20946 | 0 / 0 / - / 0 |  |
| 545 | Seal of Discord | Gear | 24595 / - / 24047 / 24047 | 25829 / - / 25529 / 25529 | 27587 / - / 27586 / 27586 | 0 / - / 0 / 0 |  |
| 546 | Yasuo, Windrider | Unit | 30308 / 143 / - / 143 | 27358 / 169 / - / 169 | 29223 / 335 / - / 335 | 0 / 0 / - / 0 | random:resolved, random:triggered, rp:resolved, rp:triggered |
| 547 | Darius, Executioner | Unit | 29852 / 7420 / - / 7420 | 26489 / 6664 / - / 6664 | 29416 / 5644 / - / 5644 | 0 / 0 / - / 0 |  |
| 548 | Karma, Channeler | Unit | 19117 / 11477 / - / 11477 | 17928 / 15035 / - / 15035 | 19437 / 19286 / - / 19286 | 0 / 0 / - / 0 |  |
| 549 | Seal of Unity | Gear | 26819 / - / 26231 / 26231 | 27071 / - / 26732 / 26732 | 27874 / - / 27868 / 27868 | 0 / - / 0 / 0 |  |
| 550 | Soraka, Wanderer | Unit | 22916 / - / - / - | 21629 / - / - / - | 23781 / - / - / - | 0 / - / - / - |  |
| 551 | Mechanized Menace | Legend | 40320 / - / - / - | 40520 / - / - / - | 40460 / - / - / - | 46200 / - / - / - |  |
| 552 | Glorious Executioner | Legend | 37960 / 28287 / - / 28206 | 41240 / 27737 / - / 27645 | 38080 / 29788 / - / 29728 | 46200 / 33733 / - / 33733 |  |
| 553 | Void Burrower | Legend | 40880 / 31217 / - / 31215 | 40700 / 37156 / - / 37156 | 40560 / 40156 / - / 40156 | 46200 / 41946 / - / 41946 |  |
| 554 | Grandmaster at Arms | Legend | 37860 / - / 13234 / 13234 | 41060 / - / 14226 / 14226 | 40660 / - / 16525 / 16525 | 0 / - / 0 / 0 |  |
| 555 | Blade Dancer | Legend | 41140 / 28186 / - / 28186 | 39700 / 32785 / - / 32785 | 41820 / 37554 / - / 37554 | 46200 / 44803 / - / 44803 |  |
| 556 | Chem-Baroness | Legend | 39200 / 29364 / - / 29354 | 41600 / 35943 / - / 35941 | 40680 / 36888 / - / 36888 | 46200 / 35659 / - / 35659 |  |
| 557 | Arena Kingpin | Unit | 20054 / - / 16647 / 16634 | 17571 / - / 13353 / 13335 | 19751 / - / 16099 / 16087 | 0 / - / 0 / 0 |  |
| 558 | Baron Pit | Battlefield | 0 / - / - / - | 0 / - / - / - | 0 / - / - / - | 0 / - / - / - | token card |
| 559 | Bird | Unit | 0 / - / - / - | 0 / - / - / - | 0 / - / - / - | 0 / - / - / - | token card |
| 560 | Inferna | Unit | 27078 / - / - / - | 28314 / - / - / - | 28102 / - / - / - | 102037 / - / - / - |  |
| 561 | Brush | Battlefield | 22492 / 31010 / - / 30996 | 23889 / 37595 / - / 37588 | 25773 / 39266 / - / 39265 | 59924 / 84821 / - / 84820 | token card |
| 562 | Mischievous Marai | Unit | 26561 / 1170 / - / 1159 | 23944 / 1087 / - / 1080 | 26557 / 1866 / - / 1839 | 56451 / 3170 / - / 3094 |  |
| 563 | Prepared Neophyte | Unit | 23595 / - / - / - | 21684 / - / - / - | 23449 / - / - / - | 0 / - / - / - |  |
| 564 | Gold | Gear | 0 / - / 0 / 0 | 0 / - / 0 / 0 | 0 / - / 0 / 0 | 0 / - / 0 / 0 | token card |
| 565 | Revna the Lorekeeper | Unit | 15531 / 2700 / - / 2700 | 14619 / 2534 / - / 2534 | 14587 / 2201 / - / 2201 | 0 / 0 / - / 0 |  |
| 566 | Reflection | Unit | 0 / - / - / - | 0 / - / - / - | 0 / - / - / - | 0 / - / - / - | token card |
| 567 | Sharkling | Unit | 22478 / - / - / - | 23016 / - / - / - | 24214 / - / - / - | 0 / - / - / - |  |
| 568 | Smite | Spell | 26338 / - / - / 26075 | 27581 / - / - / 27166 | 25671 / - / - / 25450 | 0 / - / - / 0 |  |
| 569 | Sprite | Unit | 0 / - / - / - | 0 / - / - / - | 0 / - / - / - | 0 / - / - / - | token card |
| 570 | Towering Pairofant | Unit | 17459 / - / - / - | 15939 / - / - / - | 16697 / - / - / - | 0 / - / - / - |  |
| 571 | Upstage Comedy | Spell | 22287 / - / - / 22093 | 21966 / - / - / 21721 | 24210 / - / - / 24062 | 0 / - / - / 0 |  |
| 572 | Vault Breaker | Spell | 27921 / - / - / 27671 | 27945 / - / - / 27584 | 27824 / - / - / 27644 | 0 / - / - / 0 |  |
| 573 | Fresh Beans | Gear | 22707 / 487 / - / 487 | 22094 / 1043 / - / 1043 | 24875 / 1355 / - / 1355 | 0 / 0 / - / 0 |  |
| 574 | Lord Broadmane | Unit | 22928 / 22928 / - / 22928 | 24021 / 24021 / - / 24021 | 22344 / 22344 / - / 22344 | 0 / 0 / - / 0 |  |
| 575 | Lotus Trap | Spell | 26049 / - / - / 25800 | 27900 / - / - / 27587 | 25177 / - / - / 24968 | 0 / - / - / 0 |  |
| 576 | Monster Harpoon | Spell | 28141 / - / - / 27912 | 27720 / - / - / 27419 | 26266 / - / - / 26027 | 0 / - / - / 0 |  |
| 577 | Right of Conquest | Spell | 20628 / - / - / 20428 | 18500 / - / - / 18297 | 21847 / - / - / 21683 | 0 / - / - / 0 |  |
| 578 | Scorchclaw | Unit | 23868 / 13618 / - / 13612 | 20307 / 11922 / - / 11917 | 22474 / 13316 / - / 13315 | 0 / 0 / - / 0 |  |
| 579 | Square Up | Spell | 25920 / - / - / 25641 | 25887 / - / - / 25520 | 22987 / - / - / 22804 | 0 / - / - / 0 |  |
| 580 | Yeti Brawler | Unit | 16974 / 1812 / - / 1811 | 16198 / 1611 / - / 1611 | 17099 / 2323 / - / 2322 | 0 / 0 / - / 0 |  |
| 581 | Blighted Battleaxe | Gear | 19488 / 14831 / - / 16153 | 16545 / 12084 / - / 13197 | 19509 / 15256 / - / 17256 | 12748 / 9488 / - / 10605 |  |
| 582 | Dancing Grenade | Spell | 21627 / - / - / 21418 | 21046 / - / - / 20798 | 23638 / - / - / 23467 | 0 / - / - / 0 |  |
| 583 | Grim Apothecary | Unit | 25107 / 9304 / - / 9304 | 25684 / 15313 / - / 15313 | 25912 / 17623 / - / 17623 | 32474 / 22468 / - / 22468 |  |
| 584 | Jhin, Murderous Artist | Unit | 39435 / 31194 / - / 31194 | 35253 / 27203 / - / 27203 | 36368 / 30156 / - / 30156 | 28811 / 22039 / - / 22039 |  |
| 585 | Katarina, Reckless | Unit | 20102 / 1594 / - / 1594 | 18467 / 1468 / - / 1468 | 18140 / 1383 / - / 1383 | 0 / 0 / - / 0 |  |
| 586 | Rengar, Unseen | Unit | 33666 / - / - / - | 29320 / - / - / - | 33376 / - / - / - | 15463 / - / - / - |  |
| 587 | Undying Legion | Unit | 23732 / - / - / - | 22693 / - / - / - | 25471 / - / - / - | 0 / - / - / - |  |
| 588 | Xerath, Freed | Unit | 20316 / - / 3748 / 3740 | 18335 / - / 2694 / 2693 | 19666 / - / 3643 / 3640 | 0 / - / 0 / 0 |  |
| 589 | Inviolus Vox | Unit | 14416 / 4760 / - / 4759 | 12458 / 4362 / - / 4360 | 12100 / 4909 / - / 4908 | 0 / 0 / - / 0 |  |
| 590 | Pyke, Dockside Butcher | Unit | 39936 / 13885 / - / 13885 | 40969 / 19693 / - / 19693 | 43228 / 21021 / - / 21021 | 70918 / 33110 / - / 33110 |  |
| 591 | Red Brambleback | Unit | 22058 / 9259 / - / 9258 | 19915 / 9685 / - / 9684 | 20435 / 11633 / - / 11631 | 0 / 0 / - / 0 |  |
| 592 | Vi, Hotheaded | Unit | 32931 / - / 17378 / 17378 | 29682 / - / 14804 / 14804 | 32448 / - / 20557 / 20557 | 0 / - / 0 / 0 |  |
| 593 | Combat Experience | Spell | 28597 / - / - / 27960 | 28525 / - / - / 27785 | 28725 / - / - / 27912 | 0 / - / - / 0 |  |
| 594 | Double Trouble | Spell | 24920 / - / - / 24528 | 22393 / - / - / 21997 | 26568 / - / - / 26194 | 0 / - / - / 0 |  |
| 595 | Frisky Hunter | Unit | 22595 / 22595 / - / 22595 | 21871 / 21871 / - / 21871 | 23396 / 23396 / - / 23396 | 59800 / 59800 / - / 59800 |  |
| 596 | Herald of Spring | Unit | 22874 / 22874 / - / 22874 | 20631 / 20631 / - / 20631 | 24090 / 24090 / - / 24090 | 0 / 0 / - / 0 |  |
| 597 | Monch | Unit | 19321 / - / - / - | 17398 / - / - / - | 19587 / - / - / - | 0 / - / - / - |  |
| 598 | Mutated Mouser | Unit | 25921 / - / - / - | 25221 / - / - / - | 27967 / - / - / - | 124494 / - / - / - |  |
| 599 | Shadow Watcher | Unit | 22137 / - / - / - | 20549 / - / - / - | 24481 / - / - / - | 0 / - / - / - |  |
| 600 | Skyward Strike | Spell | 21475 / - / - / 20969 | 21094 / - / - / 20527 | 24224 / - / - / 23666 | 0 / - / - / 0 |  |
| 601 | Soul Sword | Gear | 25117 / - / - / - | 25211 / - / - / - | 27053 / - / - / - | 35287 / - / - / - |  |
| 602 | Wuju Apprentice | Unit | 24656 / 15293 / - / 15279 | 26014 / 16772 / - / 16768 | 25724 / 16995 / - / 16976 | 0 / 0 / - / 0 |  |
| 603 | Allay, Eager Admirer | Unit | 23376 / - / - / - | 23485 / - / - / - | 25010 / - / - / - | 0 / - / - / - |  |
| 604 | Back Off | Spell | 25901 / - / - / 25336 | 26246 / - / - / 25500 | 27059 / - / - / 26366 | 171441 / - / - / 152856 |  |
| 605 | Enthusiastic Promoter | Unit | 23002 / 11516 / - / 11483 | 23517 / 12036 / - / 12019 | 24732 / 11607 / - / 11594 | 0 / 0 / - / 0 |  |
| 606 | Flurry of Feathers | Spell | 26409 / - / - / 26128 | 25369 / - / - / 25025 | 26086 / - / - / 25831 | 0 / - / - / 0 |  |
| 607 | Forgotten Signpost | Gear | 23745 / - / 15448 / 15448 | 22682 / - / 13603 / 13603 | 26470 / - / 14337 / 14337 | 33125 / - / 17887 / 17887 |  |
| 608 | Friendship | Spell | 26870 / - / - / 26314 | 27559 / - / - / 26829 | 27964 / - / - / 27241 | 28335 / - / - / 25122 |  |
| 609 | Mosstomper | Unit | 22621 / 13320 / - / 13314 | 22089 / 13315 / - / 13304 | 25989 / 16018 / - / 16005 | 0 / 0 / - / 0 |  |
| 610 | Trevor Snoozebottom | Unit | 24256 / 12439 / - / 12415 | 23387 / 12082 / - / 12066 | 25260 / 12572 / - / 12555 | 0 / 0 / - / 0 |  |
| 611 | Honeyfruit | Gear | 23063 / - / 20098 / 20098 | 23141 / - / 20588 / 20588 | 26416 / - / 23769 / 23769 | 0 / - / 0 / 0 |  |
| 612 | Iascylla | Unit | 16661 / 7544 / - / 7524 | 15753 / 7036 / - / 7025 | 16210 / 6538 / - / 6520 | 0 / 0 / - / 0 |  |
| 613 | Ivern, Nurturer | Unit | 36433 / 36433 / - / 36433 | 33327 / 33327 / - / 33327 | 36967 / 36967 / - / 36967 | 80247 / 80247 / - / 80247 |  |
| 614 | Nami, Headstrong | Unit | 22800 / 16206 / - / 16182 | 23300 / 19553 / - / 19521 | 24469 / 21866 / - / 21833 | 25906 / 21627 / - / 21560 |  |
| 615 | Scuttle Crab | Unit | 24881 / 24881 / - / 24881 | 24163 / 24163 / - / 24163 | 27420 / 27420 / - / 27420 | 175521 / 175521 / - / 175519 |  |
| 616 | Tricksy Tentacles | Spell | 19872 / - / - / 19390 | 19021 / - / - / 18451 | 21651 / - / - / 21062 | 0 / - / - / 0 |  |
| 617 | Vex, Mocking | Unit | 31207 / 510 / - / 510 | 27861 / 776 / - / 776 | 31090 / 905 / - / 905 | 0 / 0 / - / 0 |  |
| 618 | Yuumi, Magical Cat | Unit | 24010 / 9921 / - / 9921 | 22481 / 8805 / - / 8805 | 25018 / 11540 / - / 11540 | 0 / 0 / - / 0 |  |
| 619 | Alpha Wildclaw | Unit | 17556 / - / - / - | 17549 / - / - / - | 17709 / - / - / - | 0 / - / - / - |  |
| 620 | Lillia, Protector of Dreams | Unit | 34631 / 10247 / - / 10247 | 33258 / 9237 / - / 9237 | 37307 / 10755 / - / 10755 | 0 / 0 / - / 0 |  |
| 621 | Master Yi, Unstoppable | Unit | 9806 / - / - / - | 9592 / - / - / - | 7225 / - / - / - | 0 / - / - / - |  |
| 622 | Vilemaw | Unit | 21508 / 14266 / - / 14237 | 22681 / 16244 / - / 16206 | 19441 / 12243 / - / 12216 | 30356 / 20692 / - / 20692 |  |
| 623 | Downstage Dramatics | Spell | 28697 / - / - / 28389 | 29039 / - / - / 28635 | 27225 / - / - / 26965 | 0 / - / - / 0 |  |
| 624 | Dramatic Visionary | Unit | 22161 / 11883 / - / 11880 | 20895 / 10066 / - / 10065 | 22937 / 13280 / - / 13277 | 0 / 0 / - / 0 |  |
| 625 | Eclipse | Spell | 28620 / - / - / 28207 | 28806 / - / - / 28237 | 27440 / - / - / 27011 | 102856 / - / - / 95720 |  |
| 626 | Fate Weaver | Unit | 20441 / 20441 / - / 20441 | 19952 / 19952 / - / 19952 | 19550 / 19550 / - / 19550 | 0 / 0 / - / 0 |  |
| 627 | Icevale Archer | Unit | 24660 / 3851 / - / 3841 | 26040 / 5812 / - / 5799 | 28168 / 9680 / - / 9650 | 0 / 0 / - / 0 |  |
| 628 | Moonlight Affliction | Spell | 25691 / - / - / 25420 | 26450 / - / - / 26097 | 23670 / - / - / 23393 | 0 / - / - / 0 |  |
| 629 | Ruined Rex | Unit | 18785 / 6847 / - / 6810 | 17087 / 5558 / - / 5515 | 17726 / 6853 / - / 6805 | 25986 / 17414 / - / 17304 |  |
| 630 | Spectral Centaur | Unit | 19266 / 12189 / - / 12187 | 17200 / 9768 / - / 9767 | 17541 / 10589 / - / 10587 | 0 / 0 / - / 0 |  |
| 631 | Sprite Burst | Spell | 17959 / - / - / 17751 | 15468 / - / - / 15216 | 18127 / - / - / 17905 | 74095 / - / - / 73217 |  |
| 632 | Turn to Dust | Spell | 21880 / - / - / 21511 | 21204 / - / - / 20820 | 24619 / - / - / 24295 | 20735 / - / - / 20415 |  |
| 633 | Chakram Dancer | Unit | 26622 / 26622 / - / 26622 | 27434 / 27434 / - / 27434 | 28406 / 28406 / - / 28406 | 24200 / 24200 / - / 24200 |  |
| 634 | Crescent Strike | Spell | 23039 / - / - / 22494 | 23212 / - / - / 22623 | 23643 / - / - / 23103 | 0 / - / - / 0 |  |
| 635 | Deadly Flourish | Spell | 19033 / 9825 / - / 18659 | 17277 / 9082 / - / 16903 | 20184 / 10675 / - / 19725 | 51094 / 32698 / - / 50040 |  |
| 636 | Frigid Jewel | Gear | 23991 / 10653 / - / 10649 | 24030 / 10812 / - / 10808 | 26546 / 13387 / - / 13377 | 0 / 0 / - / 0 |  |
| 637 | Gustwalker | Unit | 24212 / 13732 / - / 13724 | 23287 / 13762 / - / 13760 | 23747 / 14279 / - / 14270 | 0 / 0 / - / 0 |  |
| 638 | Petal Pixie | Unit | 25021 / - / - / - | 24164 / - / - / - | 25411 / - / - / - | 0 / - / - / - |  |
| 639 | Soul Shepherd | Unit | 21598 / - / - / - | 19893 / - / - / - | 19783 / - / - / - | 0 / - / - / - |  |
| 640 | Sprite Fountain | Gear | 23100 / 23100 / - / 23100 | 23380 / 23380 / - / 23380 | 25129 / 25129 / - / 25129 | 211154 / 211154 / - / 211154 |  |
| 641 | Diana, Lunari | Unit | 42800 / 20105 / - / 20103 | 39536 / 24902 / - / 24902 | 41750 / 32437 / - / 32437 | 44625 / 30524 / - / 30524 |  |
| 642 | Hwei, Brooding Painter | Unit | 19693 / 13922 / - / 13922 | 19305 / 13091 / - / 13091 | 19431 / 14755 / - / 14755 | 63255 / 49450 / - / 49450 |  |
| 643 | Keeper of Masks | Unit | 26113 / 26113 / - / 26113 | 26055 / 26055 / - / 26055 | 27420 / 27420 / - / 27420 | 0 / 0 / - / 0 |  |
| 644 | Lillia, Fae Fawn | Unit | 42353 / 35973 / - / 35973 | 40173 / 35158 / - / 35158 | 42484 / 40691 / - / 40691 | 44695 / 42086 / - / 42086 |  |
| 645 | Smoke and Mirrors | Spell | 24096 / - / - / 23745 | 23074 / - / - / 22682 | 22702 / - / - / 22411 | 28157 / - / - / 25646 |  |
| 646 | Sprite Queen | Unit | 17541 / 17541 / - / 17541 | 15658 / 15658 / - / 15658 | 16016 / 16016 / - / 16016 | 0 / 0 / - / 0 |  |
| 647 | Sumpworks Map | Gear | 27984 / 15409 / - / 15409 | 30550 / 15896 / - / 15896 | 27486 / 17422 / - / 17422 | 0 / 0 / - / 0 |  |
| 648 | Zilean, Time Mage | Unit | 20691 / - / - / - | 19522 / - / - / - | 19793 / - / - / - | 0 / - / - / - |  |
| 649 | Blue Sentinel | Unit | 22099 / 11121 / - / 11113 | 20401 / 10392 / - / 10377 | 22198 / 10714 / - / 10700 | 24012 / 11576 / - / 11506 |  |
| 650 | Gutter Palace | Gear | 20990 / 275 / 17445 / 17523 | 19522 / 356 / 16467 / 16546 | 20814 / 76 / 19926 / 19926 | 26892 / 205 / 24480 / 24487 |  |
| 651 | Jhin, Meticulous Killer | Unit | 40722 / - / - / - | 39040 / - / - / - | 39949 / - / - / - | 34880 / - / - / - |  |
| 652 | LeBlanc, Everywhere at Once | Unit | 40067 / - / - / - | 37628 / - / - / - | 41463 / - / - / - | 0 / - / - / - |  |
| 653 | Concentrate | Spell | 17345 / - / - / 17156 | 16755 / - / - / 16518 | 17895 / - / - / 17674 | 0 / - / - / 0 |  |
| 654 | Demacian Diplomat | Unit | 24206 / 24206 / - / 24206 | 25015 / 25015 / - / 25015 | 25964 / 25964 / - / 25964 | 32768 / 32768 / - / 32768 |  |
| 655 | Dragonsoul Sage | Unit | 25472 / - / 16769 / 16769 | 25082 / - / 18301 / 18301 | 26251 / - / 17355 / 17355 | 0 / - / 0 / 0 |  |
| 656 | Gemhand Hunter | Unit | 26271 / 16060 / - / 16050 | 25176 / 15964 / - / 15957 | 26100 / 17266 / - / 17256 | 0 / 0 / - / 0 |  |
| 657 | Grim Resolve | Spell | 23798 / 2904 / - / 23406 | 27344 / 3518 / - / 26859 | 25108 / 4270 / - / 24719 | 78801 / 12362 / - / 77202 |  |
| 658 | Hunter's Machete | Gear | 23002 / 14938 / - / 20509 | 21634 / 14000 / - / 18887 | 23483 / 17724 / - / 22362 | 0 / 0 / - / 0 |  |
| 659 | Kinkou Initiate | Unit | 23705 / 23705 / - / 23705 | 22211 / 22211 / - / 22211 | 25863 / 25863 / - / 25863 | 127558 / 127558 / - / 127558 |  |
| 660 | Targonian Visionary | Unit | 19423 / - / - / - | 17930 / - / - / - | 19124 / - / - / - | 0 / - / - / - |  |
| 661 | Towering Combatant | Unit | 22054 / - / - / - | 21502 / - / - / - | 22418 / - / - / - | 0 / - / - / - |  |
| 662 | Voracious Gromp | Unit | 20666 / 12194 / - / 12190 | 19610 / 11654 / - / 11645 | 21767 / 13438 / - / 13430 | 0 / 0 / - / 0 |  |
| 663 | Call to Battle | Spell | 11520 / - / - / 11295 | 9926 / - / - / 9735 | 13503 / - / - / 13245 | 0 / - / - / 0 |  |
| 664 | Crowd Favorite | Unit | 22643 / 13588 / 7995 / 14358 | 23545 / 14659 / 8788 / 15503 | 26679 / 16995 / 12717 / 18368 | 0 / 0 / 0 / 0 |  |
| 665 | Disposal Order | Spell | 28009 / - / - / 27681 | 27494 / - / - / 27172 | 27134 / - / - / 26798 | 0 / - / - / 0 |  |
| 666 | Gentle Gemdragon | Unit | 15625 / 15625 / - / 15625 | 13865 / 13865 / - / 13865 | 14351 / 14351 / - / 14351 | 0 / 0 / - / 0 |  |
| 667 | Imposing Challenger | Unit | 20552 / 2702 / - / 2688 | 19374 / 3679 / - / 3651 | 20707 / 6060 / - / 6007 | 0 / 0 / - / 0 |  |
| 668 | Repulse | Spell | 5872 / - / - / 5809 | 8352 / - / - / 8276 | 8105 / - / - / 8033 | 23514 / - / - / 22878 |  |
| 669 | Stare Down | Spell | 22057 / - / - / 21733 | 21950 / - / - / 21575 | 24434 / - / - / 24084 | 0 / - / - / 0 |  |
| 670 | Wily Newtfish | Unit | 22884 / - / - / - | 21068 / - / - / - | 23468 / - / - / - | 0 / - / - / - |  |
| 671 | Blood Rose | Gear | 25576 / 18459 / 8538 / 18982 | 25341 / 20544 / 11746 / 20896 | 27196 / 22687 / 16746 / 23110 | 0 / 0 / 0 / 0 |  |
| 672 | Clash of Giants | Spell | 14409 / - / - / 14186 | 13311 / - / - / 13093 | 15434 / - / - / 15186 | 0 / - / - / 0 |  |
| 673 | Determined Sentry | Unit | 25783 / - / - / - | 26427 / - / - / - | 27205 / - / - / - | 0 / - / - / - |  |
| 674 | Irresistible Faefolk | Unit | 24819 / 9444 / - / 9425 | 25625 / 13949 / - / 13908 | 28413 / 20231 / - / 20201 | 170410 / 89643 / - / 89399 |  |
| 675 | Master Yi, Tempered | Unit | 41854 / 26543 / - / 26524 | 38674 / 25111 / - / 25100 | 41283 / 27711 / - / 27701 | 71644 / 48287 / - / 48287 |  |
| 676 | Nidalee, Cat Form | Unit | 27192 / 9911 / - / 9911 | 27843 / 9504 / - / 9503 | 26707 / 10348 / - / 10348 | 58145 / 22345 / - / 22345 |  |
| 677 | Nilah, Joyful Ascetic | Unit | 24376 / 20606 / - / 20606 | 23880 / 20676 / - / 20676 | 23155 / 22022 / - / 22022 | 0 / 0 / - / 0 |  |
| 678 | Poppy, Paragon | Unit | 34145 / 7616 / - / 7616 | 35258 / 7783 / - / 7783 | 37316 / 9552 / - / 9552 | 72934 / 16585 / - / 16585 |  |
| 679 | Arachnoid Horror | Unit | 19675 / 12003 / - / 11989 | 18054 / 10980 / - / 10976 | 21097 / 13196 / - / 13192 | 0 / 0 / - / 0 |  |
| 680 | Elder Dragon | Unit | 4942 / 4942 / - / 4921 | 5262 / 5262 / - / 5240 | 3484 / 3484 / - / 3474 | 54056 / 54056 / - / 53948 |  |
| 681 | Kha'Zix, Evolving Hunter | Unit | 37659 / 22988 / - / 22971 | 34314 / 21403 / - / 21392 | 37643 / 24125 / - / 24107 | 0 / 0 / - / 0 |  |
| 682 | Rengar, Trophy Hunter | Unit | 37656 / - / - / - | 39268 / - / - / - | 39250 / - / - / - | 219053 / - / - / - |  |
| 683 | Bewitching Spirit | Unit | 22746 / 22746 / - / 22746 | 22305 / 22305 / - / 22305 | 25189 / 25189 / - / 25189 | 67549 / 67549 / - / 67548 |  |
| 684 | Crescent Guardian | Unit | 21998 / 21998 / - / 21998 | 20303 / 20303 / - / 20303 | 23037 / 23037 / - / 23037 | 0 / 0 / - / 0 |  |
| 685 | Evershade Stalker | Unit | 23332 / 23332 / - / 23332 | 22524 / 22524 / - / 22524 | 23774 / 23774 / - / 23774 | 0 / 0 / - / 0 |  |
| 686 | Isolate | Spell | 19736 / - / - / 19267 | 17221 / - / - / 16772 | 21285 / - / - / 20793 | 0 / - / - / 0 |  |
| 687 | Lunar Boon | Spell | 27264 / - / - / 26812 | 28228 / - / - / 27780 | 27792 / - / - / 27361 | 0 / - / - / 0 |  |
| 688 | Megatusk | Unit | 18127 / - / 808 / 808 | 17409 / - / 560 / 560 | 16737 / - / 1076 / 1076 | 0 / - / 0 / 0 |  |
| 689 | Mister Root | Unit | 25178 / 20073 / - / 20073 | 23199 / 19039 / - / 19039 | 25349 / 23326 / - / 23326 | 0 / 0 / - / 0 |  |
| 690 | Star-Crossed | Spell | 26485 / - / - / 26018 | 28524 / - / - / 27885 | 25142 / - / - / 24613 | 250671 / - / - / 240295 |  |
| 691 | Vicious Snapjaws | Unit | 19861 / 12137 / - / 12131 | 18617 / 10125 / - / 10122 | 20564 / 11864 / - / 11860 | 0 / 0 / - / 0 |  |
| 692 | Walking Roost | Unit | 19965 / 19965 / - / 19965 | 19200 / 19200 / - / 19200 | 20198 / 20198 / - / 20198 | 0 / 0 / - / 0 |  |
| 693 | Abandon | Spell | 17877 / - / - / 17738 | 21588 / - / - / 21341 | 19127 / - / - / 19026 | 16335 / - / - / 14538 |  |
| 694 | Angler Beast | Unit | 21209 / 21209 / - / 21209 | 18682 / 18682 / - / 18682 | 20162 / 20162 / - / 20162 | 0 / 0 / - / 0 |  |
| 695 | Blast Cone | Gear | 19876 / 9984 / - / 9950 | 17690 / 13218 / - / 13165 | 20744 / 18975 / - / 18923 | 0 / 0 / - / 0 |  |
| 696 | Existential Dread | Spell | 9416 / - / - / 9158 | 10831 / - / - / 10543 | 11585 / - / - / 11286 | 10549 / - / - / 9871 |  |
| 697 | Insightful Investigator | Unit | 23911 / 23911 / - / 23911 | 22957 / 22957 / - / 22957 | 24559 / 24559 / - / 24559 | 0 / 0 / - / 0 |  |
| 698 | Scryer's Bloom | Gear | 26176 / - / 18881 / 18881 | 24054 / - / 16901 / 16901 | 25985 / - / 21753 / 21753 | 0 / - / 0 / 0 |  |
| 699 | Sinister Poro | Unit | 25163 / 3835 / - / 3804 | 24009 / 5277 / - / 5229 | 27026 / 9125 / - / 9072 | 0 / 0 / - / 0 |  |
| 700 | The List | Gear | 26180 / - / 2167 / 2165 | 23299 / - / 1860 / 1856 | 27656 / - / 3123 / 3120 | 32594 / - / 2929 / 2924 |  |
| 701 | Bone Skewer | Spell | 24672 / 12406 / - / 24338 | 24070 / 18807 / - / 23731 | 24617 / 22453 / - / 24303 | 0 / 0 / - / 0 |  |
| 702 | Conscription | Spell | 7646 / - / - / 7481 | 6602 / - / - / 6426 | 8351 / - / - / 8146 | 0 / - / - / 0 |  |
| 703 | Evelynn, Entrancing | Unit | 24714 / 793 / - / 790 | 24992 / 1363 / - / 1350 | 26241 / 543 / - / 542 | 46312 / 1426 / - / 1418 |  |
| 704 | Heedless Resurrection | Spell | 28573 / - / - / 28166 | 27096 / - / - / 26620 | 26315 / - / - / 25948 | 0 / - / - / 0 |  |
| 705 | Kha'Zix, Mutating Horror | Unit | 44751 / 21720 / - / 21720 | 45553 / 18973 / - / 18973 | 45645 / 22395 / - / 22395 | 69425 / 33290 / - / 33290 |  |
| 706 | Maduli the Gatekeeper | Unit | 16888 / - / 4666 / 4666 | 15804 / - / 4170 / 4170 | 15042 / - / 6186 / 6186 | 0 / - / 0 / 0 |  |
| 707 | Pyke, Returned | Unit | 42041 / 19798 / - / 19793 | 41637 / 18494 / - / 18488 | 43600 / 22491 / - / 22489 | 49147 / 23193 / - / 23193 |  |
| 708 | Syndra, Transcendent | Unit | 19805 / - / - / - | 16025 / - / - / - | 16790 / - / - / - | 0 / - / - / - |  |
| 709 | Baron Nashor | Unit | 8963 / - / - / - | 8163 / - / - / - | 6021 / - / - / - | 11830 / - / - / - |  |
| 710 | Cursed Sarcophagus | Gear | 19530 / 19530 / 9672 / 19530 | 17975 / 17975 / 9258 / 17975 | 19751 / 19751 / 13844 / 19751 | 0 / 0 / 0 / 0 |  |
| 711 | Diana, No Longer Human | Unit | 45336 / 25441 / - / 25441 | 44501 / 25825 / - / 25825 | 44411 / 24155 / - / 24155 | 0 / 0 / - / 0 |  |
| 712 | Vex, Apathetic | Unit | 33733 / 18710 / - / 18710 | 31163 / 16425 / - / 16425 | 33914 / 19687 / - / 19687 | 163418 / 88440 / - / 88439 |  |
| 713 | Bandle Soldier | Unit | 21956 / - / - / - | 21471 / - / - / - | 24404 / - / - / - | 0 / - / - / - |  |
| 714 | Black Rose Dignitary | Unit | 24208 / 15264 / - / 15263 | 24558 / 14048 / - / 14048 | 25656 / 17122 / - / 17120 | 17408 / 13022 / - / 13021 |  |
| 715 | Carrion Dredger | Unit | 24472 / 16598 / - / 16596 | 25631 / 16211 / - / 16209 | 26728 / 19418 / - / 19417 | 86039 / 63948 / - / 63948 |  |
| 716 | Crimson Pigeons | Unit | 23921 / - / - / - | 23438 / - / - / - | 26858 / - / - / - | 28974 / - / - / - |  |
| 717 | Heroic Charge | Spell | 14096 / - / - / 13801 | 16733 / - / - / 16304 | 17251 / - / - / 16840 | 0 / - / - / 0 |  |
| 718 | Loyal Poro | Unit | 24252 / 14662 / - / 14661 | 24961 / 13748 / - / 13744 | 25517 / 16633 / - / 16632 | 28891 / 11995 / - / 11995 |  |
| 719 | Scrutinizing Sergeant | Unit | 19691 / 19691 / - / 19691 | 18802 / 18802 / - / 18802 | 20089 / 20089 / - / 20089 | 0 / 0 / - / 0 |  |
| 720 | Shepherd's Heirloom | Gear | 24165 / 24165 / - / 24165 | 24061 / 24061 / - / 24061 | 27124 / 27124 / - / 27124 | 64346 / 64346 / - / 64346 |  |
| 721 | Soul Harvest | Spell | 19615 / - / - / 19313 | 19321 / - / - / 18956 | 22807 / - / - / 22438 | 0 / - / - / 0 |  |
| 722 | Ultrasoft Poro | Unit | 22954 / - / 4207 / 4207 | 20893 / - / 3044 / 3044 | 22244 / - / 4242 / 4242 | 0 / - / 0 / 0 |  |
| 723 | Divining Shells | Gear | 23593 / - / 22015 / 21988 | 22752 / - / 21710 / 21665 | 27123 / - / 26899 / 26860 | 27050 / - / 26389 / 26345 |  |
| 724 | Enthralling Protector | Unit | 25121 / 14894 / 7649 / 15345 | 26344 / 16545 / 8756 / 16994 | 27482 / 17734 / 11616 / 18416 | 26433 / 16796 / 15993 / 20120 |  |
| 725 | Mageseeker Investigator | Unit | 22562 / - / - / - | 22000 / - / - / - | 23381 / - / - / - | 0 / - / - / - |  |
| 726 | Safety Inspector | Unit | 22135 / 22135 / - / 22135 | 18934 / 18934 / - / 18934 | 22446 / 22446 / - / 22446 | 0 / 0 / - / 0 |  |
| 727 | Shadow's Call | Spell | 22403 / - / - / 22076 | 23096 / - / - / 22755 | 25082 / - / - / 24790 | 21708 / - / - / 21124 |  |
| 728 | Stalking Wolf | Unit | 12702 / - / - / - | 13424 / - / - / - | 12492 / - / - / - | 0 / - / - / - |  |
| 729 | Starhound | Unit | 20743 / 5049 / - / 5049 | 19456 / 4600 / - / 4600 | 21619 / 6248 / - / 6248 | 0 / 0 / - / 0 |  |
| 730 | Undying Loyalty | Spell | 11120 / - / - / 10991 | 9448 / - / - / 9334 | 13215 / - / - / 13070 | 0 / - / - / 0 |  |
| 731 | Ashe, Focused | Unit | 20231 / 20231 / - / 20231 | 19793 / 19793 / - / 19793 | 20638 / 20638 / - / 20638 | 0 / 0 / - / 0 |  |
| 732 | Atakhan | Unit | 20263 / 7663 / - / 7663 | 18623 / 5960 / - / 5960 | 18327 / 7323 / - / 7323 | 0 / 0 / - / 0 |  |
| 733 | Galio, Indefatigable | Unit | 25445 / - / - / - | 22903 / - / - / - | 25528 / - / - / - | 0 / - / - / - |  |
| 734 | LeBlanc, Fragmented | Unit | 42682 / 29206 / - / 29206 | 40617 / 27104 / - / 27103 | 42556 / 32078 / - / 32075 | 43569 / 38826 / - / 38826 |  |
| 735 | Sacrifice | Spell | 25513 / - / - / 25212 | 26795 / - / - / 26369 | 25919 / - / - / 25569 | 80663 / - / - / 77887 |  |
| 736 | Shard of Undoing | Gear | 16333 / 1361 / - / 1361 | 15943 / 1649 / - / 1649 | 17063 / 1584 / - / 1584 | 0 / 0 / - / 0 |  |
| 737 | Tactical Retreat | Spell | 28650 / - / - / 28279 | 30355 / - / - / 29847 | 29835 / - / - / 29447 | 15591 / - / - / 15135 |  |
| 738 | Vi, Peacekeeper | Unit | 37107 / 14320 / - / 14250 | 40302 / 13892 / - / 13758 | 37718 / 16994 / - / 16899 | 190109 / 67792 / - / 67155 |  |
| 739 | Ivern, Friend to All | Unit | 34232 / 20063 / - / 20037 | 32090 / 18757 / - / 18745 | 35237 / 21199 / - / 21189 | 0 / 0 / - / 0 |  |
| 740 | Poppy, Defender of the Meek | Unit | 42846 / - / - / - | 44362 / - / - / - | 42187 / - / - / - | 0 / - / - / - |  |
| 741 | Rift Herald | Unit | 15635 / 10305 / - / 10305 | 14624 / 9357 / - / 9357 | 13389 / 9302 / - / 9302 | 36733 / 31100 / - / 31100 |  |
| 742 | The Ruination | Spell | 9854 / - / - / 9741 | 9235 / - / - / 9116 | 8169 / - / - / 8089 | 8090 / - / - / 8051 |  |
| 743 | Curtain Call | Spell | 12984 / - / - / 12879 | 11583 / - / - / 11485 | 12933 / - / - / 12873 | 0 / - / - / 0 |  |
| 744 | Pridestalker | Legend | 40440 / 40436 / - / 40436 | 40000 / 39991 / - / 39991 | 40800 / 40800 / - / 40800 | 46200 / 46200 / - / 46200 |  |
| 745 | Thrill of the Hunt | Spell | 16025 / - / - / 15936 | 16259 / - / - / 16123 | 16333 / - / - / 16269 | 25832 / - / - / 25116 |  |
| 746 | Bloodharbor Ripper | Legend | 39120 / - / 34181 / 34181 | 40380 / - / 33582 / 33582 | 40360 / - / 38557 / 38557 | 46200 / - / 38462 / 38462 |  |
| 747 | Death from Below | Spell | 10375 / 1810 / - / 10191 | 10070 / 2664 / - / 9838 | 11461 / 3720 / - / 11200 | 0 / 0 / - / 0 |  |
| 748 | Hextech Gauntlets | Gear | 12636 / 2215 / - / 11406 | 11611 / 1947 / - / 10236 | 13605 / 3871 / - / 13126 | 30742 / 7022 / - / 28617 |  |
| 749 | Bashful Bloom | Legend | 39560 / - / 31463 / 31463 | 42280 / - / 30796 / 30796 | 40420 / - / 34390 / 34390 | 46200 / - / 42945 / 42945 |  |
| 750 | Lilting Lullaby | Spell | 10708 / - / - / 10655 | 13843 / - / - / 13773 | 11695 / - / - / 11668 | 17105 / - / - / 17074 |  |
| 751 | Alpha Strike | Spell | 16140 / 9145 / - / 15731 | 15814 / 8503 / - / 15246 | 16533 / 9135 / - / 16132 | 0 / 0 / - / 0 |  |
| 752 | Shadow | Unit | 16280 / - / 923 / 913 | 14833 / - / 1395 / 1378 | 16491 / - / 217 / 214 | 0 / - / 0 / 0 |  |
| 753 | Green Father | Legend | 40120 / 36058 / - / 36055 | 40880 / 38690 / - / 38690 | 40260 / 39632 / - / 39632 | 91200 / 89070 / - / 89070 |  |
| 754 | Daisy! | Unit | 9076 / 2 / - / 2 | 8567 / 2 / - / 2 | 8688 / 4 / - / 4 | 37782 / 3223 / - / 3178 | random:resolved, random:triggered, rp0:resolved, rp0:triggered, rp:resolved, rp:triggered |
| 755 | Moonfall | Spell | 14078 / - / - / 13845 | 14521 / - / - / 14274 | 15475 / - / - / 15231 | 32205 / - / - / 30341 |  |
| 756 | Deceiver | Legend | 38760 / 34915 / - / 34914 | 39940 / 37809 / - / 37809 | 39920 / 39357 / - / 39357 | 46200 / 42855 / - / 42855 |  |
| 757 | Mirror Image | Spell | 15136 / 15024 / - / 15050 | 16126 / 15980 / - / 16013 | 14971 / 14898 / - / 14917 | 38456 / 37968 / - / 38326 |  |
| 758 | Void Assault | Spell | 15326 / - / - / 15148 | 15874 / - / - / 15624 | 15251 / - / - / 15046 | 30539 / - / - / 29936 |  |
| 759 | Keeper of the Hammer | Legend | 36520 / 32832 / 21285 / 33046 | 40540 / 36924 / 23252 / 37196 | 40380 / 37513 / 28421 / 37883 | 46200 / 43067 / 36242 / 43834 |  |
| 760 | Keeper's Verdict | Spell | 12930 / - / - / 12799 | 14509 / - / - / 14330 | 15398 / - / - / 15239 | 23562 / - / - / 23288 |  |
| 761 | Abandoned Hall | Battlefield | 32868 / 18070 / - / 18070 | 33857 / 23242 / - / 23242 | 33496 / 26507 / - / 26507 | 45388 / 39917 / - / 39917 |  |
| 762 | Altar of Blood | Battlefield | 32572 / - / - / - | 33046 / - / - / - | 33451 / - / - / - | 0 / - / - / - |  |
| 763 | Amateur Recital | Battlefield | 32848 / 26606 / - / 26570 | 32895 / 30689 / - / 30669 | 32434 / 32048 / - / 32042 | 30294 / 27089 / - / 27025 |  |
| 764 | Black Flame Altar | Battlefield | 32622 / - / - / - | 33625 / - / - / - | 33838 / - / - / - | 15203 / - / - / - |  |
| 765 | Dusk Rose Lab | Battlefield | 33213 / 28922 / - / 28921 | 31669 / 30692 / - / 30691 | 32232 / 32354 / - / 32354 | 60159 / 57339 / - / 57339 |  |
| 766 | Forbidding Waste | Battlefield | 32578 / - / - / - | 32897 / - / - / - | 32700 / - / - / - | 93999 / - / - / - |  |
| 767 | Forgotten Library | Battlefield | 34563 / 9016 / - / 9016 | 33514 / 8196 / - / 8196 | 32238 / 7741 / - / 7741 | 0 / 0 / - / 0 |  |
| 768 | Frozen Fortress | Battlefield | 33204 / 33599 / - / 33599 | 34584 / 35052 / - / 35052 | 33451 / 33976 / - / 33976 | 74889 / 76405 / - / 76405 |  |
| 769 | Gardens of Becoming | Battlefield | 32880 / - / 22957 / 22957 | 33160 / - / 20360 / 20360 | 32412 / - / 24545 / 24545 | 15234 / - / 10234 / 10234 |  |
| 770 | Ripper's Bay | Battlefield | 33027 / 1931 / - / 1931 | 33212 / 2943 / - / 2943 | 33663 / 4118 / - / 4118 | 15196 / 8712 / - / 8712 |  |
| 771 | Star Spring | Battlefield | 34072 / 19931 / - / 19931 | 32599 / 23188 / - / 23188 | 33451 / 27849 / - / 27849 | 74598 / 57057 / - / 57056 |  |
| 772 | The Academy | Battlefield | 33185 / 32590 / - / 32584 | 31735 / 30832 / - / 30819 | 34099 / 33729 / - / 33722 | 15351 / 14975 / - / 14974 |  |
| 773 | Trapping Grounds | Battlefield | 33429 / 8783 / - / 8782 | 32986 / 8182 / - / 8180 | 33990 / 12315 / - / 12311 | 0 / 0 / - / 0 |  |
| 774 | Valley of Idols | Battlefield | 33208 / 19580 / - / 19580 | 32940 / 22899 / - / 22899 | 33174 / 25984 / - / 25984 | 0 / 0 / - / 0 |  |
| 775 | Vaults of Helia | Battlefield | 34046 / 33371 / - / 33366 | 32962 / 32081 / - / 32075 | 32927 / 32599 / - / 32592 | 0 / 0 / - / 0 |  |
| 776 | Pouty Poro | Unit | 24597 / - / - / - | 25037 / - / - / - | 25030 / - / - / - | 0 / - / - / - |  |
| 777 | Lonely Poro | Unit | 24393 / 15229 / - / 15228 | 22849 / 13008 / - / 13008 | 26783 / 18411 / - / 18408 | 0 / 0 / - / 0 |  |
| 778 | Plundering Poro | Unit | 25516 / 10627 / - / 10626 | 26018 / 12138 / - / 12138 | 26007 / 13988 / - / 13986 | 0 / 0 / - / 0 |  |
| 779 | Veteran Poro | Unit | 26683 / 24711 / - / 24711 | 25949 / 25283 / - / 25283 | 26185 / 26185 / - / 26185 | 0 / 0 / - / 0 |  |
| 780 | Mystic Poro | Unit | 24549 / - / - / - | 23983 / - / - / - | 26128 / - / - / - | 0 / - / - / - |  |
| 781 | Daring Poro | Unit | 25566 / - / - / - | 25852 / - / - / - | 29453 / - / - / - | 0 / - / - / - |  |
| 782 | Virtuoso | Legend | 41900 / 17904 / - / 17904 | 40400 / 23096 / - / 23096 | 38780 / 26468 / - / 26468 | 0 / 0 / - / 0 |  |
| 783 | Piltover Enforcer | Legend | 40040 / 6683 / - / 6671 | 39500 / 9504 / - / 9483 | 39400 / 17085 / - / 17064 | 46200 / 15424 / - / 15400 |  |
| 784 | Wuju Master | Legend | 41640 / - / - / - | 39340 / - / - / - | 40960 / - / - / - | 0 / - / - / - |  |
| 785 | Gloomist | Legend | 41800 / 32784 / - / 32773 | 40980 / 36281 / - / 36279 | 40480 / 37309 / - / 37309 | 46200 / 39472 / - / 39472 |  |
| 786 | Scorn of the Moon | Legend | 41420 / - / 41420 / 41420 | 38440 / - / 38440 / 38440 | 38120 / - / 38120 / 38120 | 46200 / - / 46200 / 46200 |  |
| 787 | Voidreaver | Legend | 40580 / 31381 / 31651 / 34631 | 41520 / 29421 / 30114 / 33711 | 39500 / 32192 / 35274 / 35661 | 46200 / 35613 / 33326 / 36803 |  |
| 788 | Baccai Sandspinner | Unit | 18318 / - / 6094 / 6094 | 16149 / - / 4585 / 4585 | 17262 / - / 6135 / 6135 | 0 / - / 0 / 0 |  |
| 789 | Blade Twirler | Unit | 21063 / 15492 / - / 15492 | 19687 / 14073 / - / 14073 | 21195 / 16531 / - / 16531 | 0 / 0 / - / 0 |  |
| 790 | Brittle Steel | Spell | 20477 / - / - / 20323 | 18024 / - / - / 17872 | 23113 / - / - / 23004 | 0 / - / - / 0 |  |
| 791 | Dune Surfer | Unit | 22705 / - / - / - | 20929 / - / - / - | 23315 / - / - / - | 0 / - / - / - |  |
| 792 | Forsaken Baccai | Unit | 24930 / 21688 / - / 21688 | 23425 / 20843 / - / 20843 | 26033 / 22482 / - / 22482 | 0 / 0 / - / 0 |  |
| 793 | Oasis Raider | Unit | 22748 / 19770 / - / 19770 | 19444 / 17065 / - / 17065 | 21992 / 18513 / - / 18513 | 0 / 0 / - / 0 |  |
| 794 | Punching Poro | Unit | 24113 / - / 18335 / 18335 | 24296 / - / 19788 / 19788 | 25352 / - / 23995 / 23995 | 0 / - / 0 / 0 |  |
| 795 | Ruthless Strike | Spell | 24253 / - / - / 24001 | 24777 / - / - / 24423 | 23882 / - / - / 23675 | 0 / - / - / 0 |  |
| 796 | Baccai Reaper | Unit | 22541 / 3949 / - / 3949 | 20018 / 4890 / - / 4890 | 23631 / 9063 / - / 9063 | 0 / 0 / - / 0 |  |
| 797 | Consuming Curse | Spell | 25428 / - / - / 25136 | 27173 / - / - / 26819 | 24345 / - / - / 24106 | 0 / - / - / 0 |  |
| 798 | Pendulum Blade | Gear | 21203 / 13815 / - / 18783 | 19686 / 12594 / - / 16970 | 22296 / 17292 / - / 21040 | 0 / 0 / - / 0 |  |
| 799 | Perfect Execution | Spell | 21842 / - / - / 21708 | 19344 / - / - / 19186 | 22978 / - / - / 22883 | 57664 / - / - / 56867 |  |
| 800 | Shadow Assassin | Unit | 20335 / - / - / - | 17920 / - / - / - | 19709 / - / - / - | 0 / - / - / - |  |
| 801 | Shadow Fiend | Unit | 24032 / - / 12696 / 12696 | 24060 / - / 12182 / 12182 | 25365 / - / 15768 / 15768 | 18700 / - / 9445 / 9445 |  |
| 802 | Decree of Rage | Spell | 7219 / - / - / 7219 | 7657 / - / - / 7657 | 7618 / - / - / 7618 | 0 / - / - / 0 |  |
| 803 | Eclipse Dragon | Unit | 14837 / 87 / - / 87 | 12935 / 86 / - / 86 | 12177 / 133 / - / 133 | 0 / 0 / - / 0 | random:resolved, random:triggered, rp:resolved, rp:triggered |
| 804 | Morgana, Vindictive | Unit | 22958 / 22958 / - / 22926 | 24737 / 24737 / - / 24694 | 22828 / 22828 / - / 22793 | 0 / 0 / - / 0 |  |
| 805 | Rage Amplifier | Gear | 18739 / - / 7516 / 7516 | 18789 / - / 6914 / 6914 | 18691 / - / 8303 / 8303 | 11138 / - / 3768 / 3768 |  |
| 806 | Renekton, Rage Fueled | Unit | 20535 / 429 / - / 429 | 16379 / 193 / - / 193 | 17504 / 144 / - / 144 | 0 / 0 / - / 0 | rp:resolved, rp:triggered |
| 807 | Twilight Reveler | Unit | 23785 / 7132 / - / 7132 | 20528 / 5539 / - / 5539 | 23872 / 8302 / - / 8302 | 0 / 0 / - / 0 |  |
| 808 | Akali, Deadly Weapon | Unit | 41247 / 22495 / 22486 / 30190 | 39015 / 27174 / 20217 / 31009 | 43217 / 36371 / 27546 / 38953 | 41519 / 32217 / 22310 / 34845 |  |
| 809 | Endless Riches | Gear | 16370 / 16370 / - / 16370 | 14856 / 14856 / - / 14856 | 17087 / 17087 / - / 17087 | 0 / 0 / - / 0 |  |
| 810 | Zed, From the Shadows | Unit | 39028 / 20368 / - / 20368 | 34434 / 27683 / - / 27683 | 38518 / 36789 / - / 36789 | 0 / 0 / - / 0 |  |
| 811 | Affectionate Poro | Unit | 23542 / 4213 / - / 4213 | 23308 / 3789 / - / 3789 | 25360 / 5032 / - / 5032 | 26744 / 6308 / - / 6308 |  |
| 812 | Esteemed Hierophant | Unit | 21686 / - / - / - | 18902 / - / - / - | 21845 / - / - / - | 0 / - / - / - |  |
| 813 | Field Musicians | Unit | 23142 / 23142 / - / 23116 | 21049 / 21049 / - / 21014 | 24160 / 24160 / - / 24131 | 0 / 0 / - / 0 |  |
| 814 | Hand Hammer | Gear | 23193 / - / - / - | 23829 / - / - / - | 25700 / - / - / - | 27753 / - / - / - |  |
| 815 | Mournful Witness | Unit | 26200 / 6379 / - / 6379 | 24754 / 5386 / - / 5386 | 26674 / 7024 / - / 7024 | 33891 / 15359 / - / 15359 |  |
| 816 | Ol' Poro | Unit | 24441 / - / - / - | 23232 / - / - / - | 27141 / - / - / - | 0 / - / - / - |  |
| 817 | Serene Ascetic | Unit | 25178 / - / 11608 / 11608 | 23277 / - / 9601 / 9601 | 25580 / - / 13495 / 13495 | 0 / - / 0 / 0 |  |
| 818 | Twilight Shroud | Spell | 24461 / - / - / 24248 | 24076 / - / - / 23821 | 26622 / - / - / 26476 | 0 / - / - / 0 |  |
| 819 | Frostcoat Mother | Unit | 24057 / - / 10466 / 10466 | 23420 / - / 9583 / 9583 | 24844 / - / 10684 / 10684 | 29032 / - / 17371 / 17371 |  |
| 820 | Pakaa Protector | Unit | 20338 / 14441 / - / 14441 | 18594 / 12820 / - / 12820 | 22003 / 16754 / - / 16754 | 0 / 0 / - / 0 |  |
| 821 | Resonating Strike | Spell | 21254 / - / - / 20807 | 22551 / - / - / 22056 | 21051 / - / - / 20612 | 0 / - / - / 0 |  |
| 822 | Sanction | Spell | 26295 / - / - / 25763 | 26866 / - / - / 26118 | 26739 / - / - / 26087 | 0 / - / - / 0 |  |
| 823 | Sandstone Chimera | Unit | 16468 / - / - / - | 15281 / - / - / - | 16317 / - / - / - | 0 / - / - / - |  |
| 824 | Tomb-Raider Barbara | Unit | 23149 / 10481 / - / 10454 | 20716 / 8974 / - / 8946 | 23460 / 9644 / - / 9615 | 0 / 0 / - / 0 |  |
| 825 | Akali, Silent | Unit | 37386 / 28413 / - / 28413 | 35887 / 26585 / - / 26585 | 41023 / 33435 / - / 33435 | 51967 / 40646 / - / 40645 |  |
| 826 | Crumbling Sands | Spell | 19207 / - / - / 19059 | 23776 / - / - / 23514 | 21531 / - / - / 21389 | 0 / - / - / 0 |  |
| 827 | Decree of Focus | Spell | 5311 / - / - / 5205 | 6151 / - / - / 6016 | 6463 / - / - / 6337 | 0 / - / - / 0 |  |
| 828 | Riven, Shattered | Unit | 22305 / 20974 / - / 20970 | 21905 / 21378 / - / 21376 | 24536 / 24536 / - / 24536 | 29790 / 27405 / - / 27394 |  |
| 829 | Shen, Scourge of Shadows | Unit | 32373 / 7801 / - / 7783 | 28317 / 6490 / - / 6477 | 32320 / 6758 / - / 6749 | 0 / 0 / - / 0 |  |
| 830 | Steel Paws | Unit | 26079 / - / 5915 / 5915 | 26619 / - / 5337 / 5337 | 28032 / - / 5792 / 5792 | 115713 / - / 27934 / 27934 |  |
| 831 | Astral Heron | Unit | 17445 / 10365 / - / 10365 | 16336 / 9264 / - / 9264 | 16512 / 9816 / - / 9816 | 76444 / 45737 / - / 45737 |  |
| 832 | Helm of Suppression | Gear | 18658 / - / 9616 / 9616 | 19692 / - / 9105 / 9105 | 21245 / - / 12171 / 12171 | 0 / - / 0 / 0 |  |
| 833 | Nasus, Ascended | Unit | 27799 / 1786 / 5663 / 5674 | 24398 / 1524 / 4096 / 4104 | 25468 / 2329 / 5749 / 5758 | 8144 / 398 / 859 / 860 |  |
| 834 | Apprentice Mage | Unit | 23869 / 12991 / 13001 / 13019 | 23118 / 11682 / 11679 / 11704 | 25585 / 16755 / 16739 / 16770 | 0 / 0 / 0 / 0 |  |
| 835 | Cloud Drake | Unit | 18938 / 18938 / - / 18938 | 17573 / 17573 / - / 17573 | 18464 / 18464 / - / 18464 | 0 / 0 / - / 0 |  |
| 836 | Dredge Up | Spell | 24143 / - / - / 23992 | 23908 / - / - / 23767 | 26548 / - / - / 26451 | 71215 / - / - / 70765 |  |
| 837 | Grumpy Rockbear | Unit | 22224 / - / 10359 / 10359 | 20153 / - / 8871 / 8871 | 22642 / - / 10831 / 10831 | 0 / - / 0 / 0 |  |
| 838 | Iterative Design | Spell | 21045 / - / - / 20867 | 19023 / - / - / 18841 | 22720 / - / - / 22546 | 0 / - / - / 0 |  |
| 839 | Mesmerize | Spell | 27207 / - / - / 26827 | 29225 / - / - / 28712 | 27095 / - / - / 26712 | 35228 / - / - / 31342 |  |
| 840 | Otterpus | Unit | 25049 / - / - / - | 23723 / - / - / - | 27230 / - / - / - | 0 / - / - / - |  |
| 841 | Questionable Tome | Gear | 21434 / - / 18427 / 18427 | 21298 / - / 18636 / 18636 | 23934 / - / 23720 / 23720 | 0 / - / 0 / 0 |  |
| 842 | Applied Researchers | Unit | 22708 / - / 10286 / 10286 | 20861 / - / 8377 / 8377 | 21997 / - / 11654 / 11654 | 33159 / - / 15880 / 15879 |  |
| 843 | Clairvoyance | Spell | 23734 / - / - / 23482 | 25982 / - / - / 25653 | 21343 / - / - / 21149 | 34777 / - / - / 34697 |  |
| 844 | Covert Informant | Unit | 25447 / 8405 / 11632 / 11637 | 22243 / 6503 / 9100 / 9109 | 23217 / 9280 / 12070 / 12076 | 0 / 0 / 0 / 0 |  |
| 845 | Patched Porobot | Unit | 25400 / 1523 / - / 1523 | 23734 / 1379 / - / 1379 | 24739 / 1843 / - / 1843 | 58062 / 10906 / - / 10906 |  |
| 846 | Shock Blast | Spell | 24991 / - / - / 24547 | 25855 / - / - / 25295 | 26457 / - / - / 25921 | 30962 / - / - / 30010 |  |
| 847 | Sky Crusier | Unit | 22400 / - / 2657 / 2650 | 20942 / - / 2075 / 2071 | 21571 / - / 1935 / 1929 | 0 / - / 0 / 0 |  |
| 848 | Decree of Insight | Spell | 8981 / - / - / 8779 | 9087 / - / - / 8905 | 8525 / - / - / 8347 | 0 / - / - / 0 |  |
| 849 | Hextech Formula | Gear | 24240 / - / 17479 / 17476 | 23220 / - / 16264 / 16257 | 24703 / - / 20008 / 20002 | 0 / - / 0 / 0 |  |
| 850 | Nasus, Guardian of Knowledge | Unit | 35945 / 13692 / - / 13690 | 32860 / 10457 / - / 10455 | 35327 / 13550 / - / 13548 | 35408 / 15489 / - / 15489 |  |
| 851 | Plaza Guardian | Unit | 11940 / - / - / - | 11033 / - / - / - | 10001 / - / - / - | 0 / - / - / - |  |
| 852 | Swain, Visionary | Unit | 18604 / 61 / - / 61 | 16211 / 63 / - / 63 | 17343 / 33 / - / 33 | 0 / 0 / - / 0 | random:resolved, random:triggered, rp0:resolved, rp0:triggered, rp:resolved, rp:triggered |
| 853 | Temporal Breach | Spell | 24406 / - / - / 24051 | 24954 / - / - / 24544 | 26472 / - / - / 26095 | 125478 / - / - / 120830 |  |
| 854 | Bottled Constellation | Gear | 7790 / 2729 / - / 2729 | 7072 / 3559 / - / 3559 | 5986 / 2599 / - / 2599 | 0 / 0 / - / 0 |  |
| 855 | Jayce, Brilliant Inventor | Unit | 26843 / 16237 / - / 16231 | 24429 / 21044 / - / 21032 | 26462 / 26462 / - / 26443 | 33121 / 28823 / - / 28811 |  |
| 856 | Mel, Newly Awakened | Unit | 38140 / 38140 / 15603 / 38140 | 36949 / 36949 / 13272 / 36949 | 40158 / 40158 / 18022 / 40158 | 80812 / 80812 / 29110 / 80812 |  |
| 857 | Brutal Hunter | Unit | 25153 / - / 11563 / 11563 | 23436 / - / 9926 / 9926 | 25439 / - / 13446 / 13446 | 0 / - / 0 / 0 |  |
| 858 | Fretful Feline | Unit | 19788 / 16850 / - / 16850 | 16979 / 14602 / - / 14602 | 19558 / 16202 / - / 16202 | 0 / 0 / - / 0 |  |
| 859 | Guttural Roar | Spell | 26185 / - / - / 25826 | 27178 / - / - / 26690 | 26324 / - / - / 25941 | 31824 / - / - / 31195 |  |
| 860 | Jagged Cutlass | Gear | 21292 / - / - / - | 21515 / - / - / - | 23753 / - / - / - | 0 / - / - / - |  |
| 861 | Legion Marauder | Unit | 26085 / - / 22215 / 22215 | 25817 / - / 22081 / 22081 | 28458 / - / 27678 / 27678 | 24753 / - / 22123 / 22123 |  |
| 862 | Platewyrm Egg | Gear | 21475 / - / 18501 / 18501 | 20722 / - / 18044 / 18044 | 22843 / - / 20108 / 20108 | 33120 / - / 30911 / 30911 |  |
| 863 | Repair Specialist | Unit | 24940 / - / - / - | 22246 / - / - / - | 25614 / - / - / - | 0 / - / - / - |  |
| 864 | Tools of Empire | Gear | 19660 / - / 19060 / 19053 | 17434 / - / 16284 / 16276 | 20320 / - / 20175 / 20172 | 11871 / - / 11485 / 11479 |  |
| 865 | Baccai Witherclaw | Unit | 22548 / 5839 / 15009 / 15016 | 20998 / 4867 / 13594 / 13610 | 21913 / 8046 / 18327 / 18354 | 0 / 0 / 0 / 0 |  |
| 866 | Dame the Despoiler | Unit | 21113 / 3797 / 6837 / 6852 | 19925 / 2768 / 5664 / 5682 | 22424 / 4604 / 8188 / 8224 | 0 / 0 / 0 / 0 |  |
| 867 | Noxian Demolitionist | Unit | 26444 / 2450 / - / 2447 | 25560 / 4257 / - / 4251 | 26590 / 6675 / - / 6671 | 0 / 0 / - / 0 |  |
| 868 | Onslaught | Spell | 19430 / - / - / 19237 | 18820 / - / - / 18580 | 22219 / - / - / 22029 | 46997 / - / - / 45578 |  |
| 869 | Profiteer | Unit | 22123 / 1499 / - / 1497 | 22047 / 2323 / - / 2319 | 23218 / 3622 / - / 3619 | 22467 / 7679 / - / 7664 |  |
| 870 | Rampage | Spell | 19099 / - / - / 18758 | 17858 / - / - / 17445 | 20245 / - / - / 19922 | 165654 / - / - / 157877 |  |
| 871 | Ambessa, The Wolf | Unit | 39087 / - / 18082 / 18082 | 38041 / - / 16005 / 16005 | 41328 / - / 21888 / 21888 | 42120 / - / 20311 / 20311 |  |
| 872 | Decree of Strength | Spell | 23781 / - / - / 23535 | 25671 / - / - / 25395 | 27520 / - / - / 27262 | 0 / - / - / 0 |  |
| 873 | Gangplank, Naval | Unit | 18657 / - / 13738 / 13738 | 17043 / - / 12721 / 12721 | 19761 / - / 18531 / 18531 | 0 / - / 0 / 0 |  |
| 874 | Hextech Disc | Gear | 19782 / - / 16814 / 16814 | 18709 / - / 16318 / 16318 | 19648 / - / 19457 / 19457 | 0 / - / 0 / 0 |  |
| 875 | Jayce, Hammer in Hand | Unit | 32436 / 28881 / - / 28881 | 30175 / 27063 / - / 27063 | 32774 / 28387 / - / 28387 | 0 / 0 / - / 0 |  |
| 876 | Wild Claw | Spell | 13571 / 5177 / - / 13386 | 12344 / 7884 / - / 12123 | 13735 / 13415 / - / 13555 | 0 / 0 / - / 0 |  |
| 877 | Cataclysmic Duel | Spell | 11388 / - / - / 11255 | 10622 / - / - / 10459 | 11025 / - / - / 10888 | 0 / - / - / 0 |  |
| 878 | Corrupted Dragon | Unit | 10297 / 1281 / - / 1276 | 9827 / 1836 / - / 1822 | 8123 / 1999 / - / 1983 | 0 / 0 / - / 0 |  |
| 879 | Renekton, Brute | Unit | 25930 / 696 / 16387 / 16454 | 21523 / 683 / 13690 / 13762 | 20514 / 2614 / 16794 / 16820 | 0 / 0 / 0 / 0 |  |
| 880 | Kinkou Lifeblade | Unit | 21270 / - / 11458 / 11458 | 20741 / - / 10261 / 10261 | 23168 / - / 15052 / 15052 | 0 / - / 0 / 0 |  |
| 881 | Mask Mother | Unit | 24637 / 695 / - / 695 | 21621 / 1112 / - / 1112 | 24563 / 1427 / - / 1427 | 0 / 0 / - / 0 |  |
| 882 | Shadow Order Disciple | Unit | 25703 / 13499 / - / 13499 | 24322 / 16530 / - / 16530 | 26455 / 21860 / - / 21860 | 149408 / 106266 / - / 106260 |  |
| 883 | Shadowblade Lurker | Unit | 21120 / - / - / - | 19067 / - / - / - | 20255 / - / - / - | 25112 / - / - / - |  |
| 884 | Spiderling | Unit | 25984 / - / - / - | 22438 / - / - / - | 26320 / - / - / - | 0 / - / - / - |  |
| 885 | Stargazer | Unit | 20067 / - / - / - | 18305 / - / - / - | 20509 / - / - / - | 0 / - / - / - |  |
| 886 | Tornado Warrior | Unit | 24136 / 4472 / - / 4469 | 23743 / 8089 / - / 8084 | 25667 / 13292 / - / 13283 | 107849 / 42372 / - / 42334 |  |
| 887 | Up from the Deep | Spell | 24841 / - / - / 24610 | 21918 / - / - / 21655 | 26068 / - / - / 25848 | 89446 / - / - / 86732 |  |
| 888 | Gust Monk | Unit | 26259 / 11636 / - / 11621 | 23712 / 15179 / - / 15152 | 25132 / 18147 / - / 18123 | 0 / 0 / - / 0 |  |
| 889 | Ravenbloom Prefect | Unit | 24516 / 5605 / - / 5605 | 21351 / 6855 / - / 6855 | 24898 / 9448 / - / 9448 | 14042 / 3983 / - / 3983 |  |
| 890 | Shadows of the Past | Spell | 21854 / - / - / 21485 | 19349 / - / - / 19004 | 23595 / - / - / 23223 | 0 / - / - / 0 |  |
| 891 | Tail-Cloaked Matriarch | Unit | 22200 / 3442 / 11900 / 11901 | 20661 / 4537 / 10068 / 10078 | 21602 / 8864 / 13729 / 13749 | 90954 / 35442 / 50568 / 50836 |  |
| 892 | Twilight Step | Spell | 22861 / - / - / 22591 | 21097 / - / - / 20763 | 25594 / - / - / 25338 | 0 / - / - / 0 |  |
| 893 | Wind and Ghosts | Spell | 25136 / - / - / 24580 | 25546 / - / - / 25001 | 24656 / - / - / 24230 | 0 / - / - / 0 |  |
| 894 | Decree of Discord | Spell | 25188 / - / - / 24840 | 24791 / - / - / 24413 | 28067 / - / - / 27779 | 0 / - / - / 0 |  |
| 895 | Forgotten Relic | Gear | 18187 / 18187 / - / 18187 | 15714 / 15714 / - / 15714 | 17608 / 17608 / - / 17608 | 0 / 0 / - / 0 |  |
| 896 | Illaoi, Prophet of the Great Kraken | Unit | 18972 / 18972 / - / 18972 | 16755 / 16755 / - / 16755 | 17646 / 17646 / - / 17646 | 0 / 0 / - / 0 |  |
| 897 | Mel, Defiant Soul | Unit | 36150 / 6079 / 19748 / 19756 | 34556 / 5675 / 19651 / 19660 | 37174 / 8073 / 25013 / 25028 | 0 / 0 / 0 / 0 |  |
| 898 | Minah Swiftfoot | Unit | 19205 / 12367 / - / 12367 | 17149 / 10691 / - / 10691 | 17480 / 12218 / - / 12218 | 0 / 0 / - / 0 |  |
| 899 | Zed, Without a Sound | Unit | 37306 / 14863 / 12740 / 16692 | 32681 / 13583 / 12147 / 15145 | 35207 / 17146 / 15477 / 18460 | 42413 / 15927 / 19481 / 22382 |  |
| 900 | Kennen, Storm of Shuriken | Unit | 41968 / 41968 / - / 41968 | 39021 / 39021 / - / 39021 | 43170 / 43170 / - / 43170 | 70412 / 70412 / - / 70412 |  |
| 901 | Kharox | Unit | 18056 / 4501 / 4492 / 4511 | 16694 / 3645 / 3608 / 3648 | 17857 / 4409 / 4387 / 4417 | 22346 / 6764 / 6505 / 6765 |  |
| 902 | Ocean Drake | Unit | 16762 / 8880 / - / 8860 | 14415 / 11782 / - / 11745 | 13511 / 13391 / - / 13362 | 0 / 0 / - / 0 |  |
| 903 | Dragon Form | Spell | 23113 / - / - / 22936 | 21995 / - / - / 21793 | 25094 / - / - / 24948 | 22161 / - / - / 21856 |  |
| 904 | Disciple of Shen | Unit | 27176 / - / - / - | 26723 / - / - / - | 27295 / - / - / - | 0 / - / - / - |  |
| 905 | Horns of the Dragon | Unit | 18918 / - / - / - | 17016 / - / - / - | 20267 / - / - / - | 0 / - / - / - |  |
| 906 | Keeper of Law | Unit | 21708 / - / - / - | 20886 / - / - / - | 23022 / - / - / - | 28047 / - / - / - |  |
| 907 | Masa, Crashing Thunder | Unit | 23221 / 6055 / - / 6016 | 22364 / 8600 / - / 8519 | 24306 / 11741 / - / 11679 | 0 / 0 / - / 0 |  |
| 908 | Reluctant Leader | Unit | 23019 / 17729 / - / 17729 | 21271 / 16345 / - / 16345 | 23627 / 17759 / - / 17759 | 0 / 0 / - / 0 |  |
| 909 | Solari Sunhawk | Unit | 24808 / - / 13282 / 13282 | 23712 / - / 12139 / 12139 | 26627 / - / 17478 / 17478 | 52425 / - / 31420 / 31420 |  |
| 910 | Soulspinner | Unit | 26547 / - / - / - | 28491 / - / - / - | 29143 / - / - / - | 0 / - / - / - |  |
| 911 | Escaped Grayback | Unit | 23993 / - / 21892 / 21892 | 23918 / - / 20959 / 20959 | 25604 / - / 25302 / 25302 | 0 / - / 0 / 0 |  |
| 912 | Hungry Wolf | Unit | 22593 / - / 3259 / 3259 | 22746 / - / 3638 / 3638 | 24491 / - / 6390 / 6390 | 27885 / - / 878 / 878 |  |
| 913 | Ki Barrier | Spell | 27126 / - / - / 26759 | 28583 / - / - / 28119 | 28704 / - / - / 28360 | 57792 / - / - / 54132 |  |
| 914 | Lacerate | Spell | 24265 / - / - / 24085 | 23804 / - / - / 23593 | 28086 / - / - / 27916 | 0 / - / - / 0 |  |
| 915 | Noxian Emissary | Unit | 25448 / 9871 / 16421 / 16424 | 25341 / 9494 / 16607 / 16615 | 28879 / 15753 / 23423 / 23438 | 36461 / 18538 / 27037 / 27039 |  |
| 916 | Sacred Protector | Unit | 22509 / - / - / - | 20502 / - / - / - | 23230 / - / - / - | 0 / - / - / - |  |
| 917 | Aurok General | Unit | 20856 / - / 9063 / 9063 | 19921 / - / 7865 / 7865 | 22389 / - / 11481 / 11481 | 0 / - / 0 / 0 |  |
| 918 | Decree of Unity | Spell | 6803 / - / - / 6721 | 6989 / - / - / 6850 | 7734 / - / - / 7656 | 0 / - / - / 0 |  |
| 919 | Fallen Feline | Unit | 26033 / 26033 / - / 26033 | 25744 / 25744 / - / 25744 | 26637 / 26637 / - / 26637 | 28254 / 28254 / - / 28254 |  |
| 920 | Glowstone | Gear | 24364 / 23905 / 13205 / 24152 | 24194 / 23657 / 16984 / 24003 | 26934 / 25147 / 26694 / 26747 | 0 / 0 / 0 / 0 |  |
| 921 | Kayle, Justified | Unit | 24943 / - / 11182 / 11182 | 24728 / - / 10149 / 10149 | 25751 / - / 13483 / 13483 | 13606 / - / 6270 / 6270 |  |
| 922 | Kennen, Keeper of Balance | Unit | 43717 / 16432 / - / 16410 | 42261 / 21631 / - / 21594 | 45904 / 23456 / - / 23418 | 53884 / 23718 / - / 23645 |  |
| 923 | Ambessa, Respected and Feared | Unit | 35711 / 6172 / 22014 / 22016 | 34708 / 5330 / 21158 / 21161 | 38191 / 9823 / 29535 / 29535 | 12684 / 2219 / 8878 / 8878 |  |
| 924 | Shady Spectacles | Gear | 20832 / - / - / - | 19983 / - / - / - | 22192 / - / - / - | 14624 / - / - / - |  |
| 925 | Shen, Leader of the Kinkou Order | Unit | 28972 / 6518 / - / 6517 | 27150 / 5603 / - / 5600 | 27131 / 4983 / - / 4976 | 0 / 0 / - / 0 |  |
| 926 | Rogue Assassin | Legend | 38880 / - / 38124 / 38124 | 39020 / - / 38556 / 38556 | 42380 / - / 42377 / 42377 | 46200 / - / 45782 / 45782 |  |
| 927 | Shuriken Flip | Spell | 14512 / - / - / 14429 | 14344 / - / - / 14194 | 16851 / - / - / 16741 | 34255 / - / - / 32620 |  |
| 928 | Butcher of the Sands | Legend | 40780 / - / 40779 / 40779 | 40960 / - / 40960 / 40960 | 39540 / - / 39540 / 39540 | 0 / - / 0 / 0 |  |
| 929 | Dominus | Spell | 6016 / - / 911 / 5948 | 3774 / - / 721 / 3723 | 657 / - / 132 / 646 | 0 / - / 0 / 0 |  |
| 930 | Master of Shadows | Legend | 42700 / 19766 / 18505 / 19768 | 38960 / 18626 / 17968 / 18626 | 40800 / 25011 / 24992 / 25013 | 46200 / 36751 / 35559 / 36753 |  |
| 931 | Death Mark | Spell | 15885 / - / - / 15803 | 14476 / - / - / 14404 | 16454 / - / - / 16404 | 35171 / - / - / 34926 |  |
| 932 | Curator of the Sands | Legend | 40140 / 14826 / - / 14826 | 41140 / 19552 / - / 19552 | 39780 / 22528 / - / 22528 | 46200 / 38698 / - / 38698 |  |
| 933 | Siphoning Strike | Spell | 11490 / 10336 / - / 11213 | 10642 / 9425 / - / 10356 | 12322 / 10828 / - / 12024 | 25238 / 19878 / - / 23091 |  |
| 934 | Eye of Twilight | Legend | 41180 / - / 41085 / 41085 | 42720 / - / 42666 / 42666 | 42560 / - / 42557 / 42557 | 0 / - / 0 / 0 |  |
| 935 | Shadow Dash | Spell | 10578 / - / - / 10415 | 10447 / - / - / 10218 | 13871 / - / - / 13640 | 0 / - / - / 0 |  |
| 936 | Defender of Tomorrow | Legend | 39380 / - / 38736 / 38736 | 40300 / - / 39294 / 39294 | 39160 / - / 39091 / 39091 | 46200 / - / 45876 / 45876 |  |
| 937 | Acceleration Gate | Spell | 12089 / - / - / 12005 | 12667 / - / - / 12560 | 13273 / - / - / 13187 | 0 / - / - / 0 |  |
| 938 | Soul's Reflection | Legend | 38540 / 20948 / 17965 / 20949 | 39760 / 19894 / 17344 / 19894 | 40540 / 25111 / 24975 / 25113 | 46200 / 24747 / 22021 / 24765 |  |
| 939 | Rebuttal | Spell | 10707 / - / - / 10661 | 13125 / - / - / 13045 | 12522 / - / - / 12488 | 39215 / - / - / 38996 |  |
| 940 | Matriarch of War | Legend | 38040 / 23308 / 20166 / 23309 | 40300 / 23528 / 20983 / 23529 | 39780 / 29009 / 28885 / 29009 | 46200 / 41146 / 38648 / 41146 |  |
| 941 | Public Execution | Spell | 11030 / - / - / 10949 | 10791 / - / - / 10693 | 13472 / - / - / 13402 | 23883 / - / - / 23546 |  |
| 942 | Heart of the Tempest | Legend | 41720 / 40277 / 39660 / 40277 | 39780 / 37985 / 37567 / 37985 | 40800 / 40362 / 40354 / 40362 | 46200 / 45702 / 45371 / 45703 |  |
| 943 | Lightning Rush | Spell | 16325 / - / - / 16263 | 14923 / - / - / 14858 | 17788 / - / - / 17733 | 38857 / - / - / 38382 |  |
| 944 | Dragon Roost | Battlefield | 32965 / - / - / - | 33538 / - / - / - | 33963 / - / - / - | 0 / - / - / - |  |
| 945 | Heisho, Shell of the World | Battlefield | 33959 / - / - / - | 33036 / - / - / - | 31728 / - / - / - | 0 / - / - / - |  |
| 946 | Kinkou Temple | Battlefield | 32431 / - / - / - | 33169 / - / - / - | 33228 / - / - / - | 0 / - / - / - |  |
| 947 | Mystic Vortex | Battlefield | 32533 / - / - / - | 32894 / - / - / - | 33311 / - / - / - | 0 / - / - / - |  |
| 948 | Piltovan Forge | Battlefield | 33498 / - / - / - | 32866 / - / - / - | 33765 / - / - / - | 0 / - / - / - |  |
| 949 | Protective Sands | Battlefield | 31955 / 2864 / - / 2864 | 33244 / 5456 / - / 5456 | 33270 / 10842 / - / 10842 | 0 / 0 / - / 0 |  |
| 950 | Risen Altar | Battlefield | 33361 / - / - / - | 33209 / - / - / - | 31786 / - / - / - | 30274 / - / - / - |  |
| 951 | Sandswept Tomb | Battlefield | 33024 / - / - / - | 33217 / - / - / - | 34187 / - / - / - | 75127 / - / - / - |  |
| 952 | Shadow Temple | Battlefield | 33225 / 32672 / - / 32664 | 33061 / 32145 / - / 32136 | 33084 / 32723 / - / 32721 | 15268 / 14965 / - / 14964 |  |
| 953 | Threshold of the Gray | Battlefield | 33133 / 26355 / - / 26355 | 34016 / 23704 / - / 23704 | 33435 / 27705 / - / 27705 | 0 / 0 / - / 0 |  |
| 954 | Recruit | Unit | 0 / - / - / - | 0 / - / - / - | 0 / - / - / - | 0 / - / - / - | token card |
| 955 | Shadow Clone | Unit | 44374 / 8778 / - / 8778 | 48101 / 12138 / - / 12138 | 59375 / 23925 / - / 23925 | 41776 / 14034 / - / 14034 | token card |
