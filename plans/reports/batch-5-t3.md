# Batch 5 / T3 — honest ISMCTS: final report

Branch `fix/batch-5-t3`, from master `8fcca18`; master (batch 4b, `c6deb2c`) merged at `e82d5de`. The gate ran on
`e82d5de`; the commit after it adds plans and this report only.

## 1. Result and gate

- **Done.** `IsMctsAgent` no longer reads hidden cards: every simulation runs in a world drawn from what the
  searching player knows (section 3). Perfect-information search stays, labelled (`mcts:sims=N`; `ismcts:...,world=perfect`).
- Clean rebuild (`--target clean`, then `riftbound_tests riftbound riftbound_openspiel_tests
  riftbound_clone_equiv_test deck_fuzz search_strength`), lane B, `-j3`, 393 steps: **0 warnings**. Checked build
  (`-DRIFTBOUND_CHECK_BOARD_INDEX=ON`, `build-checked`, first configured here): 0 warnings.
- **`riftbound_tests`: 3,394 passed** (master after 4b: 3,392; +2 new). `riftbound_openspiel_tests`: **33 passed**
  (was 27; +6 new). `riftbound_clone_equiv_test`: 10/10. Checked build: 3,394 and 33 passed.
- Smoke (fiora vs yi_rebuild, 200 games, seed 20261010, 3 threads): **P1 135 / P2 65 / 0 draws / 17 turns / 282
  decisions — identical to master after batch 4b**. No rule changed; random agents do not search.
- MCTS batch (4 games, seed 7): rc 0, 3-1, without the fault; rc 1 with `RIFTBOUND_STEP_FAULT_AT=40` ("a game
  failed: game 1 [seed=7]: RIFTBOUND_STEP_FAULT_AT: injected engine fault at decision 40"), 0 draws. `MctsAgent`
  is unchanged and plays the same games as before.
- ISMCTS fixed-seed commands, ten runs each, yi_rebuild vs fiora: rc 0 twenty times, **byte-identical output run to
  run**. They are different games from batch 4b's, as expected (the search now runs in sampled worlds, keys its
  nodes differently and plays the most visited option instead of drawing in proportion to visits):
  - `ismcts:sims=50 --agent2 random --games 2 --threads 1 --seed 20261010`: P2 4-8 in 321 decisions, P1 8-1 in 143
    (4b: P2 5-8 in 354, P2 7-8 in 397).
  - `ismcts:sims=20 --games 8 --seed 1`: 4-4 (4b: 2-6).
- Fuzz (optimised, `--decks 600 --games-per-pair 10 --seed 20261901 --known docs/testing-known-failures.txt`):
  6,000 games, 1 failure, known (F5), 0 new groups, 0 without a winner. Random agents do not search; this only
  shows the merged tree is sound.

## 2. What was not done as asked, said plainly

