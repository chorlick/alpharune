# sweep-verify-cards — twelfth (final) tranche report (9.GG3)

## 1. Result

- Branch `fix/sweep-verify-cards`, head `08a69d3`; **verified against `fix/integration` at `053fa3f`**
  (merged in, no conflict; working tree clean).
- The whole gate set, on my worktree (the same checks as `.worktrees/gate.sh`, at `-j3` / `--threads 3`,
  lane B), after a clean build of all four targets with **0 warnings**:
  - `riftbound_tests`: **2880 passed, 0 failed**
  - `riftbound_openspiel_tests`: **25 passed**
  - clone-equivalence: **10/10**
  - smoke, 200 games fiora vs yi_rebuild, seed 20261010: **rc=0, P1 139 / P2 61 / 0 draws**, avg 17 turns,
    293 decisions
  - MCTS batch (4 games, seed 7) without the fault: **rc=0**
  - MCTS batch with `RIFTBOUND_STEP_FAULT_AT=40`: **rc=1**, "game 1 [seed=7]: RIFTBOUND_STEP_FAULT_AT:
    injected engine fault at decision 40", 0 draws — a failure, not a draw, as wanted.
- `VerifyCardsTest`: 349 tests. Red check for this tranche: 8 tests failed against the 9 unfixed card files.

## 2. Closed — 9.GG3, ticked

- **Elder Dragon 680** — the number of battlefields it covers is fixed on first entry; a battlefield added in
  response is not asked about at resolution. Red first.
- **Imposing Challenger 667** — its list of destinations is stored on the chain item; a battlefield added in
  response no longer makes it ask for a destination at resolution. Red first.
- **Veiled Temple 535** — FIXED: "If it's an Equipment, you may detach it" is a "you may" in a later part of
  the effect, so CR 383.3.a.3 puts it on resolution. The opening may and the gear stay at finalization; the
  detach question is asked as it resolves, and only of an Equipment still attached then. Two tests, red first.
  `C0Test.VeiledTemple_ReadiesAGearAndMayDetachIt` passes unchanged (same three prompts, the third later).
- **Aphelios 372** — the re-check is NOT dead and stays: he picks with two `selectObject` calls, and a stored
  `selectObject` answer is not re-validated by the helper, so the re-check is what skips a rune readied or gone
  since. The comment now says exactly that. (Unlike the five `selectSeveral` cards of 9.EE3.)
- **Trigger conditions** — the three direct-call tests are on the engine path with the same assertions
  (`Vendetta_Barbara_KillsEnemyGearAtSevenRunes`, `Vendetta_Ambessa_AttackKillsWeakerEnemyHereOnlyWhileEmpowered`,
  `FreshBeans_NoOpOutsideShowdown`), and the in-callback copy of the condition is removed from Fresh Beans 573,
  Tomb-Raider Barbara 824, Ambessa 923 and Dame the Despoiler 866: the condition is asked once, when the event
  happens, and the ability still resolves if it stops holding in response (four tests, red first).
  - Mischievous Marai 562 — NOT A DEFECT, nothing removed: what remains in `onTrigger` is "here", which is
    checked on execution (CR 359.3.f.2), not the trigger condition again. Pinned, comment at the code site.
  - One more existing test relied on the in-callback copy and is corrected: the first half of
    `VendettaB.Vendetta_Dame_MatchesBiggestUnitHereThenPlusOne` now goes through the engine (a direct
    `onTrigger` call skips `triggerCondition`).

## 3. Not closed

Nothing of mine. With others, as you assigned: 9.GG1 / 9.GG2 (698, 693, the fixture's Predict predicate, card
919 script matching, `pickEffectPlayLocation`) with sweep-costs; 9.GG4 (Hwei 642) with sweep-triggers; the
rules judgements with the user (Guards! hidden play, Twisted Fate 200, look-then-reveal, Disposal Order,
Curator's exhaust, Emperor's Divide from facedown, whose runes, Sona's guard).

## 4. Needs another stream / for the trainer

No `shared:` commits. Changed in this tranche: nine card files (680, 667, 535, 372 comment, 573, 562 comment,
824, 923, 866), my test file, four existing tests (`test_vendetta_A.cpp`, `_B.cpp`, `_C.cpp`,
`test_missing_card_fixes.cpp`), plans/09.

**Trainer:** Veiled Temple's detach question now comes at resolution; Ambessa, Dame, Barbara and Fresh Beans
resolve even if their condition is lost in response. The smoke split on this tree is 139 / 61 (stage 0
baseline was 128 / 72; it has moved with every card fix and with the new first-option answers recorded in
earlier reports).

## 5. New defects found

- Fresh Beans 573: the red test for "the showdown is asked about only when the unit is played" had to use a state a real game does not reach (the showdown flag cleared between the trigger being queued and its finalization) — the realistic case, the showdown ending in response, already drew on the old code because a paid "may" is not re-validated. Recorded so the test is not mistaken for a reachable scenario.
- Not searched: registry-wide tests that call `onTrigger` on every card directly; if one exists it bypasses `triggerCondition` for the nine cards whose condition now lives only there (573, 824, 923, 866, 73, and the others moved in 9.EE2).

## Stream totals (for the final report)

Twelve tranches. In 03 / 07 / 08 every box of this stream is ticked (7.33 included; 8.8 and 8.16 were never
mine). `tests/cards/test_review_fix_verify_cards.cpp` holds 349 engine-path tests. Three of my own closures
were overturned by later reading of the rules and fixed (Fae Porter 9.A14 → 9.G2; Dark Child "asks nothing";
"only its controller's runes"). Card fixes were checked red against the unfixed code throughout, by file
backup rather than git state.
