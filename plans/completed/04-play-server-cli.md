# 4. Play server, HTML output and CLIs

## Play server

- [x] **4.1 Server threads read and write `GameState` while the engine thread runs** [confirmed race, crash plausible] — FIXED: server threads read/edit GameState only while the engine is parked (human decision not yet answered, spectator gate, or after notifyGameFinished) and hold state_mutex_ throughout; `choose` and the gate exit go through the same mutex; otherwise the last snapshot is re-sent (`engine_busy`) and edits are refused; watcher bookkeeping moved under the mutex and no longer reads `game_over`. `HumanAgent::atDecision()` takes the agent mutex since search's 1.10; test: PlayServerProtocol.EditsAndSnapshotsWaitForTheEngineToPark (no TSAN run)
  - Where: `src/io/play_server.cpp:585-683` (`broadcastDecisionFrame`), `:685-712` (`runWatcher`), `:239-320` (edit handlers); contract in `play_server.h:17-23`.
  - `state_mutex_` is only taken by server threads; the engine never takes it. Nothing checks a human is parked before reading or editing.
  - The watcher broadcasts on the "at-decision to engine-busy" transition (`:705`), iterating `state.objects` while the engine resolves the click. `request_state` and `onAccept` do the same at any time. `edit_*` handlers check only `godModeEnabled()`.
  - Also unsynchronised: `last_p1/p2_decision_id`, `was_at_decision`, `state().game_over` at `:707`, `HumanAgent::at_decision_`.

- [x] **4.2 One malformed WebSocket message kills the IO thread and leaves the process unkillable by Ctrl-C** [confirmed] — FIXED: every client field is read through non-throwing readers, the read handler catches, and the IO thread re-enters `ioc.run()` after an exception so the signal set stays live; test: PlayServerProtocol.MalformedMessagesGetErrorsAndServerKeepsServing
  - Where: `play_server.cpp:200-216`, `:241-312`, `:747-752`. Only `json::parse` is guarded; `m.value("index", -1)`, `m.value("type", "")`, `v.get<int>()` (`:312`) throw `json::type_error`.
  - The exception leaves `ioc.run()`; SIGINT/SIGTERM are still owned by the asio `signal_set` on the dead `io_context` (`:760-774`).

- [x] **4.3 A stale `choose` is applied to a later decision** [confirmed] — FIXED: `choose` must carry `decision_id` and is answered through `HumanAgent::setChosenAction(index, decision_id)`; refused with `err` unless that seat is parked on exactly that decision and the index is in range; an answered decision is recorded so a second click is refused; UI sends the id; test: PlayServerProtocol.StaleOrInvalidChooseIsRefused
  - Where: `play_server.cpp:213-229`, `src/agents/human_agent.h:74-112`. No `decision_id`, `atDecision()` or upper-bound check.
  - `docs/play-api.md:98-100`, `:172-175` say the second click is "never observed".

- [x] **4.4 No Origin check on the WebSocket upgrade, no auth** [confirmed; cross-site WebSocket hijacking class] — FIXED: upgrade only on `/ws`, Origin (when present) must be `http://<Host>`, Host must be an IP literal or `localhost` (DNS rebinding), and a non-loopback `--bind` generates an access token required on `/` and `/ws`; tests: PlayServerProtocol.UpgradeChecksOriginHostAndPath, .AccessTokenIsRequiredWhenConfigured
  - Where: `play_server.cpp:377-386`. Default bind is loopback (`play_server.h:50`), but any page in the user's browser can connect to `ws://127.0.0.1:8080`, read full state, send `choose` / `edit_*`, or trigger 4.2. `--bind 0.0.0.0` exposes it to the LAN.

- [x] **4.5 Opponent hidden information is always sent** [confirmed] — FIXED: frames and `render_text` withhold the opponent's hand and both decks' order unless god mode is on, no seat is human, or `--show-hand` (`PlayServerConfig::reveal_hidden`, `RenderVisibility`). The engine `log` stream names cards (draws, mulligans, hides) with no visibility marker, so while hidden information is withheld the server forwards no `log` message at all (`PlayServer::broadcastLog`); tests: PlayServerProtocol.FairPlayFramesWithholdOpponentHandAndDeckOrder, .RevealHiddenFramesCarryEverything, StateRendererReviewFix.HiddenHandShowsOnlyItsSize
  - Future work (not a leak): a per-event visibility tag on `LogEvent` (`src/core/events.h`, every trace call site) would let the server forward the public lines to a fair-play client instead of none.
  - Where: `state_renderer.cpp:480`, `:318-345`; `play_server.cpp:536-540`; `main.cpp:596`.
  - Both hands are rendered unconditionally; `show_hand` gates only facedown names and is forced on when a seat is human. Every frame carries both `hand`s and ordered `main_deck` / `rune_deck`, independent of `god_mode`.

