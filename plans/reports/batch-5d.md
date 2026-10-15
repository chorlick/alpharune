# Batch 5d — what a decision asks (T1, N2, N3), what a player may know (T4), derived facts (T6), id-order ties (N1), M2-M4: final report

Branch `fix/batch-5d`, from master `fcf27a4`. Master merged twice: batch 5c-perf (`ad860c1`) and batch M1
(`1d0e96b`, merge `5f210b2`). **The gate below ran on `1c06fde`**, the tree merged with M1.

## 1. Result and gate

All nine items closed (section 3). Commits: T1 `41bc460`; T4/T6 `a87b991`; N1 `bd9608f`, `725e775`, `820d980`;
M2-M4 `a025626`; docs and perf follow-ups `873c58e`; merge with M1 `5f210b2`; `1c06fde`.

- Clean rebuild (`--target clean`, then `riftbound_tests riftbound riftbound_openspiel_tests
  riftbound_clone_equiv_test deck_fuzz search_strength balance`), lane B, `-j3`, 402 steps: **0 warnings**. Checked
  build (`build-checked`, `-DRIFTBOUND_CHECK_BOARD_INDEX=ON`): 0 warnings.
- **`riftbound_tests`: 3,538 passed** (1 disabled, as on master). `riftbound_openspiel_tests`: **34 passed** (+1).
  `riftbound_clone_equiv_test`: 10/10. Checked build: 3,538 and 34 passed. `ActionIds*` green.
- **Leak tests, long run** (`RIFTBOUND_LEAK_GAMES=300`), descriptions and `ObserverView` fingerprints compared:
  - variants: 300 games, 84,556 decisions, 596,926 hidden cards changed, 77,074 own hands reordered; 0 options
    offering the decider an own card it was not shown; 38,286 prompts, 168 choosing a chain item;
  - relabelled: **299 of 300 pairs compared to the end** (batch 5b: 188), 85,484 decisions, 691,563 log lines; on the
    batch's own tree before the M1 merge 300 of 300; the pair left is Q1;
  - passive opponent: 300 pairs, 38,997 decisions, 184,385 log lines.
- Prompt census, 300 games: 84,651 decisions, 38,356 prompts, 2,055 distinct questions, no Unspecified kind, no
  description with two meanings. ObserverView, 100 games: completeness 29,245 decisions (16 kinds of fact, each told
  apart every time); derived facts 31,032 decisions, 147,593 units, 2,593 combats, 6,246 pools checked.
- Smoke (fiora v yi_rebuild, 200 games, seed 20261010, 3 threads): rc 0, **P1 137 / P2 63 / 0 draws / 17 turns / 288
  decisions**; master `1d0e96b`: 136 / 64 / 0 / 288. One game differs: N1 changes which order cards go down in when
  recycled together, the trash order of units dying together and the order of waiting trigger batches, so some
  games are other games (1 of 200 here).
- MCTS (4 games, seed 7): rc 0, 1-3; with `RIFTBOUND_STEP_FAULT_AT=40` rc 1, "a game failed: game 1 [seed=7]:
  RIFTBOUND_STEP_FAULT_AT: injected engine fault at decision 40", 0 draws.
- ISMCTS fixed seeds, ten runs each: rc 0 twenty times, **one output per command**.
  `ismcts:sims=50 --agent2 random --games 2 --threads 1 --seed 20261010`: P1 8-1 in 202, P1 8-0 in 125.
  `ismcts:sims=20 --games 8 --seed 1`: 4-4. (Other games than master's: the honest worlds now keep the runes the
  searcher knows, and N1.)
- Pairing run, every ordered pairing of the 39 lists x 7 (10,647 games): optimised and checked builds both 0
  failures, P1 5,353 / P2 5,294 on both (the same games).
- Fuzz with `--known docs/testing-known-failures.txt`: optimised 3 x 100,000 (random seed 20261521; random-player
  20261522; random-player:pass=0 20261523): **0 failures**, 0 without a winner, 0 hung (one game with a chain of
  200+ items in the third, listed by the tool, not a failure). Checked 3 x 20,000 (random, random-player,
  random-player:pass=0, seed 20261531): 0 failures.

## 2. What was not done as asked, said plainly

- **Throughput is not unchanged.** Against master `1d0e96b` (1,000 games, 1 thread, fiora v yi_rebuild, three
  alternating runs, load 3.1-3.7): 649.3 v 632.4 games/s (**-2.6%**); instructions +1.86% (perf stat, 10.16G v
  10.35G). T1 alone on identical games (its commit v its parent, five runs of 2,000): +0.73% instructions, -2.9%
  wall in a noisy window. The prompt is carried at every decision (plain fields; the words were already built), and
  the descriptions are built only on request; what costs is the carrier's construction and moves, about 300
  instructions per decision. N1's orderings and other games are the rest. Two rounds of trimming (no-op reorders
  skipped, a small vector for rune facts, sorts only for two or more) got it from -4.4% to -2.6%. Listed as Q2.
