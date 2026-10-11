# perf-engine — seventh follow-up report (plans/09 section PP)

## 1. Result

- Branch `fix/perf-engine`, head `528702b`, working tree clean. Verified against `fix/integration` `3696ad8` (merged in).
- 9.PP1, 9.PP2 and 9.PP3 are done and ticked. One more defect was found in the patch notes while doing 9.PP3 and fixed (Hidden Blade, section 4).
- Gates at the head, 0 warnings:
  - `riftbound_tests` 2993 passed, 0 failed (2980 before; 13 new).
  - `riftbound_openspiel_tests` 25 passed; clone-equivalence 10/10.
  - Smoke 200 games seed 20261010: P1 139 / P2 61 / 0 draws, 17 turns, 294 decisions — unchanged from follow-up 6.
  - Search-microbench observation checksum `94739f84565d4d16` — unchanged.
  - MCTS batch rc 0; fault injection rc 1 as wanted.
  - ISMCTS: `sims=50` ×2 games 10/10 clean; `sims=20` ×8 games 10/10 clean.
- Two things about how the gates were run:
  - The clean build was at `61434b3`. The head adds one commit after it (a comment at the code site and three more assertions in test (a), after your last message); the head's binaries are an incremental build on top of that clean one, and the whole gate set above was re-run on them.
  - The checked build (`-DRIFTBOUND_CHECK_BOARD_INDEX=ON`: suite 2993 passed, smoke 139/61/0, no throw) and the trace capture were run at `61434b3`, not again at the head.
- **Fixed-seed traces: only decision-header text changed.** In the six stdout traces, 125 `DECISION #` lines have a new header and 3 `ASK_PROMPT:` lines are new; no other line differs. The HTML replays that embed those lines differ in the same way.

## 2. 9.PP1 — decision headers

**One place derived a header from where the engine was.** `GameEngine::queryAgentForChain` wrote `(P — chain priority): N legal actions` for everything asked through it, and every card prompt and every executor prompt goes through it.

- It now writes "chain priority" only for a decision that offers PassPriority.
- Any other prompt is headed by its own text: `DECISION #N (P1): <text> [k options]`.
- The text comes from `EffectExecutor::asking()`: a card's own prompt label, or the name the executor gives a prompt it asks itself.
- Executor prompts also write an `ASK_PROMPT: <text>` line. The play server titles a decision from the last `*_PROMPT:` trace line, and these prompts had none.

Prompt kinds that went through that path, and their header now:

| Kind | Header text |
|---|---|
| Chain priority | `chain priority` (unchanged) |
| A card's prompt at resolution or finalization | the card's label, e.g. `may: Grand Duelist: exhaust me to …`, `target: Sacrifice: kill a Mighty unit`, `mode: Aurora: where to play unit` |
| A card's prompt with no label | `<card name>: a choice` |
| A choice made before payment as a spell is played | `<card name>: a choice made as it is played` |
| Which dying unit's replacements go first | `which death is replaced first (CR 373)` |
| Which replacement applies to one unit | `<unit> would die — which replacement applies first (CR 372)` |
| Void Hatchling look | `Void Hatchling: look at the top card of your Main Deck` / `… top rune of your Rune Deck` |
| `confirmNow`, `chooseLocationNow` | the caller's label |
| `chooseObjectNow` | `choose one` (it takes no label) |
| Discard | `discard a card` |
| Predict | `Predict: recycle the top card or keep it`, `Predict: which card goes on top` |
| Reveal and choose | `reveal: take a card or leave it` |

Left as they were:
- The other twelve `DECISION #` sites (mulligan, battlefield setup, cost payments, Repeat, main phase) already name their question.
- The HTML replay's decision panel prints `Decision #N [<phase>]`. That is the phase shown as the phase, with each option's own label beneath; I did not change it.
- The `on_decision` callback carries no label of its own.

Tests (`PerfEngineDecisionHeaders`): `WhichDeathIsReplacedFirst`, `WhichReplacementAppliesFirst`, `ACardsPromptCarriesItsOwnText` (Keeper's Verdict's owner prompt; also checks priority decisions keep their name). I did not run these against the old header; each asserts text the old code could not print.

## 3. 9.PP2 — seat-bound sessions

`PlayServerConfig::seat_bound`. `main.cpp` sets it when both seats are human and the new `--hot-seat` switch is not given.

When seat-bound:
- **Taking a seat.** A client sends `{"type":"seat","player":"P1"}`; the page does it when its URL carries `?seat=P1` or `?seat=P2`. The reply is `{"type":"seat","ok":…}`. Refused for a seat that is not human or that another connected client holds. The seat is released when the client disconnects.
- **No seat = spectator.**
- **Frames per seat.** A seat's frame has its own hand, and the options and prompt only when that seat is being asked. A spectator's frame has neither hand and no options. Frames carry `your_seat`.
- **Log lines per seat.** A line goes to a seat when `visibleTo(seat)`, and to spectators when both players may read it.
- **Answers.** A `choose` is accepted only from the client seated as the player it answers for.

When not seat-bound (one human seat, or two with `--hot-seat`): the same code path as before, the same frames, an answer from any client. Decision ids and the `choose` contract from plans/04 item 4.3 are untouched.

