# Fuzz findings — whole-pool fuzz of the engine, 2026-10

Stream `test-fuzz` of the testing phase (`docs/testing-phase-2026-10.md`). Find and record only: nothing
in the engine or in a card was changed. Engine at `55df656` plus tooling (branch `test/fuzz`).

Contents: 1 what was run; 2 findings F1 to F9 and V1; 3 what did not fail; 4 agents side by side;
5 coverage; 6 coverage-guided decks; 7 throughput; 8 a stopping rule for the iteration; 9 how to
reproduce.

**A deck seed means a deck of the card pool at this commit** (`DeckFuzz.DeckOfSeedOneIsPinned` fails when
that changes). A fix to a card's behaviour does not change the decks; a new card, a ban or an unban does.

## 1. What was run

All runs: `deck_fuzz`, 3 threads, coverage listener on every game, `--known docs/testing-known-failures.txt`
(at the time holding F1 and F2 only). "random-player" is the new agent of section 4.

| Run | Build | Decks | Games | Agents | Failures | Not F1 / F2 |
|---|---|---|---|---|---|---|
| large, seed 20261012 | optimised | 50,000 by seed | 1,000,000 | random | 6,913 | 7 |
| large, seed 20261012 | optimised | same | 1,000,000 | random-player (pass=0.2) | 8,644 | 15 |
| large, seed 20261012 | optimised | same | 1,000,000 | random-player:pass=0 | 5,925 | 419 |
| side by side, seed 20261011 | optimised | 5,000 by seed | 4 x 100,000 | random; pass=0.2; pass=0; pass=0.2 against random | 707; 859; 528; 788 | 0; 1; 33; 2 |
| guided, seed 20261014 | optimised | 5,000 by seed | 2 x 100,000 | random, unguided and guided | 674; 288 | 1; 1 |
| checked, seed 20261016 | checked | 5,000 by seed | 3 x 100,000 | random; pass=0.2; pass=0 | 687; 859; 630 | 34; 54; 141 |
| checked, seed 20261015 | checked | 2,500 by seed | 3 x 20,000 | the same three | 184; 203; 144 | 2; 12; 26 |
| deck lists, seed 20261013 | optimised | the 18 lists, 324 ordered pairings x 200 | 3 x 64,800 | the same three | 0; 0; 1 | 0; 0; 1 |
| deck lists, seed 20261013 | checked | the same, all 324 x 200 | 3 x 64,800 | the same three | 0; 0; 1 | 0; 0; 1 |

- **3,960,000 games on random decks** (3,000,000 on the 50,000 decks of the large pass, the rest on
  17,500 others) and **388,800 on the deck lists**: 4.3 million in all, 554,400 of them on the checked
  build.
- Every game ended with a winner and a stated reason, or as a recorded failure. **0 games hit the turn cap
  (200), 0 ended without a winner, 0 without a reason, 0 hung, 0 ran out of memory** (with the driver's
  chain stop on; see F1 for what happens without it).
- `DeckValidator` accepted every one of the 67,500 seed decks and all 18 lists.
- The longest game that ended: 1,346 decisions, 52 turns (pass=0). I read the longest games of each large
  pass (`length.longest` in the result files): long because of many turns, longest chain 8 items, no loop.
- How games ended, large random pass: 992,500 by points, 306 The Grand Plaza, 201 Gutter Palace,
  80 burn-out point, 6,913 failures.

## 2. Findings

Numbered F1, F2 as in the interim report, then by how often they came up. "Reading" is my reading of the
likely cause from the trace and the card's code; none was verified by a fix. Counts are per 1,000,000
games of the large pass unless a line says otherwise. Every replay command is exact.

| | Finding | random | pass=0.2 | pass=0 | checked build, 3 x 100,000 |
|---|---|---|---|---|---|
| F1 | Viktor, Innovator feeds itself: the chain never empties | 6,718 | 8,377 | 5,271 | 640 / 789 / 471 |
| F2 | Alpha Strike: finalize pass ends early when it has no play-time target | 188 | 252 | 235 | 13 / 16 / 18 |
| F3 | Board state written to an object that has left the board (4 cards) | not detected | not detected | not detected | 32 / 53 / 95 |
| F4 | Emperor's Divide re-offers a unit that cannot move: 1,024 prompts | 0 | 0 | 382 | 0 / 0 / 44 |
| F5 | "You may pay [N] to ..." trigger: a Deflect paid for its target leaves the [N] unpayable (3 cards) | 3 | 6 | 15 | 0 / 0 / 1 |
| F6 | "Kill a friendly unit" additional cost, the death replaced for a price: spell left unpayable (2 cards) | 0 | 5 | 9 | 1 / 0 / 1 |
| F7 | Strike Down offered with no enemy unit to choose | 4 | 3 | 4 | 1 / 1 / 0 |
| F8 | Imposing Challenger and Irresistible Faefolk move each other without end | 0 | 0 | 9 | 0 / 0 / 0 (1 in the 20,000) |
| F9 | Star Spring: finalize pass ends early | 0 | 1 | 0 | 0 |
| V1 | `DeckValidator` does not enforce [Unique] | | | | |