- **N1 is not complete**: 1 pair of 300 still diverges on the merged tree, 3 of 300 in games 300-599 (Q1, section 8).
- One command broke the shell rule early on: a throughput loop used `bash -c` (one run, in the first measurement).
  Every later loop is a Python script with plain argument lists.
- The checked fuzz's three runs share one seed (different agents, so different games).
- `StepResult::legal` keeps the generator's order (N3: the description lists options by action id with the index).
- Two pinned tests and two test helpers changed by design (section 6).

## 3. Verdicts

| Item | Verdict | Test (shown red where FIXED) |
|---|---|---|
| T1 | DONE | PromptCensus.*, Batch5dTest.T1_*, RiftboundStateTest.EveryDecisionSaysWhatItAsks..., HiddenOrderLeak.* |
| N2 | FIXED | HiddenOrderLeak.* now compares prompt lines as written |
| N3 | DONE | describePrompt lists options in action-id order with action id and index |
| T4 | DONE | ObserverViewGames.TwoPositions..., Batch5dTest.T4_* (red: rune hook removed) |
| T6 | DONE | ObserverViewGames.EachDerivedFact... |
| N1 | FIXED (tail Q1) | HiddenOrderLeak.TheSameGame... (red: 111 of 300 pairs diverged; now fails above 1 in 50) |
| M2 | FIXED | Batch5dTest.M2_... (red) |
| M3 | FIXED | Batch5dTest.M3_... (red) |
| M4 | FIXED | Batch5dTest.M4_... (red) — not a question: the text says the owner plays it, CR 355.2 gives the player who plays a unit the choice of location |