- **The strength table rests on two engines.** The first measurement ran before batch 4b reached master (G1's
  first-turn draw, D9's rune choices change games). When it merged I re-ran the decisive ISMCTS cells on the merged
  tree; both sets are in section 6, labelled. The perfect-information MCTS cells at 50 / 200 / 400 and the
  `world=perfect` cells against random agents are pre-merge only (one MCTS cell and the honest-v-perfect cell were
  re-run and agree).
- **Run-lane use.** My first strength script held `.run.lock` for about an hour. On the coordinator's request I
  stopped it at a matchup boundary (the first 400 v 200 run, just started, was discarded; nothing finished was lost)
  and ran the rest in chunks of at most about 15 minutes, each under its own `flock`.
- **The opponent's model is weak by design** (section 4): an unseen hand is random legal cards of the right Domains.
  It is honest; a better prior is the belief head's job (trainer item 5).
- **The setup battlefield choice is not searched** (answered from the agent's RNG; CR 487.5 / 488.5, item G4): the
  second chooser's pick would otherwise see the first's, which a world cannot redraw (L3).
- The core leak test varies the opponent's deck contents and order through deck lists; **variation of the searcher's
  own deck order** cannot be produced in a live game without changing what it draws, so it is tested at the world
  level (two own deck cards swapped at every decision of eight games), not through whole searches.

## 3. How a world is drawn (what changed)

- `src/agents/determinizer.{h,cpp}`: `WorldSampler::sample(world, observer, rng)` rewrites a copy of the true
  position, reading only `GameKnowledgeView` for the observer. **No object moves**: ids stay in their hand, deck,
  facedown and rune-deck slots; only which card an id is changes (`becomeCard`: a fresh instance, no memory, no
  last-known information). The engine's suspended call, chain items and both players' positional tracker facts
  therefore stay valid at any kind of decision, mid-chain included.
  - Kept: public zones, own hand and facedown cards, every card the observer knows at an exact place (revealed hand
    card, known deck slot, a facedown card it looked at, a card held by its own look, its own set-aside hand cards).
  - Shuffled among their slots: own Main Deck (composition known), known unordered groups, own rune deck.
  - Placed at random: an opponent's card known by identity but not by place (deck-only if confirmed in the deck,
    else any hidden slot; facedown only if it has [Hidden]); the observer's fact follows it.
  - Drawn from the deck model: everything else of the opponent's (hand, deck, facedown, held by their look) and
    their rune deck. Their unchosen battlefields and private `observed_cards` are cleared.
  - Every refill is sorted before it is shuffled, so a world is a function of the observer's knowledge and the RNG,
    not of which id held which card.
- `GameEngine::editHiddenInformation(rewrite, rng_seed)` applies a rewrite as an edit while parked
  (`mutableState`), reseeds the engine RNG (future shuffles and random picks are hidden too) and throws
  `std::logic_error` if the decider's rebuilt options changed. `RiftboundState::editHiddenInformation` wraps it; an
  edited state refuses `syncToHistory` and a clone of it at a non-resumable decision throws instead of silently
  replaying the unedited game.
- `IsMctsAgent` (`agents/mcts_agent.{h,cpp}`): one world per simulation from the search's own RNG. Nodes keyed by
  the **root player's** observation (single-observer ISMCTS). Keyed by the player to move, as before, every opponent
  decision was a new node in every world (their observation names the cards each world drew) and honest ISMCTS-50
  won 6 of 20 against random, i.e. played like random (random Yi v random Fiora: 34% over 400 games); keyed by the
  root player, 21 of 30. The root is keyed by the live options (Intent equality per world). It plays the most visited
  root option (ties by the RNG).
- **For the recorder (coordinator's note 2):** `IsMctsAgent::lastSearch()` returns a `SearchSummary`: `searched`,
  `chosen` (index into the live legal list), `visits` and `value` (mean return of the decider) per live legal index,
  `root_value`, `simulations`. The visit distribution is the policy target; it is reported for every searched
  decision, and `searched == false` marks the ones answered without a search (one option, battlefield setup, no
  root).
- `RIFTBOUND_ISMCTS_CHECK_WORLDS=1` (or `setCheckWorlds(true)`) runs `WorldSampler::check` on every world.
- `MctsAgent` unchanged in behaviour; its class comment, `--help` and CLAUDE.md call it perfect-information MCTS, a
  benchmark rung, never a teacher. `ismcts:sims=N,world=perfect` is the clairvoyant ISMCTS for comparison.
- `RandomDeckBuilder::legendCards(legend)` exposes the pool the deck builder already used (refactor, same decks).
- `tools/search_strength.cpp` (target `search_strength`, not in `all`).

## 4. The deck model and why

The opponent's unseen cards are **a random completion of a legal deck around their legend**: each is drawn
uniformly from the cards a constructed deck with that legend may hold (Domain Identity, Signature cards with its
champion tag, no banned or token card), within the copy limits (3 of a name, [Unique] 1, unlimited exempt, at most
3 Signature cards) counted over everything of theirs the observer has seen; facedown cards from the [Hidden] cards
of that pool; runes as a Domain of the legend's, then a printing. Why: it uses only public facts — the legend and
what has been shown — plus the construction rules, which is what a human at the table knows and what the trainer
plan's open question 1 proposes. "The true deck's unseen multiset, shuffled" would hand the search the decklist and
was rejected. `DeterminizerWorlds.TheOpponentsUnseenCardsComeFromTheirLegendsPoolNotTheirDecklist` checks that most
of a drawn deck is not the true list.

## 5. Tests

New, all green on the merged tree and the checked build:

- `DeterminizerWorlds.EveryWorldAgreesWithWhatItsPlayerKnowsAndIgnoresWhatItCannotSee` — every decision of eight
  games (four deck-list pairings including [Hidden] decks, four random constructed pairings; `random-player`
  choices): **1,925 decisions, 3,850 worlds checked** (both observers) by `WorldSampler::check` (same objects in the
  same places; public, own and known cards unchanged; groups and own deck/runes the same multisets; known cards of
  the opponent present; drawn cards legal; `knowledge::check(world)` empty), and **1,925 leak checks**: a variant
  position the decider cannot tell apart (an opponent hand card swapped with a deck card, a deck card replaced, a
  rune replaced, two own deck cards swapped, a facedown card swapped; 7,567 hidden cards changed) gives the same
  world for the same seed; a control checks the variant really differs.
- `DeterminizerWorlds.TheOpponentsUnseenCardsComeFromTheirLegendsPoolNotTheirDecklist`.
- `IsMctsHonestTest.GamesThatDifferOnlyInCardsTheSearcherCannotSeeGetTheSameSearch` — the core test: yi_rebuild
  searching (both seats) against fiora, against fiora with the same cards in another order (other deck order and
  hand), and against fiora with other cards of the legend's pool; the opponent passes wherever it can. **284
  searches compared** while the histories agree: identical option played, identical root visits per option,
  identical values.
- `IsMctsHonestTest.ThePerfectInformationSearchDoesSeeTheDifference` — the control: `world=perfect` differs.
- `IsMctsHonestTest.EveryWorldOfAGameAgreesWithWhatTheSearcherKnows` — two whole ISMCTS games against
  `random-player`, every world checked; visits sum to the budget, the move played is the most visited.
- `IsMctsAgentTest.ThePerfectWorldVariantIsAlsoAFunctionOfItsSeeds`.
- `RiftboundStateTest.AnEditedStateIsNeverRebuiltByAReplay`, `RiftboundStateTest.AnEditThatChangesTheDecidersOptionsIsRefused`.

Existing tests: none changed. `IsMctsAgentTest.TheSameSeedsPlayTheSameGame` (now in sampled worlds),
`.PlaysAFullGameAsEitherSeat`, `ResumeStress.IsMctsGamesAcrossSeedsNeverFailToResume`, the `MctsAgentTest`s and
`LiveMirrorTest`s pass unchanged; no test pinned the clairvoyant behaviour.

## 6. Strength

`search_strength --a A --b B`: each game draws two different deck lists of `decks/` (39 lists) by its seed, A in seat
1 on even games and seat 2 on odd ones, the engine tosses for the first player; a draw would count half (there were
none, nor any engine error, in 5,400 games). Win rate of A with Wilson 95% interval; seconds per decision over
decisions with more than one option; games per hour = games / wall time on 3 threads.

**Honest ISMCTS against fixed opponents**

| A | B | engine | games | A wins | 95% CI | s / decision (A) |
|---|---|---|---|---|---|---|
| ismcts:50 | random | pre-4b | 200 | 0.730 | 0.665-0.787 | 0.0154 |
| ismcts:50 | random | merged | 200 | 0.745 | 0.680-0.800 | 0.0156 |
| ismcts:100 | random | pre-4b | 200 | 0.760 | 0.696-0.814 | 0.0330 |
| ismcts:100 | random | merged | 200 | 0.785 | 0.723-0.836 | 0.0331 |
| ismcts:200 | random | pre-4b | 200 | 0.835 | 0.777-0.880 | 0.0704 |
| ismcts:200 | random | merged | 200 | 0.850 | 0.794-0.893 | 0.0708 |
| ismcts:400 | random | pre-4b | 200 | 0.785 | 0.723-0.836 | 0.1494 |
| ismcts:400 | random | merged | 100 | 0.830 | 0.745-0.891 | 0.1473 |
| ismcts:50 | random-player | pre-4b | 200 | 0.790 | 0.728-0.841 | 0.0154 |
| ismcts:50 | random-player | merged | 200 | 0.785 | 0.723-0.836 | 0.0155 |
| ismcts:100 | random-player | pre-4b | 200 | 0.855 | 0.800-0.897 | 0.0328 |
| ismcts:200 | random-player | pre-4b | 200 | 0.895 | 0.845-0.930 | 0.070 |
| ismcts:200 | random-player | merged | 200 | 0.870 | 0.816-0.910 | 0.0696 |
| ismcts:400 | random-player | pre-4b | 200 | 0.865 | 0.811-0.906 | 0.1492 |

**Honest ISMCTS head to head** (the budget question)

| A | B | engine | games | A wins | 95% CI |
|---|---|---|---|---|---|
| ismcts:100 | ismcts:50 | pre-4b | 200 | 0.505 | 0.436-0.574 |
| ismcts:100 | ismcts:50 | merged | 200 | 0.520 | 0.451-0.588 |
| ismcts:200 | ismcts:100 | pre-4b | 200 | 0.530 | 0.461-0.598 |
| ismcts:200 | ismcts:100 | merged | 200 | 0.570 | 0.501-0.636 |
| ismcts:400 | ismcts:200 | pre-4b | 200 | 0.465 | 0.397-0.534 |
| ismcts:200 | ismcts:50 | merged | 200 | 0.570 | 0.501-0.636 |
| ismcts:400 | ismcts:50 | pre-4b | 200 | 0.605 | 0.536-0.670 |
| ismcts:400 | ismcts:50 | merged | 200 | 0.610 | 0.541-0.675 |

Pooled over both engines: 100 v 50 0.51 (400 games), 200 v 100 0.55 (400, CI 0.50-0.60), 400 v 200 0.47 (200),
400 v 50 0.61 (400, CI 0.56-0.65).

**Perfect-information search, for comparison**

| A | B | engine | games | A wins | 95% CI | s / decision |
|---|---|---|---|---|---|---|
| mcts:50 | random | pre-4b | 100 | 0.770 | 0.678-0.842 | 0.0089 |
| mcts:100 | random | pre-4b / merged | 100 / 100 | 0.870 / 0.830 | 0.790-0.922 / 0.745-0.891 | 0.019 |
| mcts:200 | random | pre-4b | 100 | 0.850 | 0.767-0.907 | 0.0391 |
| mcts:400 | random | pre-4b | 100 | 0.880 | 0.802-0.930 | 0.0820 |
| mcts:50 / 100 / 200 / 400 | random-player | pre-4b | 100 each | 0.880 / 0.870 / 0.860 / 0.890 | | 0.009 / 0.019 / 0.040 / 0.100 |
| ismcts:100,world=perfect | random | pre-4b | 100 | 0.820 | 0.733-0.883 | 0.0280 |
| ismcts:100,world=perfect | random-player | pre-4b | 100 | 0.870 | 0.790-0.922 | 0.0280 |
| ismcts:100,world=perfect | ismcts:100 (honest) | pre-4b / merged | 100 / 100 | 0.490 / 0.500 | 0.394-0.587 / 0.404-0.596 | |

**Cost per budget** (honest ISMCTS, 3 threads, against random so one side searches; a self-play game costs about
twice the search time):

| sims | s / decision | games / hour, 3 threads (v random) | v itself, est. |
|---|---|---|---|
| 50 | 0.0155 | 7,100-7,500 | ~4,000 |
| 100 | 0.033 | 3,600-3,700 | ~1,900 |
| 200 | 0.070 | 1,750-1,780 | ~900 |
| 400 | 0.149 | 890-960 | ~450 |

(Measured 200 v 100: 100 games in 305-310 s, 1,180 games / hour.)

**Reading.** Every budget beats both fixed opponents clearly. Against each other: **100 does not beat 50**; **200 is
the cheapest budget that beats the one below it** (55% pooled against 100, lower bound at 0.50 — "clearly" only
just; 57% against 50); 400 does not beat 200 (47%) and beats 50 by about as much as 200 does. The clairvoyant
variant is no stronger than the honest one at 100 simulations: the search exploits card information little, which
also means honesty costs nothing measurable here. The flat curve is item L1 (section 7).

## 7. What the trainer should use as a teacher

- **Honest ISMCTS at 200 simulations** (`ismcts:sims=200`, default `world=sampled`), with the root visit
  distribution from `IsMctsAgent::lastSearch()` as the policy target and `root_value` alongside. About 0.07 s per
  decision; ~900 self-play games per hour on 3 threads.
- **Never** `mcts:sims=N` or `world=perfect` as a teacher; they are benchmark rungs.
- Caution: the curve is flat — this teacher beats random by ~85% but gains little from more simulations. Before
  generating a large dataset, L1 (a leaf evaluation that sees more than the score) is likely worth more than budget;
  the measurement should be repeated after it with `search_strength`.

## 8. New defects and leads found

- **L1** (agents) Honest ISMCTS gains little from its budget and nothing from clairvoyance: the score-difference leaf
  value is flat for most of a turn. Overlaps G28.
- **L2** (T2) An action slot keys a prompt's object by its card even when the decider cannot see it
  (`cardKey` in `openspiel/action_vocab.cpp`); a prompt offering an opponent's hidden card would leak its identity
  into the action id and InfoSetId. Not seen in a game; the T3 search does not depend on it.
- **L3** (owner, with G4) The battlefield pick is held in a local of `setupBattlefields`; a searched battlefield
  choice would see the first chooser's pick.
- No engine or card rule defect was found; none was changed.