Deck lists: one failure in each pass=0 run, both builds, the same game: F5 (Kennen, Keeper of Balance in
`leblanc.txt` against Irelia, Fervent's [Deflect] in `irelia.txt`).

### F1 — Viktor, Innovator (117) triggers on its own Recruit token

- **Count:** 6,718 / 8,377 / 5,271 per million (about 1 game in 150). 20,366 in the large pass, all three
  agents.
- **Cards:** Viktor, Innovator. The stop names the three cards with most items on the runaway chain;
  Viktor is among them every time, with whatever else triggers on each Recruit (Vex, Apathetic; Lillia,
  Protector of Dreams; a Recruit's [Vision]; Rally the Troops; ...).
- **Reproduction** (89 decisions; stopped after 12 chain items instead of the default 500):
  `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay 8392194388957710676 6231710079672200936 412255519 --max-chain 12 --trace --tail 30`
- **Trace:**
```
[DBG] TRIGGER: Teemo, Scout (P2) fires ability
[DBG] TRIGGER: Viktor, Innovator (P2) fires ability
[DBG] TRIGGER: Viktor, Innovator (P2) fires ability
[TRC] DECISION #84 (P2 — chain priority): 1 legal actions
[TRC] CHOSE: [P2] PassPriority
[TRC] DECISION #85 (P1 — chain priority): 1 legal actions
[TRC] CHOSE: [P1] PassPriority
[TRC] RESOLVE: Viktor, Innovator (chain_item=18, ability=1, spell=0)
[TRC] TOKEN: created Recruit (1M, id=109) for P2
[DBG] TRIGGER: Viktor, Innovator (P2) fires ability
[DBG] TRIGGER: Viktor, Innovator (P2) fires ability
[TRC] VIKTOR, INNOVATOR: off-turn play -> 1[M] Recruit in base
[TRC] DECISION #86 (P2 — chain priority): 1 legal actions
[TRC] CHOSE: [P2] PassPriority
[TRC] DECISION #87 (P1 — chain priority): 1 legal actions
[TRC] CHOSE: [P1] PassPriority
[TRC] RESOLVE: Viktor, Innovator (chain_item=21, ability=1, spell=0)
[TRC] TOKEN: created Recruit (1M, id=110) for P2
[DBG] TRIGGER: Viktor, Innovator (P2) fires ability
[DBG] TRIGGER: Viktor, Innovator (P2) fires ability
[TRC] VIKTOR, INNOVATOR: off-turn play -> 1[M] Recruit in base
[TRC] DECISION #88 (P2 — chain priority): 1 legal actions
[TRC] CHOSE: [P2] PassPriority
[TRC] DECISION #89 (P1 — chain priority): 1 legal actions
[TRC] CHOSE: [P1] PassPriority
[TRC] RESOLVE: Viktor, Innovator (chain_item=23, ability=1, spell=0)
[TRC] TOKEN: created Recruit (1M, id=111) for P2
[DBG] TRIGGER: Viktor, Innovator (P2) fires ability
[DBG] TRIGGER: Viktor, Innovator (P2) fires ability
[TRC] VIKTOR, INNOVATOR: off-turn play -> 1[M] Recruit in base
deck A 8392194388957710676 (Blind Monk / Lee Sin, Ascetic)
deck B 6231710079672200936 (Swift Scout / Teemo, Scout)
game seed 412255519: chain_runaway: a chain had 12 items finalized without emptying; most are of Viktor, Innovator (8), Teemo, Scout (2), Ravenbloom Prefect (1)
winner None, score 0-4, turn 9, 89 decisions
```
- **What the engine does without the driver's stop:** `ChainManager::processFEPR` gives up after 1,000
  passes, drops the chain with one warning line and plays on with about 1,000 Recruits. I then saw, in
  three games: 8,285 decisions and then memory without bound (21 GB resident of 23 when I killed it);
  over 300 s inside one decision (Divine Judgment over a thousand units); 3,993 "move another unit with
  this move" prompts in a row. Seeds of the first: `--replay 11351480819872766142 8437191995691559575
  1178137619 --max-chain 0 --memory-limit-gb 3` (ends in `std::bad_alloc` after 11 s).
- **Reading:** the card registers `WhenYouPlayAUnit` (with spell and gear) to stand for "When you play a
  card on an opponent's turn". Tokens are played (CR 350.2, `TokenCreatedEvent`), so the Recruit it plays
  fires it again. A token is not a card; the text should not fire on it. Separately: the 1,000-pass bound
  hides a loop instead of failing the game, which is why this went unseen.

### F2 — Alpha Strike (751): its finalize pass returns before the marker

- **Count:** 188 / 252 / 235 per million.
- **Exception:** `GameEngine::runFinalizePass: Alpha Strike returned from its finalize pass before its
  end-of-choices marker and not because a choice helper had nothing to decide (the rule is at
  Card::finalizingChoices)`
- **Cards:** Alpha Strike, when its choices are made without a Play intent: played by Blind Fury or
  Promising Future, or given new choices by Mystic Reversal (8 of the 10 shortest games of the first pass).
- **Reproduction** (22 decisions): `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay 1275224568967422562 7261107329683671889 1602439880 --trace --tail 30`
- **Trace:**
```
[TRC] PLAY: Recruit (ZN) (id=18, Unit, cost=0E)
[TRC] PAY_COST: Recruit (ZN) E=0 P=0
[TRC] DECISION #13 (P1): 5 legal actions
[TRC] CHOSE: [P1] StandardMove units=[Recruit (ZN)]
[TRC] INTENT: StandardMove by P1
[TRC] MOVE: Recruit (ZN) (id=19) -> BF#1
[TRC] SHOWDOWN: BF#1 (non-combat)
[TRC] INTENT: PlayActionCard by P1
[TRC] SPELL: Blind Fury (id=39) targets=[]
[TRC] PAY_COST: Blind Fury E=4 P=2
[TRC] DECISION #15 (P1): pick rune to exhaust for energy [4 options]
[TRC]   EXHAUST_RUNE: Fury Rune (id=52) for energy [agent choice]
[TRC] DECISION #16 (P1): pick rune to exhaust for energy [3 options]
[TRC]   EXHAUST_RUNE: Chaos Rune (id=45) for energy [agent choice]
[TRC] DECISION #17 (P1): pick rune to exhaust for energy [2 options]
[TRC]   EXHAUST_RUNE: Chaos Rune (id=50) for energy [agent choice]
[TRC] DECISION #18 (P1): pick rune to exhaust for energy [1 options]
[TRC]   EXHAUST_RUNE: Fury Rune (id=42) for energy [agent choice]
[TRC] DECISION #19 (P1): pick rune to recycle for power [2 options]
[TRC]   RECYCLE_RUNE: Fury Rune (id=52, exhausted) for power [agent choice]
[TRC] DECISION #20 (P1): pick rune to recycle for power [1 options]
[TRC]   RECYCLE_RUNE: Fury Rune (id=42, exhausted) for power [agent choice]
[TRC] DECISION #21 (P1 — chain priority): 1 legal actions
[TRC] CHOSE: [P1] PassPriority
[TRC] DECISION #22 (P2 — chain priority): 1 legal actions
[TRC] CHOSE: [P2] PassPriority
[TRC] RESOLVE: Blind Fury (chain_item=4, ability=0, spell=1)
[TRC] BLIND FURY: opponent reveals Alpha Strike
[TRC] EFFECT: banish Alpha Strike (id=58)
[TRC] FINALIZE-CHOICES: Alpha Strike (chain_item=5, ability=0, spell=1)
deck A 1275224568967422562 (Glorious Executioner / Draven, Audacious)
deck B 7261107329683671889 (Wuju Bladesman - Starter / Yi, Honed)
game seed 1602439880: engine_error: GameEngine::runFinalizePass: Alpha Strike returned from its finalize pass before its end-of-choices marker and not because a choice helper had nothing to decide (th
winner None, score 0-0, turn 2, 22 decisions
```
- **Reading:** `AlphaStrike::onResolve` opens with `if (targets.empty() || !ctx.state.objectExists(targets[0])) return;`.
  The friendly unit is a play-time target that only a Play intent supplies; an effect's play and "make new
  choices" supply none, so the pass leaves at that line. It is the only finalize-pass spell of the 34 with
  that line.

### F3 — Board state written to an object that is no longer on the board (checked build only)

- **Count:** 180 in the 300,000 checked games on random decks (32 / 53 / 95), 34 in the 60,000 before
  them. The optimised build does not check this: there the object silently keeps the state.
- **Exception:** `GameEngine::recalculateAuras: <object> (id=N, zone N) is off the board and still carries
  board state: it left the board by a path that does not call EffectExecutor::leftBoard`. The object named
  is the one written to (36 different names), not the card that wrote. I replayed the 145 distinct example
  games the result files hold and sorted them by the card that resolved last:

| Writer | Examples | What is written, to what | Fewest decisions | Replay (checked build) |
|---|---|---|---|---|
| Green Father (753) | 67 | replaces a battlefield with a Brush token; the replaced battlefield card (Gardens of Becoming, Forge of the Fluft, ...) goes to Banishment with its board state | 59 | `RIFTBOUND_ROOT=. ./build-check/deck_fuzz --replay 16794050529343751079 9683174656529673871 1687156847 --trace --tail 30` |
| Fading Memories | 47 | "gains [Temporary]" on a target that has left: a unit returned to hand, a Gold token that was killed for its ability | 43 | `RIFTBOUND_ROOT=. ./build-check/deck_fuzz --replay 5597636225965851729 128808137382265066 1833423049 --agent1 random-player --agent2 random-player --trace --tail 30` |
| Vi, Hotheaded (592) | 17 | its own activated ability resolves after Vi died: +Might this turn on the card in the trash | 33 | `RIFTBOUND_ROOT=. ./build-check/deck_fuzz --replay 5636454921321490439 6116819903138382408 83937122 --agent1 random-player --agent2 random-player --trace --tail 30` |
| Pendulum Blade | 14 | +2 [M] this turn to the unit it was on, after that unit left | 61 | `RIFTBOUND_ROOT=. ./build-check/deck_fuzz --replay 13526038138039490185 9015023040755099300 212409093 --agent1 random-player:pass=0 --agent2 random-player:pass=0 --trace --tail 30` |

  The sorting is by the last card to resolve, and I read one game of each label. In 21 examples that
  card was a bystander (Battle Mistress 15, Forgotten Library 3, Abandoned Hall 1, Ravenbloom Student 1,
  Master of Shadows 1): in the game I read of each, the write was Fading Memories', a few lines earlier
  (the check runs at the next `recalculateAuras`), and I count all 21 under Fading Memories. Two examples
  named Akali, Deadly Weapon and Traveling Merchant with Pendulum Blade recalled in the last lines: counted
  under Pendulum Blade. The split of the 145 is therefore exact for Green Father and Vi and an attribution
  for the other two.
- **Trace** (Green Father):
```
[TRC] CHOSE: [P1] PassPriority
[TRC] DECISION #55 (P2 — chain priority): 1 legal actions
[TRC] CHOSE: [P2] PassPriority
[TRC] RESOLVE: The List (chain_item=13, ability=1, spell=0)
[TRC] EFFECT: give Apprentice Smith -2 [M] this turn -> 0M
[TRC] THE LIST: -2 [M] this turn to a "Freljord" unit
[TRC] DECISION #56 (P1): 1 legal actions
[TRC] CHOSE: [P1] EndTurn
[TRC] ── Ending Step ──
[TRC] ── Expiration Step ──
[TRC] ════ TURN 6 (P2) ════ Score: P1=0 P2=2
[TRC] ── Awaken Phase ──
[TRC] ── Beginning Step ──
[TRC] ── Scoring Step ──
[TRC] SCORE: P2 +1 via Hold BF#0 -> 3
[DBG] TRIGGER: Green Father (P2) fires ability
[TRC] FINALIZE-CHOICES: Green Father (chain_item=14, ability=1, spell=0)
[TRC] MAY_PROMPT: Green Father: exhaust to replace that battlefield with Brush? (P=P2, deferred to agent)
[TRC] CHOICE-REQUEST (finalize): may: Green Father: exhaust to replace that battlefield with Brush? [2 options] (P2)
[TRC] DECISION #57 (P2): may: Green Father: exhaust to replace that battlefield with Brush? [2 options]
[TRC] CHOSE: [P2] MakeChoice
[TRC] FINALIZE-CHOICES: Green Father (chain_item=14, ability=1, spell=0)
[TRC] MAY_ACCEPTED: Green Father: exhaust to replace that battlefield with Brush?
[TRC] DECISION #58 (P2 — chain priority): 1 legal actions
[TRC] CHOSE: [P2] PassPriority
[TRC] DECISION #59 (P1 — chain priority): 1 legal actions
[TRC] CHOSE: [P1] PassPriority
[TRC] RESOLVE: Green Father (chain_item=14, ability=1, spell=0)
[TRC] BF_REPLACE: BF#0 replaced by 'Brush' token (card=110)
[TRC] GREEN FATHER: exhausted to replace BF#0 with Brush token
deck A 16794050529343751079 (Gloomist / Vex, Apathetic)
deck B 9683174656529673871 (Green Father / Ivern, Nurturer)
game seed 1687156847: engine_error: GameEngine::recalculateAuras: Gardens of Becoming (id=107, zone 5) is off the board and still carries board state: it left the board by a path that does not call Ef
winner None, score 0-3, turn 6, 59 decisions
```
- **Trace** (Fading Memories):
```
[TRC] FINALIZE-CHOICES: Windsinger (chain_item=6, ability=1, spell=0)
[TRC] TGT_PICKED: Windsinger = Shipyard Skulker (id=30)
[DBG] TRIGGER: Blade Dancer (P1) fires ability
[TRC] FINALIZE-CHOICES: Blade Dancer (chain_item=8, ability=1, spell=0)
[TRC] MAY_PROMPT: Blade Dancer: exhaust me + pay [A] to ready the chosen unit? (P=P1, deferred to agent)
[TRC] CHOICE-REQUEST (finalize): may: Blade Dancer: exhaust me + pay [A] to ready the chosen unit? [2 options] (P1)
[TRC] DECISION #40 (P1): may: Blade Dancer: exhaust me + pay [A] to ready the chosen unit? [2 options]
[TRC] CHOSE: [P1] MakeChoice
[TRC] FINALIZE-CHOICES: Blade Dancer (chain_item=8, ability=1, spell=0)
[TRC] MAY_ACCEPTED: Blade Dancer: exhaust me + pay [A] to ready the chosen unit?
[TRC] PAY_ADD: P1 pays [0] + 1 power (any): exhausted 0, recycled 1
[TRC] DECISION #41 (P1 — chain priority): 1 legal actions
[TRC] CHOSE: [P1] PassPriority
[TRC] DECISION #42 (P2 — chain priority): 1 legal actions
[TRC] CHOSE: [P2] PassPriority
[TRC] RESOLVE: Blade Dancer (chain_item=8, ability=1, spell=0)
[TRC] BLADE DANCER: exhaust me + [A] -> ready Shipyard Skulker
[TRC] DECISION #43 (P1 — chain priority): 1 legal actions
[TRC] CHOSE: [P1] PassPriority
[TRC] DECISION #44 (P2 — chain priority): 1 legal actions
[TRC] CHOSE: [P2] PassPriority
[TRC] RESOLVE: Windsinger (chain_item=6, ability=1, spell=0)
[TRC] EFFECT: bounce Shipyard Skulker (id=30) to hand
[TRC] WINDSINGER: returned Shipyard Skulker to hand
[TRC] DECISION #45 (P2 — chain priority): 1 legal actions
[TRC] CHOSE: [P2] PassPriority
[TRC] DECISION #46 (P1 — chain priority): 1 legal actions
[TRC] CHOSE: [P1] PassPriority
[TRC] RESOLVE: Fading Memories (chain_item=5, ability=0, spell=1)
[TRC] FADING MEMORIES: Shipyard Skulker gains [Temporary]
deck A 5321711823005117331 (Blade Dancer / Irelia, Fervent)
deck B 16686925118191993966 (Bloodharbor Ripper / Pyke, Returned)
game seed 745873444: engine_error: GameEngine::recalculateAuras: Shipyard Skulker (id=30, zone 2) is off the board and still carries board state: it left the board by a path that does not call EffectE
winner None, score 1-0, turn 4, 46 decisions
```
- **Trace** (Vi, Hotheaded):
```
[TRC]   DREW: Back to Back (id=67)
[TRC] ── Main Phase ──
[TRC] DECISION #26 (P2): 9 legal actions
[TRC] CHOSE: [P2] ActivateAbility (src=Vi, Hotheaded)
[TRC] INTENT: ActivateAbility by P2
[TRC] PAY_ADD: P2 pays [2] + 1 power: exhausted 2, recycled 1
[TRC] DECISION #27 (P2 — chain priority): 3 legal actions
[TRC] CHOSE: [P2] PlayReaction
[TRC] INTENT: PlayReaction by P2
[TRC] SPELL: Deathgrip (id=74) targets=[Vi, Hotheaded(id=70)]
[TRC] PAY_COST: Deathgrip E=2 P=0
[TRC] DECISION #28 (P2): pick rune to exhaust for energy [3 options]
[TRC]   EXHAUST_RUNE: Order Rune (id=104) for energy [agent choice]
[TRC] DECISION #29 (P2): pick rune to exhaust for energy [2 options]
[TRC]   EXHAUST_RUNE: Fury Rune (id=106) for energy [agent choice]
[TRC] DECISION #30 (P2 — chain priority): 1 legal actions
[TRC] CHOSE: [P2] PassPriority
[TRC] DECISION #31 (P1 — chain priority): 1 legal actions
[TRC] CHOSE: [P1] PassPriority
[TRC] RESOLVE: Deathgrip (chain_item=6, ability=0, spell=1)
[TRC] EFFECT: kill Vi, Hotheaded (id=70)
[TRC] EFFECT: P2 draws 1 (deck=33)
[TRC]   DREW: Twilight Reveler (id=73)
[TRC] DECISION #32 (P2 — chain priority): 1 legal actions
[TRC] CHOSE: [P2] PassPriority
[TRC] DECISION #33 (P1 — chain priority): 1 legal actions
[TRC] CHOSE: [P1] PassPriority
[TRC] RESOLVE: Vi, Hotheaded (chain_item=5, ability=1, spell=0)
[TRC] EFFECT: give Vi, Hotheaded 3 [M] this turn -> 6M
[TRC] VI HOTHEADED: doubled Might this turn (+3)
deck A 5636454921321490439 (Purifier / Lucian, Gunslinger)
deck B 6116819903138382408 (Piltover Enforcer / Vi, Hotheaded)
game seed 83937122: engine_error: GameEngine::recalculateAuras: Vi, Hotheaded (id=70, zone 4) is off the board and still carries board state: it left the board by a path that does not call EffectExecu
winner None, score 0-0, turn 4, 33 decisions
```
- **Trace** (Pendulum Blade):
```
[TRC] EFFECT: unattach Pendulum Blade (id=76)
[TRC] STALKING WOLF: killed a Bird/Cat/Dog/Poro (additional cost)
[TRC] PAY_COST: Stalking Wolf E=4 P=1
[TRC] DECISION #52 (P2): pick rune to exhaust for energy [5 options]
[TRC]   EXHAUST_RUNE: Fury Rune (id=98) for energy [agent choice]
[TRC] DECISION #53 (P2): pick rune to exhaust for energy [4 options]
[TRC]   EXHAUST_RUNE: Fury Rune (id=104) for energy [agent choice]
[TRC] DECISION #54 (P2): pick rune to exhaust for energy [3 options]
[TRC]   EXHAUST_RUNE: Order Rune (id=99) for energy [agent choice]
[TRC] DECISION #55 (P2): pick rune to exhaust for energy [2 options]
[TRC]   EXHAUST_RUNE: Order Rune (id=97) for energy [agent choice]
[TRC] DECISION #56 (P2): pick rune to recycle for power [3 options]
[TRC]   RECYCLE_RUNE: Order Rune (id=99, exhausted) for power [agent choice]
[TRC] DECISION #57 (P2 — chain priority): 2 legal actions
[TRC] CHOSE: [P2] ActivateReactionAbility
[TRC] DECISION #58 (P2 — chain priority): 1 legal actions
[TRC] CHOSE: [P2] PassPriority
[TRC] DECISION #59 (P1 — chain priority): 1 legal actions
[TRC] CHOSE: [P1] PassPriority
[TRC] RESOLVE: Hand of Noxus (chain_item=12, ability=1, spell=0)
[TRC] EFFECT: P2 adds 1 floating Energy (pool=1)
[TRC] HAND OF NOXUS: [E] -> [Add] [1]
[TRC] DECISION #60 (P2 — chain priority): 1 legal actions
[TRC] CHOSE: [P2] PassPriority
[TRC] DECISION #61 (P1 — chain priority): 1 legal actions
[TRC] CHOSE: [P1] PassPriority
[TRC] RESOLVE: Pendulum Blade (chain_item=10, ability=1, spell=0)
[TRC] EFFECT: give Rengar, Pouncing 2 [M] this turn -> 5M
[TRC] RECALL (CR 323.6): Pendulum Blade (id=76)
[TRC] RECALL: Pendulum Blade (id=76) to base
deck A 13526038138039490185 (Blind Monk / Lee Sin, Ascetic)
deck B 9015023040755099300 (Hand of Noxus / Darius, Executioner)
game seed 212409093: engine_error: GameEngine::recalculateAuras: Rengar, Pouncing (id=94, zone 4) is off the board and still carries board state: it left the board by a path that does not call EffectE
winner None, score 1-0, turn 5, 61 decisions
```
- **Reading:** two different faults behind one message. Green Father's battlefield replacement moves the
  replaced card off the board without `leftBoard` (what the message says). The other three act on an
  object because it still exists as an id (`objectExists`), not because it is on the board: their effect
  is applied after the target or the source has left.

### F4 — Emperor's Divide (366): the same unit is offered again after its move was blocked

- **Count:** 382 per million with pass=0, 44 per 100,000 on the checked build; 0 with the other agents.
- **Exception:** `ChainManager::runResumable: Emperor's Divide asked more than 1024 choices while resolving`
- **Reproduction** (1,041 decisions, of which about 1,024 are the prompt):
  `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay 7035018309797515888 7060025083829224003 1543643482 --agent1 random-player:pass=0 --agent2 random-player:pass=0 --trace --tail 30`
- **Trace:**
```
[TRC] RESOLVE: Emperor's Divide (chain_item=3, ability=0, spell=1)
[TRC] MOVE_BLOCKED: Determined Sentry can't move to base
[TRC] EMPEROR'S DIVIDE: moved a unit to base
[TRC] CHOICE-REQUEST: Emperor's Divide: move a unit to base, or stop [2 options] (P2)
[TRC] DECISION #1037 (P2): Emperor's Divide: move a unit to base, or stop [2 options]
[TRC] CHOSE: [P2] MakeChoice
[TRC] RESOLVE: Emperor's Divide (chain_item=3, ability=0, spell=1)
[TRC] MOVE_BLOCKED: Determined Sentry can't move to base
[TRC] EMPEROR'S DIVIDE: moved a unit to base
[TRC] CHOICE-REQUEST: Emperor's Divide: move a unit to base, or stop [2 options] (P2)
[TRC] DECISION #1038 (P2): Emperor's Divide: move a unit to base, or stop [2 options]
[TRC] CHOSE: [P2] MakeChoice
[TRC] RESOLVE: Emperor's Divide (chain_item=3, ability=0, spell=1)
[TRC] MOVE_BLOCKED: Determined Sentry can't move to base
[TRC] EMPEROR'S DIVIDE: moved a unit to base
[TRC] CHOICE-REQUEST: Emperor's Divide: move a unit to base, or stop [2 options] (P2)
[TRC] DECISION #1039 (P2): Emperor's Divide: move a unit to base, or stop [2 options]
[TRC] CHOSE: [P2] MakeChoice
[TRC] RESOLVE: Emperor's Divide (chain_item=3, ability=0, spell=1)
[TRC] MOVE_BLOCKED: Determined Sentry can't move to base
[TRC] EMPEROR'S DIVIDE: moved a unit to base
[TRC] CHOICE-REQUEST: Emperor's Divide: move a unit to base, or stop [2 options] (P2)
[TRC] DECISION #1040 (P2): Emperor's Divide: move a unit to base, or stop [2 options]
[TRC] CHOSE: [P2] MakeChoice
[TRC] RESOLVE: Emperor's Divide (chain_item=3, ability=0, spell=1)
[TRC] MOVE_BLOCKED: Determined Sentry can't move to base
[TRC] EMPEROR'S DIVIDE: moved a unit to base
[TRC] CHOICE-REQUEST: Emperor's Divide: move a unit to base, or stop [2 options] (P2)
[TRC] DECISION #1041 (P2): Emperor's Divide: move a unit to base, or stop [2 options]
[TRC] CHOSE: [P2] MakeChoice
deck A 7035018309797515888 (Void Burrower / Rek'Sai, Swarm Queen)
deck B 7060025083829224003 (Wuju Bladesman - Starter / Master Yi, Unstoppable)
game seed 1543643482: engine_error: ChainManager::runResumable: Emperor's Divide asked more than 1024 choices while resolving
winner None, score 0-0, turn 2, 1041 decisions
```
- **Reading:** "Move any number of friendly units at a battlefield to their base." A unit whose move to
  base is blocked (`MOVE_BLOCKED: ... can't move to base`) stays in the list, the card logs "moved a unit
  to base" and asks again. An agent that never picks "stop" is asked 1,024 times. A random agent stops by
  chance, so plain random play never showed it; the wrong log line and the re-offer are there for any
  agent.

### F5 — "You may pay [N] to ..." triggers: the Deflect of the chosen unit is paid first and the [N] can no longer be paid

- **Count per million:** Kennen, Keeper of Balance (922) 3 / 5 / 9; Sinister Poro (699) 0 / 0 / 5;
  Icevale Archer (627) 0 / 1 / 1. Also the one failure of the deck lists (`irelia.txt` against
  `leblanc.txt`, pass=0, both builds).
- **Exception:** `GameEngine::runFinalizePass: <card> returned from its finalize pass before its
  end-of-choices marker ...`
- **Reproductions:**
  - Kennen (32 decisions): `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay 11476230185063871233 5669109198155917197 1428537605 --agent1 random-player:pass=0 --agent2 random-player:pass=0 --trace --tail 30`
  - Kennen, deck lists (54): `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay decks/irelia.txt decks/leblanc.txt 114701520 --agent1 random-player:pass=0 --agent2 random-player:pass=0 --trace --tail 30`
  - Sinister Poro (57): `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay 11432795166332029498 14790590402838137845 1582996165 --agent1 random-player:pass=0 --agent2 random-player:pass=0 --trace --tail 30`
  - Icevale Archer (255): `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay 14269406505543956759 6133059986897357316 379721881 --agent1 random-player:pass=0 --agent2 random-player:pass=0 --trace --tail 30`
- **Trace** (Kennen):
```
[TRC]   EXHAUST_RUNE: Order Rune (id=98) for energy [agent choice]
[TRC] DECISION #26 (P2): pick rune to exhaust for energy [2 options]
[TRC]   EXHAUST_RUNE: Body Rune (id=96) for energy [agent choice]
[TRC] DECISION #27 (P2): pick rune to exhaust for energy [1 options]
[TRC]   EXHAUST_RUNE: Body Rune (id=102) for energy [agent choice]
[TRC] DECISION #28 (P2): pick rune to recycle for power [3 options]
[TRC]   RECYCLE_RUNE: Body Rune (id=96, exhausted) for power [agent choice]
[TRC] DECISION #29 (P2 — chain priority): 1 legal actions
[TRC] CHOSE: [P2] PassPriority
[TRC] DECISION #30 (P1 — chain priority): 2 legal actions
[TRC] CHOSE: [P1] PlayReaction
[TRC] INTENT: PlayReaction by P1
[DBG] TRIGGER: Kennen, Keeper of Balance (P1) fires ability
[DBG] TRIGGER: Valley of Idols (P1) fires ability
[DBG] TRIGGER: Heart of the Tempest (P1) fires ability
[TRC] FINALIZE-CHOICES: Kennen, Keeper of Balance (chain_item=5, ability=1, spell=0)
[TRC] MAY_PROMPT: Kennen, Keeper of Balance: pay [2] to stun a unit? (P=P1, deferred to agent)
[TRC] CHOICE-REQUEST (finalize): may: Kennen, Keeper of Balance: pay [2] to stun a unit? [2 options] (P1)
[TRC] DECISION #31 (P1): may: Kennen, Keeper of Balance: pay [2] to stun a unit? [2 options]
[TRC] CHOSE: [P1] MakeChoice
[TRC] FINALIZE-CHOICES: Kennen, Keeper of Balance (chain_item=5, ability=1, spell=0)
[TRC] MAY_ACCEPTED: Kennen, Keeper of Balance: pay [2] to stun a unit?
[TRC] TGT_PROMPT: Kennen, Keeper of Balance: stun [Kennen, Keeper of Balance/Noxian Emissary/Galio, Indefatigable] (agent decides among 3 targets)
[TRC] CHOICE-REQUEST (finalize): target: Kennen, Keeper of Balance: stun [3 options] (P1)
[TRC] DECISION #32 (P1): target: Kennen, Keeper of Balance: stun [3 options]
[TRC] CHOSE: [P1] MakeChoice
[TRC] FINALIZE-CHOICES: Kennen, Keeper of Balance (chain_item=5, ability=1, spell=0)
[TRC] TGT_PICKED: Kennen, Keeper of Balance: stun = Galio, Indefatigable (id=87)
[TRC] PAY_ADD: P1 pays [0] + 1 power (any): exhausted 0, recycled 1
[TRC] DEFLECT: P1 pays 1 [A] to choose Galio, Indefatigable
deck A 11476230185063871233 (Heart of the Tempest / Kennen, Keeper of Balance)
deck B 5669109198155917197 (Might of Demacia - Starter / Garen, Rugged)
game seed 1428537605: engine_error: GameEngine::runFinalizePass: Kennen, Keeper of Balance returned from its finalize pass before its end-of-choices marker and not because a choice helper had nothing 
winner None, score 1-0, turn 3, 32 decisions
```
- **Trace** (Sinister Poro):
```
[TRC] CONQUER: P1 conquers BF#0 (score was 0)
[TRC] SCORE: P1 +1 via Conquer -> 1
[TRC] SHOWDOWN: BF#1 (non-combat)
[TRC] CONQUER: P2 conquers BF#1 (score was 0)
[TRC] SCORE: P2 +1 via Conquer -> 1
[TRC] DECISION #54 (P1): 4 legal actions
[TRC] CHOSE: [P1] StandardMove units=[Demacian Diplomat]
[TRC] INTENT: StandardMove by P1
[TRC] MOVE: Demacian Diplomat (id=13) -> Base(P1)
[TRC] DECISION #55 (P1): 3 legal actions
[TRC] CHOSE: [P1] StandardMove units=[Sinister Poro]
[TRC] INTENT: StandardMove by P1
[TRC] MOVE: Sinister Poro (id=24) -> BF#1
[TRC] COMBAT: BF#1 attacker=P1 defender=P2
[DBG] TRIGGER: Sinister Poro (P1) fires ability
[TRC] FINALIZE-CHOICES: Sinister Poro (chain_item=9, ability=1, spell=0)
[TRC] MAY_PROMPT: Sinister Poro: pay [1] to move an enemy unit here? (P=P1, deferred to agent)
[TRC] CHOICE-REQUEST (finalize): may: Sinister Poro: pay [1] to move an enemy unit here? [2 options] (P1)
[TRC] DECISION #56 (P1): may: Sinister Poro: pay [1] to move an enemy unit here? [2 options]
[TRC] CHOSE: [P1] MakeChoice
[TRC] FINALIZE-CHOICES: Sinister Poro (chain_item=9, ability=1, spell=0)
[TRC] MAY_ACCEPTED: Sinister Poro: pay [1] to move an enemy unit here?
[TRC] TGT_PROMPT: Sinister Poro: choose an enemy unit here [Jax, Unmatched] (agent decides among 1 targets)
[TRC] CHOICE-REQUEST (finalize): target: Sinister Poro: choose an enemy unit here [1 options] (P1)
[TRC] DECISION #57 (P1): target: Sinister Poro: choose an enemy unit here [1 options]
[TRC] CHOSE: [P1] MakeChoice
[TRC] FINALIZE-CHOICES: Sinister Poro (chain_item=9, ability=1, spell=0)
[TRC] TGT_PICKED: Sinister Poro: choose an enemy unit here = Jax, Unmatched (id=55)
[TRC] PAY_ADD: P1 pays [0] + 1 power (any): exhausted 0, recycled 1
[TRC] DEFLECT: P1 pays 1 [A] to choose Jax, Unmatched
deck A 11432795166332029498 (Voidreaver / Kha'Zix, Mutating Horror)
deck B 14790590402838137845 (Grandmaster at Arms / Jax, Unmatched)
game seed 1582996165: engine_error: GameEngine::runFinalizePass: Sinister Poro returned from its finalize pass before its end-of-choices marker and not because a choice helper had nothing to decide (t
winner None, score 1-1, turn 5, 57 decisions
```
- **Reading:** this is the second lead of `plans/reports/checkpoint-followup-2.md`, in triggered
  abilities. The card asks "pay [N]?" while N is payable, then `pickTarget` pays the chosen enemy unit's
  Deflect by recycling runes, and `if (costDueNow(ctx) && !payEnergy(...)) return;` then fails before the
  marker (Icevale Archer's code; the other two end the same way). The "may" was judged without the
  Deflect, and the pick list is not limited to units that leave the cost payable, as it is for a spell
  since 9.VV1. Every "may pay [N] to <choose a unit>" trigger is a candidate; these three are the ones
  that met a [Deflect] unit with exactly too few runes.

### F6 — "Kill a friendly unit" as an additional cost, the death replaced for a price

- **Count per million:** Heedless Resurrection 0 / 1 / 6; Sacrifice 0 / 4 / 3.
- **Exception:** `GameEngine::executePlaySpell: <card> made choices it cannot be paid for with`
- **Reproductions:**
  - Sacrifice with The Boss (49 decisions): `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay 13433292000956441714 16384960977906598976 557383246 --agent1 random-player:pass=0 --agent2 random-player:pass=0 --trace --tail 30`
  - Heedless Resurrection with Altar of Blood (73): `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay 10709284387757590375 4424407890075260170 1701196939 --agent1 random-player:pass=0 --agent2 random-player:pass=0 --trace --tail 30`
- **Trace** (Sacrifice):
```
[DBG] TRIGGER: Sett, Brawler (P1) fires ability
[TRC] DECISION #44 (P1 — chain priority): 1 legal actions
[TRC] CHOSE: [P1] PassPriority
[TRC] DECISION #45 (P2 — chain priority): 1 legal actions
[TRC] CHOSE: [P2] PassPriority
[TRC] RESOLVE: Sett, Brawler (chain_item=7, ability=1, spell=0)
[TRC] EFFECT: [Buff] Sett, Brawler (+1 permanent) -> 5M
[TRC] SETT: buff me (+1M buff)
[TRC] DECISION #46 (P1): 5 legal actions
[TRC] CHOSE: [P1] StandardMove units=[Laurent Duelist]
[TRC] INTENT: StandardMove by P1
[TRC] MOVE: Laurent Duelist (id=30) -> BF#1
[TRC] COMBAT: BF#1 attacker=P1 defender=P2
[TRC] INTENT: PlayCard by P1
[TRC] SPELL: Sacrifice (id=3) targets=[]
[TRC] FINALIZE-CHOICES: Sacrifice (chain_item=9, ability=0, spell=1)
[TRC] TGT_PROMPT: Sacrifice: kill a Mighty unit [Sett, Brawler/Laurent Duelist] (agent decides among 2 targets)
[TRC] CHOICE-REQUEST (finalize): target: Sacrifice: kill a Mighty unit [2 options] (P1)
[TRC] DECISION #48 (P1): target: Sacrifice: kill a Mighty unit [2 options]
[TRC] CHOSE: [P1] MakeChoice
[TRC] FINALIZE-CHOICES: Sacrifice (chain_item=9, ability=0, spell=1)
[TRC] TGT_PICKED: Sacrifice: kill a Mighty unit = Sett, Brawler (id=2)
[TRC] EFFECT: kill Sett, Brawler (id=2)
[TRC] ASK_PROMPT: The Boss: pay [C], exhaust me and spend Sett, Brawler's buff to recall it instead of dying
[TRC] DECISION #49 (P1): The Boss: pay [C], exhaust me and spend Sett, Brawler's buff to recall it instead of dying [2 options]
[TRC] CHOSE: [P1] MakeChoice
[TRC] PAY_ADD: P1 pays [0] + 1 power: exhausted 0, recycled 1
[TRC] EFFECT: P1 spends a buff from Sett, Brawler -> 4M
[TRC] THE BOSS: save buffed unit Sett, Brawler (pay [C], exhaust me, spend buff) -> heal/exhaust/recall
[DBG] REPLACEMENT: The Boss prevents Sett, Brawler from dying
deck A 13433292000956441714 (The Boss / Sett, Brawler)
deck B 16384960977906598976 (Battle Mistress / Sivir, Ambitious)
game seed 557383246: engine_error: GameEngine::executePlaySpell: Sacrifice made choices it cannot be paid for with
winner None, score 0-1, turn 4, 49 decisions
```
- **Trace** (Heedless Resurrection):
```
[DBG] TRIGGER: Ezreal, Dashing (P2) fires ability
[TRC] FINALIZE-CHOICES: Ezreal, Dashing (chain_item=15, ability=1, spell=0)
[TRC] TGT_PROMPT: Ezreal: deal Might damage to an enemy here [Mournful Witness] (agent decides among 1 targets)
[TRC] CHOICE-REQUEST (finalize): target: Ezreal: deal Might damage to an enemy here [1 options] (P2)
[TRC] DECISION #69 (P2): target: Ezreal: deal Might damage to an enemy here [1 options]
[TRC] CHOSE: [P2] MakeChoice
[TRC] FINALIZE-CHOICES: Ezreal, Dashing (chain_item=15, ability=1, spell=0)
[TRC] TGT_PICKED: Ezreal: deal Might damage to an enemy here = Mournful Witness (id=22)
[TRC] DECISION #70 (P2 — chain priority): 3 legal actions
[TRC] CHOSE: [P2] ActivateReactionAbility
[TRC] DECISION #71 (P2 — chain priority): 2 legal actions
[TRC] CHOSE: [P2] PlayReaction
[TRC] INTENT: PlayReaction by P2
[TRC] SPELL: Heedless Resurrection (id=59) targets=[]
[TRC] FINALIZE-CHOICES: Heedless Resurrection (chain_item=17, ability=0, spell=1)
[TRC] TGT_PROMPT: Heedless Resurrection: kill a friendly unit [Ezreal, Dashing] (agent decides among 1 targets)
[TRC] CHOICE-REQUEST (finalize): target: Heedless Resurrection: kill a friendly unit [1 options] (P2)
[TRC] DECISION #72 (P2): target: Heedless Resurrection: kill a friendly unit [1 options]
[TRC] CHOSE: [P2] MakeChoice
[TRC] FINALIZE-CHOICES: Heedless Resurrection (chain_item=17, ability=0, spell=1)
[TRC] TGT_PICKED: Heedless Resurrection: kill a friendly unit = Ezreal, Dashing (id=55)
[TRC] HEEDLESS RESURRECTION: killing Ezreal, Dashing (additional cost)
[TRC] EFFECT: kill Ezreal, Dashing (id=55)
[TRC] ASK_PROMPT: Altar of Blood: pay [A][A][A] to heal, exhaust and recall Ezreal, Dashing?
[TRC] DECISION #73 (P2): Altar of Blood: pay [A][A][A] to heal, exhaust and recall Ezreal, Dashing? [2 options]
[TRC] CHOSE: [P2] MakeChoice
[TRC] PAY_ADD: P2 pays [0] + 3 power (any): exhausted 3, recycled 3
[TRC] ALTAR OF BLOOD: Ezreal, Dashing pays [A][A][A] -> heal/exhaust/recall
[TRC] RECALL: Ezreal, Dashing (id=55) to base
[DBG] REPLACEMENT: Altar of Blood prevents Ezreal, Dashing from dying
deck A 10709284387757590375 (Blade Dancer / Irelia, Fervent)
deck B 4424407890075260170 (Prodigal Explorer / Ezreal, Dashing)
game seed 1701196939: engine_error: GameEngine::executePlaySpell: Heedless Resurrection made choices it cannot be paid for with
winner None, score 2-0, turn 5, 73 decisions
```
- **Reading:** the spell kills a friendly unit as it makes its choices, before its own cost. A death
  replacement the same player may pay for (The Boss: [C]; Altar of Blood: [A][A][A]) is offered in the
  middle of that, the player pays it from the runes the spell needed, and the play cannot be completed.
  Two questions for the fix batch, not answered here: whether the replacement may be offered at all for a
  kill that is a cost (a cost that does not happen is not paid: `Card::killAsCost`), and that the offer was
  judged without it.

### F7 — Strike Down (429) is offered when no enemy unit can be chosen

- **Count:** 4 / 3 / 4 per million.
- **Exception:** `GameEngine::executePlaySpell: Strike Down made choices it cannot be paid for with`
- **Reproduction** (62 decisions): `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay 7431632622801578709 5088382193658506930 1655487372 --trace --tail 30`
- **Trace:**
```
[TRC]   EXHAUST_RUNE: Chaos Rune (id=102) for energy [agent choice]
[TRC] DECISION #55 (P2): pick rune to exhaust for energy [4 options]
[TRC]   EXHAUST_RUNE: Body Rune (id=96) for energy [agent choice]
[TRC] DECISION #56 (P2): pick rune to exhaust for energy [3 options]
[TRC]   EXHAUST_RUNE: Body Rune (id=103) for energy [agent choice]
[TRC] DECISION #57 (P2): pick rune to recycle for power [2 options]
[TRC]   RECYCLE_RUNE: Chaos Rune (id=97, exhausted) for power [agent choice]
[DBG] TRIGGER: Battle Mistress (P2) fires ability
[TRC] FINALIZE-CHOICES: Battle Mistress (chain_item=15, ability=1, spell=0)
[TRC] MAY_PROMPT: Battle Mistress: exhaust me to play a Gold token exhausted? (P=P2, deferred to agent)
[TRC] CHOICE-REQUEST (finalize): may: Battle Mistress: exhaust me to play a Gold token exhausted? [2 options] (P2)
[TRC] DECISION #58 (P2): may: Battle Mistress: exhaust me to play a Gold token exhausted? [2 options]
[TRC] CHOSE: [P2] MakeChoice
[TRC] FINALIZE-CHOICES: Battle Mistress (chain_item=15, ability=1, spell=0)
[TRC] MAY_DECLINED: Battle Mistress: exhaust me to play a Gold token exhausted?
[TRC] DECISION #59 (P2 — chain priority): 1 legal actions
[TRC] CHOSE: [P2] PassPriority
[TRC] DECISION #60 (P1 — chain priority): 1 legal actions
[TRC] CHOSE: [P1] PassPriority
[TRC] RESOLVE: Battle Mistress (chain_item=15, ability=1, spell=0)
[TRC] DECISION #61 (P2): 5 legal actions
[TRC] CHOSE: [P2] StandardMove units=[Demacian Diplomat]
[TRC] INTENT: StandardMove by P2
[TRC] MOVE: Demacian Diplomat (id=82) -> Base(P2)
[TRC] DECISION #62 (P2): 3 legal actions
[TRC] CHOSE: [P2] PlayCard Strike Down
[TRC] INTENT: PlayCard by P2
[TRC] SPELL: Strike Down (id=88) targets=[]
[TRC] FINALIZE-CHOICES: Strike Down (chain_item=17, ability=0, spell=1)
[TRC] TGT_PAIR_NOT_OFFERED: Strike Down (no legal first targets at resolve time)
deck A 7431632622801578709 (Keeper of the Hammer / Poppy, Paragon)
deck B 5088382193658506930 (Battle Mistress / Sivir, Mercenary)
game seed 1655487372: engine_error: GameEngine::executePlaySpell: Strike Down made choices it cannot be paid for with
winner None, score 0-2, turn 5, 62 decisions
```
- **Reading:** "Choose an equipped friendly unit. It deals damage equal to its Might to an enemy unit."
  `StrikeDown::hasLegalTargets` asks only for an equipped friendly unit; `pickTargetPair` needs an enemy
  unit for the pair as well. With an equipped unit and no enemy unit on the board the play is offered and
  the pass finds nothing to choose (`TGT_PAIR_NOT_OFFERED`).

### F8 — Imposing Challenger and Irresistible Faefolk move each other without end

- **Count:** 9 per million with pass=0 (and 1 in the 20,000 checked games); 0 with the other agents.
- **Stop:** `chain_runaway: a chain had 500 items finalized without emptying; most are of Irresistible
  Faefolk (250), Imposing Challenger (250)`
- **Reproduction** (stopped after 12 chain items): `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay 9991885925897128524 7817196308662062806 1159599994 --agent1 random-player:pass=0 --agent2 random-player:pass=0 --max-chain 12 --trace --tail 30`
- **Trace:**
```
[TRC] DECISION #69 (P1): may: Irresistible Faefolk: move an enemy unit to my battlefield? [2 options]
[TRC] CHOSE: [P1] MakeChoice
[TRC] FINALIZE-CHOICES: Irresistible Faefolk (chain_item=15, ability=1, spell=0)
[TRC] MAY_ACCEPTED: Irresistible Faefolk: move an enemy unit to my battlefield?
[TRC] TGT_PROMPT: Irresistible Faefolk: choose an enemy unit [Imposing Challenger] (agent decides among 1 targets)
[TRC] CHOICE-REQUEST (finalize): target: Irresistible Faefolk: choose an enemy unit [1 options] (P1)
[TRC] DECISION #70 (P1): target: Irresistible Faefolk: choose an enemy unit [1 options]
[TRC] CHOSE: [P1] MakeChoice
[TRC] FINALIZE-CHOICES: Irresistible Faefolk (chain_item=15, ability=1, spell=0)
[TRC] TGT_PICKED: Irresistible Faefolk: choose an enemy unit = Imposing Challenger (id=79)
[TRC] DECISION #71 (P1 — chain priority): 1 legal actions
[TRC] CHOSE: [P1] PassPriority
[TRC] DECISION #72 (P2 — chain priority): 1 legal actions
[TRC] CHOSE: [P2] PassPriority
[TRC] RESOLVE: Irresistible Faefolk (chain_item=15, ability=1, spell=0)
[DBG] TRIGGER: Imposing Challenger (P2) fires ability
[TRC] IRRESISTIBLE FAEFOLK: pulled Imposing Challenger to BF#0
[TRC] FINALIZE-CHOICES: Imposing Challenger (chain_item=16, ability=1, spell=0)
[TRC] MAY_PROMPT: Imposing Challenger: move an enemy unit here? (P=P2, deferred to agent)
[TRC] CHOICE-REQUEST (finalize): may: Imposing Challenger: move an enemy unit here? [2 options] (P2)
[TRC] DECISION #73 (P2): may: Imposing Challenger: move an enemy unit here? [2 options]
[TRC] CHOSE: [P2] MakeChoice
[TRC] FINALIZE-CHOICES: Imposing Challenger (chain_item=16, ability=1, spell=0)
[TRC] MAY_ACCEPTED: Imposing Challenger: move an enemy unit here?
[TRC] TGT_PROMPT: Imposing Challenger: choose an enemy unit [Irresistible Faefolk] (agent decides among 1 targets)
[TRC] CHOICE-REQUEST (finalize): target: Imposing Challenger: choose an enemy unit [1 options] (P2)
[TRC] DECISION #74 (P2): target: Imposing Challenger: choose an enemy unit [1 options]
[TRC] CHOSE: [P2] MakeChoice
[TRC] FINALIZE-CHOICES: Imposing Challenger (chain_item=16, ability=1, spell=0)
[TRC] TGT_PICKED: Imposing Challenger: choose an enemy unit = Irresistible Faefolk (id=8)
deck A 9991885925897128524 (Wuju Bladesman - Starter / Yi, Honed)
deck B 7817196308662062806 (Matriarch of War / Ambessa, The Wolf)
game seed 1159599994: chain_runaway: a chain had 12 items finalized without emptying; most are of Irresistible Faefolk (6), Imposing Challenger (6)
winner None, score 0-0, turn 4, 74 decisions
```
- **Reading:** not a card fault as far as I can tell. Challenger: "When I move, you may move an enemy unit
  here with less Might than me to a different battlefield." Faefolk: "When I move to a battlefield, you
  may move an enemy unit to that battlefield." Each "may" answers the other, with one player on each side.
  Two players who always say yes never stop, and the rules leave that to them (Tournament Rules 505). The
  finding is about the engine: it has no notion of a repeated position, and its only answer is the silent
  1,000-pass drop of F1. A search or a trained agent that learns to say yes can sit in it. This is the
  only action the forced-play agent could repeat for free in 1,349,600 games; turns otherwise always
  ended.

### F9 — Star Spring (771): finalize pass ends early

- **Count:** 1 in the large pass=0.2 pass; not seen again in 4.3 million games.
- **Exception:** `GameEngine::runFinalizePass: Star Spring returned from its finalize pass before its
  end-of-choices marker ...`
- **Reproduction** (157 decisions): `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay 12286906783447588048 8510674987487661848 829759374 --agent1 random-player --agent2 random-player --trace --tail 30`
- **Trace:**
```
[TRC] MAY_PROMPT: Blood Rose: pay [1] to gain 1 XP? (P=P2, deferred to agent)
[TRC] CHOICE-REQUEST (finalize): may: Blood Rose: pay [1] to gain 1 XP? [2 options] (P2)
[TRC] DECISION #154 (P2): may: Blood Rose: pay [1] to gain 1 XP? [2 options]
[TRC] CHOSE: [P2] MakeChoice
[TRC] FINALIZE-CHOICES: Blood Rose (chain_item=33, ability=1, spell=0)
[TRC] MAY_ACCEPTED: Blood Rose: pay [1] to gain 1 XP?
[TRC] PAY_ADD: P2 pays [1] + 0 power (any): exhausted 1, recycled 0
[TRC] FINALIZE-CHOICES: Star Spring (chain_item=34, ability=1, spell=0)
[TRC] MAY_PROMPT: Star Spring: move another unit here to its base? (P=P2, deferred to agent)
[TRC] CHOICE-REQUEST (finalize): may: Star Spring: move another unit here to its base? [2 options] (P2)
[TRC] DECISION #155 (P2): may: Star Spring: move another unit here to its base? [2 options]
[TRC] CHOSE: [P2] MakeChoice
[TRC] FINALIZE-CHOICES: Star Spring (chain_item=34, ability=1, spell=0)
[TRC] MAY_DECLINED: Star Spring: move another unit here to its base?
[TRC] FINALIZE-CHOICES: Pridestalker (chain_item=35, ability=1, spell=0)
[TRC] TGT_PROMPT: Pridestalker: give a unit +1 [M] [Annie, Stubborn/Sivir, Mercenary/Rek'Sai, Breacher/Towering Combatant/Ambessa, The Wolf/Blade Twirler] (agent decides among 6 targets)
[TRC] CHOICE-REQUEST (finalize): target: Pridestalker: give a unit +1 [M] [6 options] (P2)
[TRC] DECISION #156 (P2): target: Pridestalker: give a unit +1 [M] [6 options]
[TRC] CHOSE: [P2] MakeChoice
[TRC] FINALIZE-CHOICES: Pridestalker (chain_item=35, ability=1, spell=0)
[TRC] TGT_PICKED: Pridestalker: give a unit +1 [M] = Ambessa, The Wolf (id=78)
[TRC] FINALIZE-CHOICES: Blood Rose (chain_item=36, ability=1, spell=0)
[TRC] MAY_PROMPT: Blood Rose: pay [1] to gain 1 XP? (P=P2, deferred to agent)
[TRC] CHOICE-REQUEST (finalize): may: Blood Rose: pay [1] to gain 1 XP? [2 options] (P2)
[TRC] DECISION #157 (P2): may: Blood Rose: pay [1] to gain 1 XP? [2 options]
[TRC] CHOSE: [P2] MakeChoice
[TRC] FINALIZE-CHOICES: Blood Rose (chain_item=36, ability=1, spell=0)
[TRC] MAY_ACCEPTED: Blood Rose: pay [1] to gain 1 XP?
[TRC] PAY_ADD: P2 pays [1] + 0 power (any): exhausted 1, recycled 0
[TRC] FINALIZE-CHOICES: Star Spring (chain_item=37, ability=1, spell=0)
deck A 12286906783447588048 (Dark Child - Starter / Annie, Stubborn)
deck B 8510674987487661848 (Pridestalker / Rengar, Unseen)
game seed 829759374: engine_error: GameEngine::runFinalizePass: Star Spring returned from its finalize pass before its end-of-choices marker and not because a choice helper had nothing to decide (the 
winner None, score 0-3, turn 10, 157 decisions
```
- **Reading:** `StarSpring::onTrigger` has two returns of its own before the marker
  (`if (!here.has_value() || played == kInvalidId) return;` and the "first here this turn" test), which
  the finalize contract forbids. It reads the played unit from `ctx.state.chain.resuming`; in this game
  three abilities triggered by one play are finalized one after another (Pridestalker, Blood Rose, then
  Star Spring) and the pass ended there. Which of the two returns was taken I did not establish.

### V1 — `DeckValidator` does not enforce [Unique] (CR 825.3.a)

- Forgefire Cape (507), Rabadon's Deathcrown (508) and Shurelya's Requiem (509) print "[Unique] (Your
  deck can have only 1 card with this name.)" and carry `Keyword::Unique`. `checkCopyLimits` accepts three
  copies; it knows `unlimited_copies` only.
- `RandomDeckBuilder` keeps to the rule, so fuzz decks are legal by both (`DeckFuzz.AUniqueCardIsInADeckOnce`).
- Nothing else found between the validator and Tournament Rules 402 / 601 and CR 103: exactly 40 with the
  Chosen Champion, 12 runes of the legend's Domains, 3 battlefields of different names, 3 of a name
  counting the champion, 3 Signature cards in all with the legend's tag, bans, token cards. Not checked by
  the validator and not met in the pool: a token card as legend (there is none).

## 3. What did not fail

- The two leads of `checkpoint-followup-2.md`. `ChainManager::payDeflectForChoices` (spells offered once
  per target) produced no failure in 4.3 million games. The `canAffordWith` lead is F5, in triggered
  abilities, not in spells.
- The 18 deck lists: 388,800 games, one failure (F5).
- No `std::bad_alloc`, no hang, no fatal signal once the chain stop was in.
- Observations that are not failures: the token cards Recruit (954), Sprite (569), Bird (559), Reflection
  (566), Gold (564) and Baron Pit (558) are never the card of any object: their tokens are created by name
  without a card id (Gold 326, Brush 561 and Shadow Clone 955 are). Baron Pit's class has no behaviour, so
  nothing is lost today.

## 4. Agents side by side

`random-player` (`src/agents/random_player_agent.h`) is new: uniform over the legal actions that are not a
pass; it passes with probability P when it could act (`random-player:pass=P`, **default 0.2**) and always
when a pass is all there is. `pass=0` is forced play.

Same 5,000 decks, same game seeds, 100,000 games each, optimised build:

| | random | random-player (pass=0.2) | random-player:pass=0 | pass=0.2 (P1) against random (P2) |
|---|---|---|---|---|
| Failures | 707 | 859 | 528 | 788 |
| of which not F1 / F2 | 0 | 1 | 33 | 2 |
| Distinct findings | F1, F2 | F1, F2, F7 | F1, F2, F4, F5, F6, F7, F8 | F1, F2, F6, F7 |
| Decisions per game: mean / median / p99 / p99.9 / max | 279 / 267 / 481 / 587 / 834 | 272 / 256 / 495 / 617 / 1,005 | 299 / 288 / 527 / 665 / 1,003 | 275 / 262 / 484 / 595 / 874 |
| Turns per game | 16.8 | 17.7 | 15.7 | 17.2 |
| Games per second per thread | 669 | 666 | 646 | 669 |
| Per game: cards put in play / abilities triggered / activated / spells and abilities resolved | 114.3 / 22.7 / 8.1 / 36.7 | 114.6 / 22.9 / 8.7 / 37.7 | 115.0 / 23.3 / 11.6 / 41.0 | 114.2 / 22.7 / 8.4 / 37.1 |
| Least-exercised card, per 100,000 games: 5th percentile / median | 505 / 2,102 | 457 / 2,019 | 571 / 2,204 | 493 / 2,048 |
| P1 wins / P2 wins | 49,882 / 49,411 | 49,324 / 49,817 | 49,809 / 49,663 | 40,699 / 58,513 |

The same on 1,000,000 games (50,000 decks): failures 6,913 / 8,644 / 5,925; not F1 or F2: 7 / 15 / 419;
decisions mean 280 / 273 / 301, max 938 / 1,136 / 1,346; 558 / 539 / 509 games per second per thread.

What it adds, plainly:

- **Coverage barely moves.** Random play already puts 114 cards into play per game; forced play activates
  43% more abilities (11.6 against 8.1 per game) and resolves 12% more.
- **Failures do.** F4 and F8 exist only with pass=0 (an agent that never stops), F5 and F6 are three to
  five times as frequent with it, F9 was seen once with pass=0.2. Seven of the nine findings need more
  than plain random play, or far more games of it.
- **Termination.** With pass=0 turns still end: costs run out and units exhaust. 1,349,600 forced-play
  games, no game at the turn cap or the decision cap (20,000); the longest 1,662 decisions (deck lists).
  The two things that do not end are F4 and F8, and they are recorded as findings.
- **pass=0.2 loses to random** (40.7% against 58.5% as P1). Acting for its own sake is not good play; this
  agent is a test instrument, not a baseline to train against.
- **One blind spot of pass=0**, by construction: a pass is told from the Intent's fields, and a two-option
  value prompt that is not a yes/no (two modes, an X of 0 or 1, "cost [1] less / [A] less", a Hide cost's
  "pay [A] / pay Energy") looks like one. Forced play never takes option 0 of those. It is why every pass
  should include a seat that is not pass=0. The exact list of what counts as a pass is at `isPassOption`.

## 5. Coverage

From engine events and the chain (`tools/fuzz/fuzz_game.h`), every game, no sampling: *in a deck* (legend,
champion, main deck, runes, battlefields); *in play* (a `CardPlayedEvent`, an object entering the board, a
token created with the card, a rune channeled, the game's legends and battlefields); *triggered* /
*activated* (an ability of the card finalized on the chain); *resolved* (an ability of it, or the spell,
reached `ChainItemResolvedEvent`). A keyword's ability ([Vision], [Weaponmaster]) is not counted for the
card.

Large random pass, 1,000,000 games (the two random-player passes give the same table):

| | Cards | Of |
|---|---|---|
| Registered | 955 | |
| In a deck of at least one game | 934 | 955 |
| Played or put into play | 937 | 955 |
| Has a triggered ability: it triggered | 436 | 444 |
| Has an activated ability: it was activated | 131 | 132 |
| Spell: it resolved | 230 | 233 |
| Has an ability or is a spell: something of it resolved | 748 | 760 |

Every card that can legally be in a deck was in play, and every ability and spell among them triggered,
was activated and resolved. The cards with a zero, all for the obvious reason:

| Never in a deck, never in play (18) | Why |
|---|---|
| Ekko, Recurrent (110); Fight or Flight (168); Stealthy Pursuer (177); Scrapheap (182); Stacked Deck (183); Aspirant's Climb (271); Obelisk of Power (279); Reaver's Row (280); The Arena's Greatest (285); The Dreaming Tree (287); Draven, Vanquisher (343); Called Shot (443) | banned: 12 cards. Their code never runs in this fuzz. They hold the 8 triggered abilities, the 1 activated ability and the 3 spells missing above. |
| Baron Pit (558); Bird (559); Gold (564); Reflection (566); Sprite (569); Recruit (954) | token cards whose tokens are created by name, without the card (section 3) |

Token cards that are not in a deck and were in play as tokens: Gold (326), Brush (561), Shadow Clone (955).

Rarely exercised (the smallest of the counts that apply to the card, per 1,000,000 games, random):

| Card | Count | Why |
|---|---|---|
| Daisy! (754) | 2 (in play 8,559) | "When I attack while your units have all 4 tags" (Bird, Cat, Dog, Poro): four tags by chance |
| Solari Shrine (72) | 552 | |
| Renekton, Brute | 679 | |
| Megatusk | 722 | |
| Dominus | 788 (50 with pass=0) | |
| Eclipse Herald | 822 | |

Everything else is over 850. Daisy!'s attack trigger resolved 2, 4 and 3 times in the three large passes:
it has run, but nine times in three million games is not a test. It needs a deck built for it.

The 18 deck lists hold 231 different cards: all 231 in play, 110 of 110 triggered abilities, 26 of 26
activated abilities, 71 of 71 spells resolved, with each agent.

## 6. Coverage-guided decks

`--guide prev.json` weights every pick of the deck builder toward the cards the previous result exercised
least (up to 9 times a well-covered card; `guideWeights` in `tools/deck_fuzz.cpp`). Same seed (20261014),
100,000 games, random agents, guided by the side-by-side random run:

| | Unguided | Guided |
|---|---|---|
| Cards never exercised in a way that applies | 1 (Daisy!) | 0 (Daisy! 3) |
| Cards under 50 | 1 | 1 |
| Cards under 200 | 20 | 10 |
| Least-exercised count: 5th percentile / median | 522 / 2,106 | 655 / 1,406 |
| Abilities triggered per game | 22.7 | 17.2 |
| F1 failures | 651 | 260 |

- The low end rises by a quarter and the list under 200 halves; the middle falls, because rare cards
  crowd out common ones and decks of rare cards do less (fewer triggers per game).
- At 1,000,000 unguided games nothing but Daisy! is under 500, so guiding buys little there. It pays in
  short passes (100,000), and it thins out a known failure that eats budget (F1 fell by 60% because
  Viktor is well covered).
- A guided seed is a deck only together with its guide file; the replay command names it.
- Pairs of cards never seen together: not done. 934 cards make 435,000 pairs, and a pair meets only inside
  a Domain Identity; counting them needs a table per game that I judged not cheap.

## 7. Throughput

| | Games per second per thread | 1,000,000 games on 3 threads |
|---|---|---|
| Optimised, random decks, random, listener on | 669 at 100,000; 558 over the large pass | 598 s |
| Optimised, random-player pass=0.2 / pass=0 | 666 / 646; 539 / 509 over the large pass | 618 s / 654 s |
| Optimised, the 18 lists | 590 / 559 / 548 | |
| Checked build | 141 / 143 / 147 | about 39 minutes |
| Building decks | 50,000 decks in 0.20 s (4 µs a deck) | nothing |

- The coverage listener costs nothing measurable (3,000 games: 14.8 s without, 15.0 s with).
- The large passes are slower per game than the 100,000-game ones on the same machine at the same time of
  day; lane B was building. I did not chase it.
- Random decks play about as fast as the two decks of the gate (about 770 a second by the fix run's
  figure): deck generation costs
  the trainer nothing. **F1 costs it a great deal**: without a stop, 1 game in 150 runs for minutes or takes
  all the memory.

## 8. A stopping rule for the iteration

One pass, about 55 minutes on this machine at 3 threads, fresh `--seed` each time:

| Step | Games | Time |
|---|---|---|
| Optimised, 50,000 seed decks x 20, three times: random, random-player, random-player:pass=0 | 3 x 1,000,000 | 31 min |
| Checked, 5,000 seed decks x 20, the same three | 3 x 100,000 | 12 min |
| Deck lists, 324 pairings x 200, the same three, both builds | 6 x 64,800 | 2 + 6 min |
| Suite, OpenSpiel tests, clone-equivalence, smoke | | 4 min after the build |

Stop when **two consecutive passes** both have:

1. no failure group at all with an **empty** known-failures file (a line is taken out when its fix lands,
   so a fixed cause that returns is new);
2. no game at the turn cap, without a winner, without a reason, hung, or with a chain of 200 items;
3. every card that is not banned and not a token card exercised in at least **K = 200** games of each
   optimised 1,000,000-game run, in every way that applies to it (`deck_fuzz_compare.py --min-resolved 200`).
   Today one card fails this: Daisy! (2 to 4). Either a deck list built for it joins `decks/`, or it is
   named as the one exception.

Why these numbers. The rarest finding of this run came up once in 4.3 million games (F9) and three came
up under 10 times per million (F5's Icevale Archer, F7, F8); 100,000-game passes would have missed five
of nine. K = 200 is under what every card but Daisy! reaches in 1,000,000 games (the next lowest: 552
random, 576 pass=0.2, 50 for Dominus with pass=0, which the rule as written would flag: with pass=0 use
K = 50 or accept Dominus by name). Two passes rather than one because a pass with fresh seeds is a new
sample: a cause at 1 per million is missed by a single 3,000,000-game pass one time in twenty.

Cheaper inner loop while a batch is being fixed: 100,000 games per agent on the optimised build (50 s
each) and 20,000 on the checked build (40 s each) with the known-failures file; `deck_fuzz_compare.py old
new` against the last full pass.

## 9. How to reproduce

```bash
cmake --build build --target deck_fuzz -j3            # and build-check for the checked build
export RIFTBOUND_ROOT=.
# a pass
./build/deck_fuzz --decks 50000 --games-per-pair 20 --seed 20261012 --threads 3 \
    --known docs/testing-known-failures.txt --examples 40 --json big_random.json
./build/deck_fuzz ... --agents random-player            # pass=0.2
./build/deck_fuzz ... --agents random-player:pass=0
./build/deck_fuzz --deck-dir decks --games-per-pair 200 --seed 20261013 --threads 3 --json lists.json
# one game alone; a deck is a seed or a deck list path
./build/deck_fuzz --replay <deckA> <deckB> <game seed> [--agent1 ..] [--agent2 ..] --trace --tail 30
./build/deck_fuzz --replay <deckA> <deckB> <game seed> --show-decks      # the two deck lists
# reports
python3 scripts/deck_fuzz_report.py big_random.json            # totals, groups, the coverage lists
python3 scripts/deck_fuzz_compare.py old.json new.json --min-resolved 200
```

- Seeds of this document: side by side 20261011 (5,000 decks x 20); large 20261012 (50,000 x 20); lists
  20261013; guided 20261014 (guide: the side-by-side random result); checked 20261015 (2,500 x 8) and
  20261016 (5,000 x 20).
- A result file holds, per failure group, the 40 games with the fewest decisions and one per card named;
  its `config.replay_flags` are the flags a replay of that run needs.
- The driver's stops are flags: `--max-chain` (500; 0 turns it off, then set `--memory-limit-gb`),
  `--max-decisions` (20,000), `--max-seconds` (120 per game), `--memory-limit-gb` (4).
- `docs/testing-known-failures.txt` now lists F1 to F9. Its F3 line matches every "off the board" failure
  whatever card wrote the state: take it out first when the fix batch lands.