No judge questions added (M1's are 14-16).

## 4. T1: the description format

`Prompt` (`core/prompt.h`, on `StepResult::prompt`, `GameEngine::pendingPrompt()`, `AgentInterface::decide`,
`RiftboundState::decisionPrompt()`): kind, decider, source object, chain item, ability index / trigger / keyword /
spell, at finalization, the asking code's words, the call site. Holds ids; not for a model.

`describePrompt(state, prompt, legal, observer, db)` → `PromptDescription`: kind; decider and asker relative to the
observer (1 self, 2 other, 0 game); the asking card if visible; ability; chain position; for the decider the words
and options (in action-id order: index into `legal`, action id, role, value, card of each object if visible,
`visibleObjectDescription` of each, named card, chain position of a chain item, label, words); for another observer
only who decides what. `promptKey` = the question without options. Kinds (stable numbering): Unspecified,
BattlefieldSetup, Mulligan, MainPhase, ChainPriority, ShowdownFocus, NextStaged, CombatDamage, CombatDamageNext,
RuneForEnergy, RuneForPower, AddForPayment, CostSplit, Accelerate, Repeat, AdditionalCost, DiscardCost, TriggerOrder,
ReplacementOrder, NewChoices, MoveJoin, May, Mode, XAmount, Target, TargetFirst, TargetSecond, ChooseObject,
SplitTargets, SplitAmount, ChainItem, Location, Discard, LookTop, DeckOrder, Value, NameACard.

Examples (decider's text):
- `You decide (may) for your Power Nexus at battlefield 0, its triggered ability, as it is put on the chain: may: Power Nexus: pay [A][A][A][A] to score` — options `no`, `yes`.
- `You decide ([Add] for a payment) for your Power Nexus ...: activate an [Add] ability to pay` — options `ActivateReactionAbility your Gold at your base ability 1` (x2).
- `You decide (spell or ability on the chain) for your Wind Wall on the chain, its spell, as it is put on the chain: target: ...` — options `the spell of the opponent's Dredge Up on the chain (chain position 0)`, `... Premonition ... (chain position 1)`.
- The other player at the same decision: `The opponent decides (spell or ability on the chain) for the opponent's Wind Wall on the chain, its spell, as it is put on the chain`, no options.

Census counts (300 games): rune for energy 23,271, priority 21,220, main phase 16,828, focus 5,580, rune for power
4,511, move another unit 2,813, target 2,123+, may 1,632, combat damage 1,467, ... chain item 168, name a card 8.
[Add] for a payment is not reached by random games (as in 5b); `Batch5dTest.T1_AnAddForAPayment...` covers it.

## 5. T4: what a player may know, and where it comes from

| Fact (trainer plan section 2) | Who | From the engine |
|---|---|---|
| Own hand | owner | `ObserverView::ownHand` (sorted), `GameState` |
| Board: bases, battlefields, units, gear, runes, legend and champion zones | both | `GameState`; `ObserverView::board` / `battlefields` with derived facts |
| Both trashes and banishments, in full (up to a whole deck) | both | `ObserverView::publicPile` (entry order, never truncated) |
| The chain with its links, finalized or pending, timing, priority / Focus, combat in progress | both | `ObserverView::chain` (source, targets, chosen objects, a counter's chosen item by place, subject), `timing`, `battlefields` |
| Cards revealed then returned to a hidden zone | who saw them | `KnowledgeView::knownHiddenCards`, `ObserverView::knownInHand` |
| Cards the opponent played | both | public zones while there; history below |
| Opponent's hidden cards | owner only | counts (`handSize`, `deckSize`, `unknownCount`) |
| Own deck: composition, known top / bottom | owner | `ownDeckComposition`, `knownDeckTop/Bottom` |
| **Rune Decks: known top / bottom runes, own rune composition** | per observer | **new**: `knownRuneDeckTop/Bottom`, `ownRuneDeckComposition`, `about()` |
| Scores, turn, phase, energy / power, XP, per-turn counters | both | `GameState`; `ObserverView::resources`, `timing` |
| The legal actions, each by its parts | decider | `legal` + action ids; `describePrompt` options |
| What the pending decision asks | decider (others: who decides what) | **new**: `describePrompt` (T1) |
| History | per observer | the per-observer log (`LogAudience`) plus `describePrompt` at each decision; the trainer records it as it drives the game |

`ObserverView::fingerprint()` holds all of it with no id: the completeness test changes 16 kinds of knowable fact at
every decision and every change is told apart; the leak tests show it equal for positions a player cannot tell apart.

## 6. Tests

New: `PromptCensus.EveryDecisionSaysWhatItAsksAndNoTwoMeaningsShareADescription`
(`RIFTBOUND_PROMPT_GAMES`), `ObserverViewGames.TwoPositionsThatDifferInAFactThePlayerMayKnowAreToldApart`,
`ObserverViewGames.EachDerivedFactIsWhatTheEngineUsesForItsRules` (`RIFTBOUND_OBSERVER_GAMES`),
`RiftboundStateTest.EveryDecisionSaysWhatItAsksWithTheActionIdsOnOffer`, `Batch5dTest.T1_*` (3), `T4_*` (3), `M2_`,
`M3_`, `M4_`. Extended: the three `HiddenOrderLeak` tests compare `describePrompt` and `ObserverView::fingerprint`
for both players; the OpenSpiel clone tests compare `describeDecision`.

Changed (old -> new, why):
- `PerfEngineRuneDeck.TheLookIsPrivateAndNothingAboutTheRuneDeckIsRecorded` ->
  `...TheLookIsPrivateAndEachPlayerKnowsWhatItWasShownOfTheRuneDeck`: it pinned the T4 gap; now the revealed rune is
  known to both at the bottom, the looked-at one to the looker only.
- `HiddenOrderLeak` `normalised`: prompt line names compared as written (were sorted, N2); relabelled test counts a
  pair whose games both ended with one winner as finished, and fails above 1 diverging pair in 50; verbose mode
  reports the decision before a divergence.
- `HiddenOrderLeak` `makeVariant` and `DeterminizerWorlds` `indistinguishable`: replace only runes the observer does
  not know (a recycled rune is now known).

## 7. Throughput

Section 2. Master `1d0e96b` 651.2 / 644.6 / 652.1 games/s, batch 5d 634.8 / 637.4 / 625.0; instructions +1.86%; T1
alone +0.73% instructions on identical games. Descriptions and `ObserverView` cost nothing unless called.

## 8. New defects and leads

- **Q1** (N1 tail): cards returned to a hand together enter it in the effect's list order; two Zero Drives
  triggering for one death track the banished unit by trigger order; one of several identical tokens buffed — each
  a card list in id order. 1 in 300 pairs on the merged tree, 3 in games 300-599.
- **Q2** (perf): section 2.
- R11 (Arcane Shift, batch 6) is M4's pattern: `pickValueAs(ctx, owner, ...)` in `card_helpers.h` is the helper.

## 9. Trainer notes

- Key a decision's question by `PromptKind` + asking card + `promptKey`; never by the index in `StepResult::legal`
  (`OptionDescription::index` maps back; `action_id` orders). A chain-item option is named by its chain position.
- Per observer: `ObserverView` for everything a player may know and the derived facts; `fingerprint()` is the
  reference for the trainer's own completeness / leak tests.
- Option descriptions of a card in a Main Deck now carry the slot the decider knows (twin order may differ).
- Checkpoint note: `docs/checkpoint-breaking-2026-10.md` section 9 (no layout change).
