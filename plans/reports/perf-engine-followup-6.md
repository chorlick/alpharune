# perf-engine — sixth follow-up report (plans/09 section OO)

## 1. Result

- Branch `fix/perf-engine`, head `df475f9`, working tree clean. Verified against `fix/integration` `91c4ca1` (merged in).
- 9.OO1, 9.OO2 and 9.OO3 are fixed and ticked. The rules settled 9.OO1, so nothing was added to `docs/judge-questions-2026-10.md`.
- Clean build (lane A lock, `-j3`, PCH-safe launcher), 0 warnings:
  - `riftbound_tests` 2980 passed, 0 failed (2975 before; 5 new).
  - `riftbound_openspiel_tests` 25 passed; clone-equivalence 10/10.
  - Search-microbench observation checksum `94739f84565d4d16` — unchanged.
  - MCTS batch rc 0; fault injection rc 1 as wanted.
  - Checked build (`-DRIFTBOUND_CHECK_BOARD_INDEX=ON`): suite 2980 passed, smoke 139/61/0, no throw.
  - ISMCTS: `sims=50` ×2 games 10/10 clean; `sims=20` ×8 games 10/10 clean.
- **Smoke: P1 139 / P2 61 / 0 draws, 17 turns — the split is unchanged, but play did change.** Average decisions went from 293 to 294, and 90 of the 200 games differ in their trace from the previous head:
  - 9.OO3 moved them: the Fiora deck runs Elder Dragon, and units its damage kills now die in the card's own step instead of the next cleanup. In 39 games that is the whole difference (a `KILL:` line became `EFFECT: kill`).
  - 9.OO1 moved them: the Yi deck runs Zhonya's Hourglass, and in 9 games its controller is now asked which of two dying units it saves.
  - The other 42 mix the two or diverge after one of them (for example a Lonely Poro that Elder Dragon's damage kills beside another unit now "did not die alone" and draws nothing).
- **Fixed-seed traces against the previous head: one file differs**, the batch replay (seed 5, three games): one `KILL: First Mate (id=65, 4M, dmg=1)` became `EFFECT: kill First Mate (id=65)` — the Elder Dragon case above. The seed-7 one-line difference from follow-up 5 is still there, unchanged. Everything else is byte-identical.

Three of the five new tests were red in a build with the three fixes reverted; the other two are pins. The source-scan test from follow-up 5 was rewritten for the new shape.

## 2. 9.OO1 — which dying unit a single replacement saves

**The rules give the chooser.** CR 373: replacement effects "with the same controller are applied in the order of their controller's choosing", with this example: "Two units controlled by the same player die in the same cleanup. That player also controls Zhonya's Hourglass. They must decide which event to apply Zhonya's Hourglass to first."

- In `beginDeathBatch`, when two or more of one player's dying units each have something that could replace their death, that player is asked which unit goes next. One prompt per choice, options named by object.
- With one such unit, or none, nothing is asked.
- The turn player's units are still settled before the other player's (CR 373.1). The order of several replacements on one unit is still that unit's controller's choice (CR 372), unchanged.
- "Could replace" is the same list `applyDeathReplacements` offers; I factored the list out (`couldReplaceDeath`) rather than copying it.
- The prompt is asked whenever two dying units both have a candidate, including when the candidates are independent and the order cannot matter. That is one decision more than strictly needed in that case.

Tests (`PerfEngineDeathBatch`):
- `ZhonyasHourglass_ItsControllerDecidesWhichDeathItReplaces` — two P1 units and a P2 unit die in one cleanup; P1 names the first, then in a second run the second; the named unit is recalled, the other dies, the Hourglass is killed; P2 is asked nothing. Red before (the first unit in table order was always saved).
- `Pin_OneReplaceableDeathAsksNothing`.

## 3. 9.OO2 — the Main Deck Hatchling look

- The look writes one private trace line naming the card (`RIFT_TRACE_PRIVATE`), and its two options are labelled "leave / recycle the top card of your Main Deck" without naming it — the same form as the Rune Deck look.
- Test: `TheMainDeckHatchlingLookIsNamedToTheLookerOnly`, for both answers. The line naming the card is `visibleTo(P1)` and not `visibleTo(P2)`; no line P2 may read names it. Red before.
- One existing test helper changed: `askedHatchlingLook` in `test_review_fix_verify_cards.cpp` recognised the look as "the only prompt with two unlabelled answers". It now recognises it by its label. No assertion was dropped.

**Can a fair-play human see what they looked at?** From the code (`PlayServer::broadcastLog` in `src/io/play_server.cpp`, the log hookup in `src/main.cpp`); I did not run the UI.

| Seats | Result | Why |
|---|---|---|
| One human, one AI | Yes — the private line reaches the log panel | Trace is switched on whenever a seat is human, and a line is sent when every human seat may read it. |
| Two humans | No — the line is withheld | Every client gets the same stream; a line one human may not read is dropped for all. |

What is missing for two humans is seat-bound sessions: a client declares which seat it is, and log lines and frames are sent per seat. Sessions carry no seat today. I did not build it.

## 4. 9.OO3 — one test for "this damage kills this unit"

- `EffectExecutor::damageKills` is the one predicate: lethal damage, or any damage marked by a player whose damage is always lethal (Elder Dragon). It keeps the once-per-player-per-pass caching from item 6.6.
- Three callers: `GameEngine::processLethalDamage`, `DeathBatch::LethalOnly`, and the new `EffectExecutor::killLethallyDamagedTogether(units)`.
- **The 21 card kill loops are that one call.** The guard and the loop are gone from each card (124 lines removed, 21 added). The 7 kill-everyone batches keep their `DeathBatch`.
- Behaviour that changes with it:
  - A unit that a card's damage kills by the Elder Dragon rule dies in that card's batch: it is beside the others as it dies, once-per-batch watchers see one batch, and replacements are applied with all of them on the board.
  - Alpha Strike 751 ("for each unit this kills") now counts the units that died. Before it counted every kill it attempted, including one that was replaced.

Tests (`PerfEngineDeathBatch`):
- `DamageThatKillsByTheElderDragonRuleKillsInTheCardsBatch` — with an Elder Dragon, 2 damage to a 2 [M] and a 9 [M] enemy unit and to one of the Dragon's own side: both enemy units die in the card's step, each "did not die alone", a surviving watcher sees two deaths, the own-side unit lives. Red before.
- `Pin_TheCleanupAndTheCardStepAgreeOnWhatDamageKills`.
- `EveryCardBatchSaysWhoDies` (source scan): 21 helper calls, 7 kill-everyone batches, no `DeathBatch::LethalOnly` and no lethal test of a card's own after a batch.

## 5. New findings

1. **Prompts the executor asks mid-effect are traced as "chain priority".** The new order prompt shows in the trace as `DECISION #N (P2 — chain priority): 2 legal actions` / `CHOSE: [P2] MakeChoice`. The same header is used for the existing "which replacement first" prompt. The label is wrong for both; the choice itself is right. I did not change it.
2. **Two human seats cannot be shown private lines** — the seat-bound sessions named in section 3. It affects every private line (draws, peeks, looks), not only the Hatchling's.
