# 09 — Defects found during the fix run

Findings that were in no plan when the run started: found by a stream while it
verified or fixed something else. They are required work like every other
finding (run decision "nothing optional"): each box ends FIXED with a test shown
red first, or NOT A DEFECT with evidence, a pinning test and a code-site
clarification. The final gate counts these boxes with the rest.

New findings from later streams are appended here by the orchestrator when it
merges their reports. The stream reports themselves are kept in
[reports/](reports/).

## A. Card and helper defects (sweep-verify-cards, first report, 2026-10-11)

Owner is sweep-verify-cards (second tranche) unless a line names another stream.

- [ ] **9.A1** `units/0191_maddened_marauder.cpp` `onTrigger`: offers a unit at a battlefield whose units can't move to base (`moveToBaseBlocked` not checked).
- [ ] **9.A2** `gear/0695_blast_cone.cpp`, 191: offer an enemy unit with `cant_be_moved_by_enemy`, then the move silently does nothing.
- [ ] **9.A3** `units/0147_wildclaw_shaman.cpp`: the "whose buff to spend" pick goes through `pickTarget`, so it counts as choosing that unit ("when you choose a friendly unit" triggers fire); should be `selectObject`.
- [ ] **9.A4** `Card::costDueNow` (`card.cpp`): true on every re-entry when no finalize pass ran (item played by an effect); a non-idempotent cost followed by a prompt would be paid again. — **owner: sweep-costs**
- [ ] **9.A5** `battlefields/0765_dusk_rose_lab.cpp`: a cost kill that was replaced at finalization is attempted again at resolution.
- [ ] **9.A6** `units/0449_overzealous_fan.cpp`: a replaced "kill me" still moves the attacker.
- [ ] **9.A7** `gear/0021_sun_disc.cpp`: no `triggerCondition`, so every unit played puts an empty ability on the chain when the Disc is not armed.
- [ ] **9.A8** `units/0680_elder_dragon.cpp`: Deflect can be under-paid across locations (each list filtered against the full Power, all commits after the last pick).
- [ ] **9.A9** `spells/0247_icathian_rain.cpp`: a dropped play-time target re-prompts six units instead of five.
- [ ] **9.A10** `spells/0263_bullet_time.cpp`: names the battlefield by picking a unit there (Deflect, "chosen" bookkeeping); X is not capped at the int-choice slots.
- [ ] **9.A11** `units/0855_jayce_brilliant_inventor.cpp`: "ready something" offers only friendly objects.
- [ ] **9.A12** `units/0373_azir_ascendant.cpp`: the once-per-turn stamp is written only if both units are still on the board at resolution.
- [ ] **9.A13** `gear/0412_the_zero_drive.cpp`: "Banish this" cost paid at resolution (ENGINE-GAP comment in the file). — **owner: sweep-verify**
- [ ] **9.A14** `units/0446_fae_porter.cpp`: early `return` when the Porter has left drops an already paid move.
- [ ] **9.A15** `legends/0553_void_burrower.cpp`, `legends/0756_deceiver.cpp`, `units/0491_rek_sai_swarm_queen.cpp`: "may" and cost decided at resolution (no `choicesAtFinalization`); 553 and `units/0121` emit the reveal with `revealed_to_all = false`.
- [ ] **9.A16** `spells/0115_promising_future.cpp` `apply_banish`: picked id not validated against the top 5.
- [x] **9.A17** `units/0381_ornn_blacksmith.cpp`, `units/0613_ivern_nurturer.cpp`: "draw it" pushes the card into the hand by hand (no draw event) — same class as 443 / 626. — **owner: sweep-triggers** — FIXED (sweep-triggers): Ornn 381, Ivern 613, Fate Weaver 626 and Called Shot 443 hand the card to `EffectExecutor::drawHeldCard` (draws_this_turn, `CardsDrawnEvent`, knowledge, private log line unless revealed); test: SweepTriggersTest.RevealAndDrawIt_CountsAsADraw, .CalledShot_DrawingOneCountsAsADraw
- [ ] **9.A18** `gear/0601_soul_sword.cpp` `applyPassiveAura`: no on-board check.
- [ ] **9.A19** `gear/0063_spirit_s_refuge.cpp`: "if they didn't already" depends on hook order.
- [ ] **9.A20** `spells/0760_keeper_s_verdict.cpp`: asks a token's owner top / bottom.
- [ ] **9.A21** `units/0406_jayce_man_of_progress.cpp`: the play permission is not limited to a play from hand.
- [ ] **9.A22** `card_helpers.h` `pickSeveral`: "any number" stops at 12 picks (Albus Ferros). — **owner: sweep-verify**
- [ ] **9.A23** 13 trigger cards build their pick list in `unordered_map` order without sorting (234, 238, 330, 361, 453, 479, 539, 589, 618, 278, 535, 763, 63).
- [ ] **9.A24** `weaponmaster_base.h`: detaches an attached Equipment by editing the vectors instead of `unattachGear`; the "may" is asked at resolution, not finalization. — **owner: sweep-costs**
- [ ] **9.A25** plan correction: plans/cards/batch-08.md line 93 (108 Convergent Mutation) says `giveTemporaryMight` ignores an off-board unit; it does not (the card is safe through the engine's target filter).

## B. Existing tests that cannot fail or assert something false (same report)

Each must be made able to fail (shown red against the defect it claims to cover)
or replaced. Owner: sweep-verify, except where the stream that owns the card
rewrites the test anyway.

- [ ] **9.B1** `C3Test.SquareUp_DoesNotForceADiscard` (c3:440): asserts no prompt with a discardable card in hand; passes only because the Repeat cost is unimplemented — it pins the gap as correct. Must change with 8.9.
- [ ] **9.B2** `C5Test.WeaponmasterUnitsUseTheSharedMay` tail (c5:564-570): "a unit that left the board is not equipped" passes no agent, so the may is declined anyway.
- [ ] **9.B3** `C5Test.RellMagneticPlaysTheEquipmentOutOfHand` second half (c5:820-830): the only check on the Equipment is inside `if (onBoard(shield))`.
- [ ] **9.B4** `C0Test.UnlicensedArmory_SavingTheUnitCostsPowerAndIsOptional` "doomed" block (c0:794-799): no agent, so the unit dies whether or not the may is offered.
- [ ] **9.B5** `C0Test.SunDisc_ReadiesThePlayedUnitAndIsUsedUpByIt` (c0:766): the "played" unit is already ready; every assertion holds if nothing happens.
- [ ] **9.B6** `ReviewFixC2Test.HandBuiltUnitChoicesSkipAnUnchoosableEnemyUnit`, Angle Shot part (c2:1278): no control that Angle Shot prompted at all.
- [ ] **9.B7** `C3Test.RallyTheTroops_DoesNotBuffAnOpponentsUnit` (c3:502): never checks the opponent's unit reached the board.
- [ ] **9.B8** `C4Test.CarnivorousSnapvine_ThatLeftTheBoardFightsNobody`: passes without the guard (enemy killed, damage reset) — superseded by my Snapvine test.
- [ ] **9.B9** `C6Test.ElderDragon_ChoosesUpToOnePerLocation`: nothing dies, so it does not cover 7.36 — superseded by my Elder Dragon test.
- [ ] **9.B10** Negative-only: `C0Test.MaskOfForesight_IgnoresAUnitThatLeftTheBoard`, `ReviewFixC1Test.MightOfDemacia_CountsUnitsAtTheConqueredBattlefield`.
- [ ] **9.B11** `AuditFix5Test.Lillia_NoLocationFallsBackToBase` (direct call) contradicts the c6 "no base fallback" claim.
- [ ] **9.B12** `C7Test.StalkingWolf_...`: the SnoopAgent probe has no `EXPECT_GE(asked, 1)`.
- [ ] **9.B13** `TargetlessTriggerAudit` is a proxy (requirements declared + no opt-in); a card that reads `targets` in `onTrigger` without declaring requirements, or an `onEquippedTrigger`, passes it. The `Plan07Item71` probe has no rows left.

## C. From the cleanup and sweep-flow reports

- [x] **9.C1** Svellsongur 382 "the bearer has each of its abilities twice" is the batch-00 item; tracked there. This line is closed when that box is. — **owner: sweep-triggers** — FIXED (sweep-triggers): see batch-00 382; test: SweepTriggersTest.Svellsongur_*
- [ ] **9.C2** `scripts/generate_replays.py` was rewritten against `build/riftbound` but never run: run it once end to end, or remove it and its README mention. — **owner: sweep-verify**
- [ ] **9.C3** God-mode edit at a nested decision keeps the stale legal list (the reply says `legal_recomputed: false`): make it recompute there too, or show with a test that a stale intent is refused. — **owner: sweep-verify**
- [ ] **9.C4** Blitzcrank 67, Mischievous Marai 562 and Shadow 752 lost a play-anywhere permission that the old text match gave them: confirm against each card's printed text and pin with a test. — **owner: sweep-verify**
- [ ] **9.C5** Resume and finalize bounds now throw `std::logic_error`: confirm no card reaches the bound in normal play (Fox-Fire with many 0-Might units, Overt Operation with more than 12 buffed units, Albus Ferros) with engine-path tests. — **owner: sweep-verify**
- [ ] **9.C6** Compiler warnings: 253 `warning:` lines in the clean gate build at `f3670f3`; the tree must build warning-free under `-Wall -Wextra` in engine targets. — **owner: cleanup (its files), sweep-verify-cards (card files), sweep-verify (`card.h`, engine)**

## D. From the sweep-triggers report (2026-10-11)

- [ ] **9.D1** Astral Heron 831: a countered response hides the first spell (reopened residual; it was in sweep-triggers' work list and its report does not mention it). — **owner: sweep-triggers (follow-up)**
- [ ] **9.D2** Simultaneous deaths outside lethal damage are not a batch: only `processLethalDamage` opens one, so a card that kills several units by calling `killObject` in a loop gives Loyal Poro 718, Lonely Poro 777 and Battle Mistress 517 separate deaths. — **owner: sweep-triggers (follow-up)**
- [ ] **9.D3** Helper caps sized to the old 16-entry resume bound are still there with stale ENGINE-GAP comments: `kSplitDamageMaxPicks` and `kPickSeveralMaxPicks` in `card_helpers.h`, `kMaxMoves` in Corrupted Dragon 878 (same root as 9.A `pickSeveral`). — **owner: sweep-verify**
- [ ] **9.D4** Fixes merged without a test of their own: the `"recycled_main"` announcement for the mulligan and the `recycle_self` cost; reaction plays going through the engine's play path in `stepExecuteAndPass`. — **owner: sweep-triggers (follow-up)**
- [ ] **9.D5** 38 of the 51 tests in `tests/test_review_fix_sweep_triggers.cpp` were written after their fixes and never shown to fail on the unfixed code. Each fix group needs one mutation run (fix disabled, test red) or a stated reason why that cannot be done. — **owner: sweep-triggers (follow-up)**
- [ ] **9.D6** `GameEngine::attachGearToUnit` is a second attach path beside `EffectExecutor::attachGear`: refuses an already-attached gear, no on-board check. — **owner: sweep-costs**
- [ ] **9.D7** Stacked Deck 183 puts the kept card into the hand by `ps.hand.push_back` with no draw event; check the card text for whether it is a draw. — **owner: sweep-triggers (follow-up)**
- [ ] **9.D8** `EnginePathAgent`'s default answers Predict with "recycle" (the first option), so a test that plays a Vision unit without an agent silently recycles the top card. — **owner: cleanup (fixture)**
- [ ] **9.D9** Zed 810 sets `card_def_id = 955` on the Shadow Clone after `createToken` has emitted `TokenCreatedEvent`, so anything keyed on the token's card at play time does not see it. — **owner: sweep-triggers (follow-up)**
- [ ] **9.D10** `ChainManager::stepExecuteAndPass`: an activation or reaction play that adds no chain item re-asks the same player with the same options; only the 1000-iteration bound stops a card that keeps failing. — **owner: sweep-triggers (follow-up)**
- [ ] **9.D11** Card-level trace lines were audited for hidden-card names only in files that touch the deck; every `logTrace` in `src/cards` that can name a card from a hand or facedown zone must carry the right `LogAudience`. — **owner: sweep-triggers (follow-up)**
- [ ] **9.D12** `CLAUDE.md` does not describe the new primitives: recall (`recallToBase`), `LogAudience`, keyword abilities (`keyword_abilities.*`), text-copying Equipment, death batches, `StateSettledEvent`. — **owner: sweep-verify**
- [ ] **9.D13** sweep-costs' `spendBuff(ctx, unit)` in `card_helpers.h` emits `"buff_spent"` without naming the spender; it must call `ctx.executor.spendBuff(unit, ctx.controller)` or Wallop 146 / Call to Glory 207 / Kraken Hunter 150 credit Fae Dragon to the wrong player. — **owner: sweep-costs**

## E. From the perf-search report (2026-10-11)

- [ ] **9.E1** Intermittent ISMCTS game failure: `GameEngine::resumeFromSnapshot` threw "the resumed engine did not reach the snapshotted decision" twice (base `0ca1b19`: `--agent1 ismcts:sims=50 --agent2 random --games 2 --threads 1 --seed 20261010`, fiora vs yi_rebuild, game 1; and `ismcts:sims=20 --games 8 --seed 1`, about once in nine runs). A decision that `isResumableDecision` accepts resumed to a different legal list. Not seen in 18 later runs, so not known to be gone. Needs a stress loop on the merged tree (hundreds of ISMCTS games across seeds) that either reproduces it and fixes the cause, or shows it gone with the loop kept as a test. — **owner: sweep-verify**
- [ ] **9.E2** ISMCTS is not reproducible with a fixed seed: OpenSpiel's `ISMCTSBot` builds its final policy by iterating an absl hash map whose order is randomised per process (read from `is_mcts.cc`, not proven by experiment). Confirm by experiment; then make the agent's choice independent of map order (sort before selecting, in our wrapper) or document it at the agent with a test that pins what IS reproducible. — **owner: sweep-verify**
- [ ] **9.E3** `src/main.cpp` builds its own `CardRegistry` + `CardDB` while search agents use `openspiel::sharedCards()`: a process with a search agent holds two card tables. Use one. — **owner: cleanup**

## F. From the cleanup follow-up report (2026-10-11)

- [ ] **9.F1** Only `playThroughEngine` runs the engine's post-intent cleanup. `fireTriggerThroughEngine` and `resolveThroughEngine` do not, and 61 tests call `testHook_executeIntent` directly with no cleanup, so lethally damaged units stay on the board in those tests. Give the other two helpers the same sequence, and audit the 61 direct calls and the remaining `damage_marked` assertions (route through the helper, or state per test why the direct call is right). — **owner: cleanup (second follow-up)**
- [ ] **9.F2** Engine warnings left: `chain_manager.cpp:186` unused `finalized_something`; `game_engine.cpp:4457` `greedyCascade` set but not used; `trigger_manager.cpp:499` `bf_loc` set but not used. Each may be dead code or a forgotten use: decide which, do not just silence. — **owner: sweep-verify**
- [ ] **9.F3** `src/cards/card.h`: 18 unused-parameter warnings in default virtual bodies (lines 74, 75, 79, 86, 87, 235, 236, 547, 587, 605, 606, 614, 632, 633, 634 at `0227a08`). — **owner: sweep-verify**
- [ ] **9.F4** Two libstdc++ warnings raised from card code, not investigated: `-Wfree-nonheap-object` in a lambda in `DisposalOrder::onResolve` (spells/0665) and an array-bounds `memcpy` warning from a `std::string` `operator+` in `GarbageGrabber::onActivate` (gear). Establish for each whether it is a real defect (undefined behaviour) or a compiler false positive, fix or restructure so the warning is gone. — **owner: sweep-verify-cards**
- [ ] **9.F5** The explanation recorded for the integration build failure of 2026-10-11 (`types.h` redefinitions: a preprocessed-text ccache hit after a comment-only header edit) is the orchestrator's hypothesis and was NOT reproduced by cleanup's experiment; the fix (`f3670f3`, a PCH is never cached) removed the failure. Either reproduce the failure and confirm the cause, or reword `scripts/ccache_pch_safe.sh` and CLAUDE.md to say the cause is unconfirmed. Also measure the cost: with the fixed launcher every unit using the PCH misses the cache after any PCH rebuild, even with identical sources. — **owner: cleanup (second follow-up)**