Two things you might not expect:
- **God mode still reveals everything.** `main.cpp` turns god mode on by default when a seat is human, and god mode sets `reveal_hidden`. With it on, a seat-bound server sends every client the full board and log, as now; only the seat check on answers applies. Per-seat views need `--god-mode off`.
- **Two humans on one screen now needs `--hot-seat`.** Without it, a page opened with no `?seat=` is a spectator and cannot answer.

Tests (`PlayServerSeats`, in `tests/test_review_fix_io_cli.cpp`, real server and WebSocket clients, a real game with two human seats):
- `EachSeatGetsItsOwnPrivateLinesAndASpectatorNeither` — both seats play until each has received its own draws; every line each client got is one it may read; neither got a line for the other alone; the spectator got public lines only. The Hatchling look is the line `lookBeforeReveal` writes, handed to the server with its audience; I did not arrange a real look inside the live game.
- `FramesAreBuiltPerSeat` — hands, deck order, options, prompt and the text board, for both seats and a spectator on one decision.
- `OnlyTheSeatBeingAskedMayAnswerAndASeatIsHeldByOneClient` — a taken seat and a non-existent seat are refused; the wrong seat, a spectator and a refused client cannot answer; the decision is still pending; the right seat answers it.
- Pins: `Pin_AServerThatIsNotSeatBoundBehavesAsBefore`, `Pin_HotSeatSharesOneStream` (two clients get byte-identical frames, no `your_seat`).
- The twelve existing `PlayServerProtocol` tests pass unchanged. That, and the hot-seat pin, is my evidence for "byte-identical for one human + one AI"; I did not diff frames against the previous build.

**Not exercised in a browser, or at all:**
- The page's JavaScript: sending the seat for `?seat=`, the "connected as P1" label, and how the page looks to a seat that is not being asked (it receives an empty option list).
- `--hot-seat` and the `seat_bound` wiring in `main.cpp` (compiled; never run with two human seats).
- The `ASK_PROMPT:` title reaching the browser.

## 4. 9.PP3 — Zhonya's Hourglass hidden at a battlefield lost without a death

I read the rules as you do on every step. The engine already did (a), (b) and (c); they were untested and are now pinned. One part of (d) was wrong and is fixed.

| Case | Result | Test (`PerfEngineHiddenHourglass`) |
|---|---|---|
| (a) Played from facedown in response, nothing dies | At that battlefield faceup, unused. The cleanup that takes control of the battlefield away recalls it to its controller's base: on the board, faceup, ready, unattached, not in the trash. It then replaces a later death from the base. CR 811.1.d.1.a, 811.6, 149.3, 323.6, 459.1. | `PlayedInResponse_NothingDies_ItIsRecalledToBaseAndStillWorks` |
| (b) Not played | In its owner's trash after that cleanup. CR 323.6.a, 107.3.d. | `NotPlayed_TheFacedownCardGoesToItsOwnersTrash` |
| (c) One cleanup, faceup Hourglass and a different facedown card | Control lost, the gear recalled, the facedown card trashed. | `OneCleanup_ControlLost_FaceupGearRecalled_FacedownCardTrashed` |
| (d) Offered in the Closed State with the enemy spell on the chain | Yes. | asserted in (a) and (b) |
| (d) Refused once control is lost | **Was offered; fixed.** `generateHiddenPlayActions` did not look at who controls the battlefield. | `WithControlLostTheFacedownPlayIsNotOffered` (red before) |

- On (d): in a game there is no such window, because the cleanup that takes control away trashes the card at once. The test builds the state by hand.
- On (c): the engine removes the hidden card before it recalls the gear; CR 323.6 lists the recall first. The two act on different objects, so the order cannot show in the result. The test pins the engine's order and says so.
- The comment at `updateBattlefieldControl` cites CR 323.6.a and 107.3.d; no "under review" wording.
- `rules/patch-notes` and `rules/errata`: nothing separates hidden permanents from hidden spells on loss of control.

**Found in the same FAQ and fixed: Hidden Blade drew for a banished unit.** The Vendetta rules FAQ gives four Hidden Blade examples. One: a unit affected by Smite is chosen by Hidden Blade — "that unit's controller will not draw 2 … the unit was moved to a non-board zone by an effect other than Hidden Blade and thus no longer has a controller that Hidden Blade can reference." The engine drew 2, both before my 9.MM1 change and after it.
- New helper `controllerAfterKill`: the controller now if the unit is still on the board, who had it if it died, nobody if it left another way.
- Used by Hidden Blade 213 and Detonate 328.
- Test `HiddenBlade_ADeathReplacedByABanishDrawsNobodyCards` (red before), which also checks the FAQ's Zhonya's Hourglass example: the saved unit's controller draws 2.

## 5. New findings

1. **Hidden Blade with Repeat on one unit.** The same FAQ passage says that when both executions target the same unit its controller "doesn't draw 4 cards". I did not test this case.
2. **Per-seat views are off by default.** Section 3: god mode is on by default with a human seat and reveals everything to every client, seat-bound or not. Whether two-human play should default to fair play is a product decision, not something I changed.
