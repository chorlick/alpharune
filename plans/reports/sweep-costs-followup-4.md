# sweep-costs follow-up 4 — report

Branch `fix/sweep-costs`, head `415f4b3`, worktree clean. **Verified against integration `49854a2`** ("status: sweep-costs third follow-up verified"), merged in; integration had nothing newer when I committed.

## 1. Result

Clean rebuild of the merged tree, safe launcher, lane A, `-j3`. **Zero warnings.**

| Check | Result |
|---|---|
| `riftbound_tests` | 2799 run, 2799 passed |
| `riftbound_openspiel_tests` | 25 passed |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games fiora vs yi_rebuild, seed 20261010, 3 threads | rc 0, P1 139 / P2 61 / 0 draws |
| MCTS batch (4 games, seed 7) | rc 0 |
| same with `RIFTBOUND_STEP_FAULT_AT=40` | rc 1, failure named, not a draw |

## 2. Items

9.CC1, 9.CC2 and 9.CC3 are ticked. 9.CC4 is left unticked with my part marked done; the other 14 tests are sweep-verify-cards'.

**9.CC1 — an undone attempt is not offered again.**
- The engine remembers each undone attempt with the state it was made in: `PlayerState::undone_play_attempts` holds the card and a `Card::costStateFingerprint`, cleared each turn. `payableThroughAFinalizeChoice` does not offer that play again while the fingerprint is the same.
- The memory lives in `PlayerState`, not on the engine, so a cloned or replayed state offers the same actions.
- Test: `UndoneAttempt_IsNotOfferedAgainUntilTheStateChanges` (not offered at the same decision, checked twice; offered again after a card is drawn).
- **200-game batch:** `UndoneAttempt_NeverRepeatsOverTwoHundredRandomGames`. I did not use fiora / yi_rebuild. The test builds a deck where the case arises: legend Blade Dancer, champion Irelia, Graceful, two more copies of her, and three each of Resonating Strike, Switcheroo, Star-Crossed, Defiant Dance, Last Breath and Flash, with cheap Calm and Chaos units. It is mirrored and played by two random agents, seeds 1–200.
- Result: **10 undone attempts, 0 offered again in the same state.** The agent wrapper detects an undone attempt from outside (same state fingerprint at the player's next decision after choosing a play), not from an engine counter. With the check disabled the same batch shows 13 undone attempts, 8 of them offered again.
- The batch adds about 0.7 s to the suite.

**9.CC2 — an undone attempt emits nothing.**
- New `EventBus::hold / release / discardHeld`. `executePlaySpell` holds the bus from its first line for a finalize-pass spell that is too dear on its own, releases it once the choices make it payable, and drops everything held if the attempt is undone.
- Audit: every subscriber is reached only through the bus — `TriggerManager` (22 signals), the play server's log stream, the replay writer, the trace and debug sinks in `main.cpp` and `game_runner.cpp`. None sees anything from an undone attempt.
- What remains, by design: the decisions themselves (`on_decision` callbacks and `action_history`, so a replay asks the same questions), and the one `INTENT:` trace line that `executeIntent` writes before the play starts. The usual cleanup after an intent still runs.
- Test: `UndoneAttempt_EmitsNoEventAndNoLogLine`. It calls `executePlaySpell` directly (new `testHook_executePlaySpell`) so the `INTENT:` line is outside what it measures, counts log lines and five kinds of event at zero, then shows the same counters do see a play that goes through.
- In the successful case the held events are delivered later than before (after the choices, before the spell goes on the chain). This only happens for a too-dear finalize-pass spell.

**9.CC3 — declarations tied to real pick lists.**
- `FinalizePassSpellDeclarationsMatchTheirPickLists` sets up one varied board for each of the 34 spells, walks every play the engine offers and every sequence of choices the card asks for, collects the friendly units the engine announces as chosen, and requires `mayChooseAtFinalization` to say Yes for exactly those. No walk is cut off (the cap is 40,000 runs per spell and none reaches it).
- **It found one real bug:** Meditation 48 picked the unit for its additional cost with `pickTarget`, so the engine treated the unit as chosen — Deflect, "when you choose a friendly unit" triggers and the Irelia discount all applied to a cost. It now uses `selectObject`. Same class as 9.A3 (Wildclaw Shaman).
- The other 33 declarations agreed with the walk as written.
- Limit: the check is on one board. A declaration that is wrong only in a situation that board does not contain would pass.

**9.CC4, my part.** The scripts in `test_sweep_costs.cpp` now name every prompt; no `kRest` is left in the file. Bard: pay, the battlefield, "None". The two Deflect tests and Rocket Barrage: the rune pick. Zaun Punk: the trigger's own gear pick. Four tests I had given `kRest` turned out to have no trailing prompt at all.

## 3. Red against mutated code

One mutated build; exactly these four tests failed, the other 68 `SweepCostsTest` tests passed.

| Mutation | Test that went red |
|---|---|
| undone-attempt check disabled | `UndoneAttempt_IsNotOfferedAgainUntilTheStateChanges`, `UndoneAttempt_NeverRepeatsOverTwoHundredRandomGames` |
| bus not held | `UndoneAttempt_EmitsNoEventAndNoLogLine` |
| Challenge declared No; Temptation declared Yes | `FinalizePassSpellDeclarationsMatchTheirPickLists` (both named, each way) |

The Meditation fix was seen red before it was made: the walk test reported its four units as "chosen by it but declared No".

## 4. New defects found, not fixed

- The `INTENT:` trace line of an undone attempt is still written. Removing it would mean holding the bus from `executeIntent`, which is outside the cost code.
- A play can still be offered, attempted and undone once per distinct state. It no longer repeats, but each such attempt costs the agent a few decisions for nothing.
- `Card::costStateFingerprint` is the notion of "the state changed" for both the cost contract (9.J2) and this. A change it does not cover (a counter on an object, a continuous effect that alters nothing it digests) would not re-open the offer.
- The pick-list walk uses one board. Cards whose choices depend on things that board lacks (an Empowered unit for Sanction's second mode, for example) are checked only for the modes it can reach.