- [x] **4.6 Spectator mode never updates the board; final frame is racy** [logic confirmed, timing plausible] — FIXED: board updates were ALREADY FIXED by 12c5064 (spectator stepping); remaining races fixed here: main calls notifyGameFinished after joining the engine thread, stop() waits for queued frames to be written instead of sleeping, the watcher no longer re-broadcasts every 100 ms after game over; tests: PlayServerProtocol.SpectatorStepsAdvanceTheBoardOneDecisionAtATime, .TerminalFrameIsDeliveredBeforeTheServerStops
  - Where: `play_server.cpp:691-712`, `main.cpp:699-700`. With both seats AI, `changed` is only true once `game_over` is set. At game end `main` stops the server at once; while `game_over` holds the watcher re-broadcasts every 100 ms (`:707`).

- [x] **4.7 A prompt header can attach to the wrong decision** [confirmed logic] — FIXED: a prompt is attached to every frame of the decision it preceded and retired when that decision is made (accepted `choose`, spectator step, or `notifyDecisionMade` from the engine's on_decision for AI seats); state frames no longer consume it; test: PlayServerProtocol.PromptStaysWithItsDecision
  - Where: `play_server.cpp:795-805` (capture), `:665-671` (attach and clear). `last_prompt` is captured from every `*_PROMPT:` line regardless of player and cleared by whichever frame goes out next.

- [x] **4.8 God-mode moves corrupt ownership and leave orphans** [confirmed] — FIXED: frames carry each object's controller/owner and the UI sends those instead of the battlefield's controller; off-board moves set owner and controller together; moves clear `bf.facedown` and attachments; `Chain`/`FacedownZone` destinations, objects on the chain and unknown `to_zone` are refused; `setPhase` range-checks; tests: StateEditorReviewFix.* (6), PlayServerProtocol.UnknownZoneIsAnErrorNotAMoveToHand. UI change verified by reading only
  - `play_index_html.cpp:644-650` sets `toPlayer` from the battlefield's controller (or `'P1'`); `state_editor.cpp:98` overwrites `obj.controller`.
  - `removeFromZoneVector` (`state_editor.cpp:30-51`) never touches `bf.facedown`, chain items or attachments.
  - `to_zone` of `Chain` / `FacedownZone` sets `obj.zone` with no container.
  - Unknown `to_zone` falls back to `Hand` (`play_server.cpp:88`); `setPhase` casts any integer to `TurnPhase` (`:298-299`).
  - No out-of-bounds indexing found in the editor.

## Rendering

- [x] **4.9 ASCII board silently truncates unit lists at 78 columns** [confirmed] — FIXED: unit/gear lists wrap onto continuation lines and object rows never truncate; action columns size to the longest label; tests: StateRendererReviewFix.CrowdedBattlefieldListsEveryUnit, .LongOptionLabelsAreNotTruncated
  - Where: `state_renderer.cpp:11-13` (`padRight`), used at `:577`, `:616`, `:649`. Three or more units at one location overflow and are dropped from `render_text` and HTML replays.
  - `renderActions` truncates labels to 38 characters (`:938`, `:945`), so distinct options can render identically.
- [x] **4.10** `escapeJs` and the embedded `CARD_IMAGES` JSON do not neutralise `</script>`. Not reachable with today's inputs. — FIXED: `escapeJs` emits `\x3c` for `<`; the CARD_IMAGES JSON emits `\u003c`; tests: HtmlOutputReviewFix.ReplayStringsCannotCloseTheScriptBlock, .CardNamesCannotCloseTheScriptBlock
- [x] **4.11** `ReplayWriter::writeHtml` re-parses `gallery_raw.json` for every replay (`replay_writer.cpp:400`). — FIXED: `loadCardImageMap` parses once per process and returns a const reference; test: HtmlOutputReviewFix.CardImageMapIsLoadedOnce

## `main.cpp`

- [x] **4.12 `--agent1 mcts` (no `:sims=`) is accepted with `sims=0`** [validation gap confirmed, consequence plausible] — `main.cpp:181-200`; `openspiel_match.cpp:101` rejects it correctly. — FIXED: sims > 0 is required for mcts/ismcts with or without `:`; unknown options and non-numeric sims are errors; verified by CLI run (`--agent1 mcts` exits 1)
- [x] **4.13 Failure paths abort or report success** [confirmed] — FIXED: replay dir creation and `server->start()` report an error and exit 1 (start() no longer marks itself running before binding); an engine exception still writes the replay and exits 1; all flag values are validated before anything is created; test: PlayServerProtocol.StartOnABusyPortThrowsAndLeavesTheServerStopped; rest verified by CLI runs
  - `fs::create_directories` and `server->start()` are in no try block (`main.cpp:603`, `:660`; `play_server.cpp:742-743`); a busy port ends in `std::terminate`.
  - On an engine exception the game thread skips `replay->writeHtml()` and `main` returns 0 (`:694-701`).
  - `--god-mode` is validated after the replay directory is created.
- [x] **4.14 Flags accepted but ignored** [confirmed] — FIXED: explicit `--debug`/`--trace` print to stdout in a single game; batch replays go to `<replay-dir>/<timestamp>/replay_gameN.html` (`GameConfig::replay_dir`); bad `--web`/`--render-html`/`--god-mode` values, `--web on` with `--games > 1` and `--games < 1` are errors; test: GameRunnerReviewFix.ReplayGoesToTheConfiguredDirectory; rest verified by CLI runs
  - `--debug` / `--trace` in a single game without web: `on_log` is only connected for replay or web (`main.cpp:613`, `:665`), contradicting the help at `:282-283`.
  - `--replay-dir` ignored in batch: `game_runner.cpp:72` writes `replay_gameN.html` to the working directory, overwriting earlier runs.
  - `--web` / `--render-html` with any value other than `on`/`off` is silently `auto` (`:396-401`).
  - `--web on` with `--games > 1` silently ignored; `--games <= 0` runs one game.

## `openspiel_match.cpp`

- [x] **4.15 The file is in no CMake target** (733 lines of dead code). Decide: wire it in or delete it. — FIXED: deleted. src/openspiel/CMakeLists.txt already records that the match runner "was absorbed into the top-level `riftbound` binary"; it was built by nothing
- [x] **4.16 Defaults and help are stale** [confirmed] — `:284-289`, `:296`, `:359`, `:399`, `:493`. — NOT A DEFECT (obsolete): the file is deleted (4.15), so its stale defaults and its reference to the deleted decks are gone
  - Default decks point at `decks/leblanc_test.txt` (deleted in the working tree); help says "deck JSON".
  - `--registry` defaults to `cards/ban-list.csv`, which no longer exists.
  - A negative `--seed` silently runs nondeterministically; `replays/replay_gameN.html` is overwritten every run.
- [x] **4.17 With `--seed > 0`, every game rebuilds the full card database** [confirmed, not measured] — `:457` calls `LoadGame` per game (`riftbound_game.cpp:52-56`). — NOT A DEFECT (obsolete): the file is deleted (4.15). The same per-agent `LoadGame` cost in mcts_agent.cpp is plan 06

## Batch / game runner

- [x] **4.18** `runner.run()` is posted to the thread pool with no try/catch (`batch_runner.cpp:44-51`); an exception terminates the process. The single-thread path propagates normally. [confirmed] — FIXED: pool tasks catch, later games are skipped, the first exception is rethrown from runBatch, prefixed by GameRunner with the game number and seed; main reports it and exits 1; test: BatchRunnerReviewFix.ExceptionInAWorkerIsRethrownNotFatal
- [x] **4.19** In debug mode every log line is appended to `log_buffer`, which is never printed or read (`game_runner.cpp:57-67`). [confirmed] — FIXED: the per-game buffer is printed under the console mutex when the game ends; test: GameRunnerReviewFix.DebugLogIsPrinted

## Progress bar

- [x] **4.20** No terminal-width handling (line wraps and scrolls each 500 ms tick); fixed 8-space trailing pad (`progress_bar.h:198`); with `--threads > 1` the single `active_decision_` slot flips between games. No crash paths found. — NOT A DEFECT (obsolete): src/io/progress_bar.h was used only by openspiel_match.cpp and is deleted with it (4.15)

## `docs/play-api.md` drift

- [x] **4.21** [confirmed] — FIXED: docs/play-api.md rewritten where it drifted (defaults, flags, frame schema, `choose`/`err`, god-mode gating, connection checks, threading contract, examples on decks that exist)
  - `:297-304` says default is human-vs-random with UI on; actual default is random-vs-random, web off. Spectator example at `:290` omits `--web on`.
  - `:19-20` says "deck JSON"; `main.cpp:445` accepts only `.txt`. `--bind`, `--god-mode`, `--wild` are missing.
  - `:188` says god mode is "enabled iff a seat is human"; `--god-mode on|off` overrides.
  - Frame schema omits `battlefields`, `zones.*.base`, `turn.phase_name`, `p1_score`, `p2_score`, `victory_score`, `winner`, `game_over_reason`.
  - `choose` errors come back as `edit_err`; `:10` points at `play_index_html.h` (page lives in the `.cpp`); `:272-277` describes the threading guarantee that 4.1 shows is not enforced.

## Checked and clean

- No reads past `argv` and no uncaught `stoi` in either CLI.
- Agent RNG seeds derive from game index only; results aggregation uses atomics.
- No render or trace work found in `src/io/` when rendering is disabled.
