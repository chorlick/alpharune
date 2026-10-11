# sweep-verify-cards — final report (second tranche included)

## 1. Result

- Branch `fix/sweep-verify-cards`, head `31d69fc` (merged with `fix/integration` at `8f44930`; working tree clean).
- **Full test suite: 2421 passed, 0 failed**, after `cmake --build build --target clean` and a full rebuild
  with the PCH-safe launcher (`ccache_pch_safe.sh`, namespace `sweep-verify-cards`), lane B, `-j3`.
  Only `riftbound_tests` was built and run (no CLI / OpenSpiel change in this stream; `riftbound_openspiel_tests`,
  the smoke and clone-equivalence were NOT run by me).
- New test file `tests/cards/test_review_fix_verify_cards.cpp`: 112 tests (suite `VerifyCardsTest`).
- Warnings (clean build of the test target): card files **25 unused-parameter + 3 libstdc++ → 0**.
  Before: the 25 the orchestrator listed at `0227a08` (unused `targets` in 18 files, unused `unit` in 7 gear
  files) plus the libstdc++ ones in 665, 99 and 212. After: 3 `warning:` lines in the whole build, all engine
  (`chain_manager.cpp` `finalized_something`, `game_engine.cpp` `greedyCascade`, `trigger_manager.cpp` `bf_loc` — 9.F2).
- Red checks: every card fix made by this stream was run against the unfixed card file(s) and its test failed
  there (6 in the first mutation run, Akshan in a second, 29 tests in the tranche 2 run that put all 38 changed
  card files back to HEAD). Pins of NOT A DEFECT closures pass on both, as they should.
- Build notes: moved from lane A to lane B on request; reconfigured once to the safe launcher. `git stash` /
  `git checkout` of files were refused by the permission system, so the red checks used file backups in the
  scratchpad (no git state touched).

## 2. Items closed

**03 / 07 / 08: 49 boxes ticked** (each after reading every named card file in the current code and checking an
engine-path test; missing or unfalsifiable tests were added):
- 03-cards.md — 20: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 3.8, 3.9, 3.10, 3.11, 3.12, 3.14, 3.16, 3.17, 3.18, 3.19,
  3.20, 3.22, 3.24 (FIXED, verified; 3.8 after sweep-flow merged).
- 07-cards-second-pass.md — 26: 7.1, 7.2, 7.5, 7.8, 7.9, 7.10, 7.11, 7.12, 7.13, 7.14, 7.15, 7.16, 7.17, 7.18,
  7.19, 7.20, 7.21, 7.22, 7.24, 7.25, 7.27, 7.28, 7.30, 7.31, 7.34, 7.36.
- 08-cards-full-review.md — 3: S1, S6, S7.
- batch files — 2: batch-08 "671 / 783 / 274"; batch-09 "playPayingCost result ignored".

**09-found-during-fix-run.md: 23 boxes ticked**
- FIXED, each with a test red against the unfixed card: 9.A1 (191), 9.A2 (695, 191), 9.A3 (147), 9.A5 (765),
  9.A6 (449), 9.A7 (21), 9.A8 (680), 9.A9 (247), 9.A10 (263), 9.A11 (855), 9.A12 (373), 9.A15 (553, 756, 491;
  reveal flags 553, 121), 9.A16 (115), 9.A18 (601), 9.A20 (760), 9.A23 (13 cards + 108), 9.A25 (plan line).
- NOT A DEFECT with pin and clarification: 9.A14 (446 Fae Porter, CR 359.3.e.12).
- Tests made able to fail: 9.B5 (Sun Disc), 9.B8 (Snapvine), 9.B9 (Elder Dragon), 9.B12 (Stalking Wolf).
- 9.F4: both libstdc++ warnings were compiler false positives (no undefined behaviour); restructured so they
  are gone (665 reads by index; 99 and 212 build the label with `+=`).

Other card fixes by this stream: Spectral Matron 226 and Rumble 349 on `pickEffectPlayLocation` (work item 5);
Akshan 431 on-board check before the detach; play locations honour `blocks_unit_play` / `units_play_base_only`
in 486, 491, 704, 515, 513, 475, 876.

**Behaviour / prompt changes the trainer should know** (different or additional `MakeChoice` prompts):
263 Bullet Time (battlefield mode first, at finalization, then X ≤ 15; playable with no unit there),
491 Rek'Sai (the "may reveal" at finalization; a location prompt), 486 Glasc (location prompt when a battlefield
is controlled), 553 Void Burrower and 756 Deceiver ("may" and cost at finalization), 147 Wildclaw Shaman (no
"which unit" prompt with a single buffed unit), 855 Jayce (enemy objects offered), 21 Sun Disc (no chain item
when unarmed), 226 / 349 / 704 / 515 / 513 / 475 / 876 (a battlefield that units can't be played to is no longer
an option).

Existing tests edited (each with a one-line reason): `test_spells_misc.cpp` BulletTimeTest ×2,
`test_review_fix_c2.cpp` BulletTime_XCountsReadyRunes…, `test_review_fix_c5.cpp` RekSaiRevealIsAMay…,
`test_review_fix_c3.cpp` KeepersVerdict_TheOwnerChoosesTopOrBottom (victim is now a real card),
plus the four 9.B tests in c0, c4, c6, c7. No test was deleted or weakened.

## 3. Items not closed

| Box | Why open | Needs |
|---|---|---|
| 3.13 | 372 Aphelios (sweep-costs) | sweep-verify: 372 only (855 re-entry test now exists) |
| 3.15 | 272 Back-Alley Bar (sweep-triggers) | sweep-verify: 272 only (712 verified) |
| 3.21 | 421 Veteran Poro (sweep-costs) | sweep-verify: 421 still has no `Keyword::Equip` |
| 7.6 | 481, 367, 208 (sweep-costs) | sweep-verify |
| 7.7 | Weaponmaster Equip cost (sweep-costs) | sweep-verify (406 Power test now exists) |
| 7.23 | `weaponmaster_base.h` (sweep-costs) | sweep-verify: re-check; engine-path test for 828 Riven |
| 7.26 | 331, 544 through the Weaponmaster base | sweep-verify |
| 7.33 | Party Favors | stage 5 |
| 7.35 | 72 Solari Shrine (sweep-triggers) | sweep-verify: 72 only (878 verified; 895, 756 tests exist) |
| 8.4, 8.7, 8.8, 8.9, 8.16 | engine streams | not ticked, as instructed |
| S2 | 272 and the Weaponmaster units | sweep-verify (all listed off-board tests now exist) |
| S3 | 407, 550, 950 | sweep-verify (232 verified) |
| S4 | 372, 591 | sweep-verify |
| S5 | 324, 414, 421, 448, 407, 435, 423, 540, 544, 732, 847 | sweep-verify |
| batch-09 "Effect-played units land in base" | 730 (sweep-costs); shared helper | sweep-verify |
| 9.A19 Spirit's Refuge 63 aura order | cannot be order-independent inside the card | engine: an `AuraEffect` "only if absent" flag skipped by `deflectCostFor`, or a hook after all aura hooks; test `SpiritsRefuge_NextToAllayTheDeflectCostDependsOnHookOrder` shows today's behaviour |
| 9.A21 Jayce 406 "from hand" | `CostModifier` has no play-source field | engine: `from_hand_only` on `CostModifier` + check in `costModifierApplies`, then one line in the card |

Left to others as told (untouched): 9.A4 `Card::costDueNow`, 9.A13 Zero Drive 412, 9.A22 `pickSeveral`,
9.A24 `weaponmaster_base.h`, 9.A17 (already closed by sweep-triggers), 9.B1–B4, B6, B7, B10, B11, B13.
Since I had not re-checked after their merges: 3.13 / 3.15 / 7.35 / S-lists may be tickable now that
sweep-triggers is merged (272, 72, 550, 591, 423, 540, 732, 847 were not re-read by me).

## Retrofit of NOT A DEFECT closures (work item 2)

Pinning test + clarifying comment at the card site, plan line rewritten:
163 Seal of Strength (3.17), 392 Plundering Poro (3.18), reprint tags
119 / 164 / 205 / 35 / 539 / 543 / 546 / 547 / 537 / 548 (3.21, batch-06 × 4),
779 Veteran Poro (3.21), 315 Dark Child (batch-01), 509 Shurelya's Requiem
(carry-over row; noted under its batch-01 entry), 758 Void Assault attacker
designation (batch-03), 368 Not So Fast / 668 Repulse (batch-03), 728 Stalking
Wolf Ambush half (batch-10), 404 Ezreal S3 (batch-10), 940 Matriarch of War
(batch-08), 867 Noxian Demolitionist (7.35).

Engine-site closures:
- 02 item 2.20, Forgotten Monument 523 "turn-gated return": pinned by
  `VerifyCardsTest.Pin_2_20_ForgottenMonument523_NoScoreMeansNoScoreEventAndNoConquerTrigger`
  and clarified in `battlefields/0523_forgotten_monument.cpp`. **Needs
  sweep-verify:** rewrite the 02 line (not my file) and, if wanted, a comment at
  `GameEngine::isScoreGatedByTurn`.
- 08 item 8.7 engine half (`requiresLegion` / `requiresLevel` never read):
  no pin — the virtuals are being deleted by the sweep (code that will no
  longer exist). **Needs sweep-verify:** confirm they are gone when ticking 8.7.
- Crowd Favorite 664: in flight, untouched.

## Tests for fixes merged without one (work item 3)

- Vayne 537 (and 35), Vex 617, Hwei 642: off-board and accept-path tests; the
  three on-board guards were removed in a mutation build and the tests failed.
- Wild Claw 876: `WildClaw_APaymentThatFailsLeavesTheCardBanished`. The failing
  payment cannot arise in a real game: between the affordability check (resume
  point 1) and the payment (resume point 2) the only thing that happens is the
  controller's own "where to play it" prompt — no priority, no trigger
  resolution, no cleanup. The test therefore forces it (the agent empties the
  pool while that prompt is open). Red against the card with the old manual
  banishment erase. Also added the first engine-path happy-path test of Wild Claw.

## Audit of "may" tests (work item 4)

All 477 tests in `test_review_fix_c0..c7.cpp` were read. Cards whose optional
effect was only ever declined (or never offered) through the engine, now with
an accept-path test here: 475 Guards!, 486 Glasc Mixologist, 447 Loyal Pup,
955 Shadow Clone, 513 Arise! (ready up to two), 224 Salvage (kill a gear),
286 The Candlelit Sanctum (recycle one), 671 Blood Rose (pay [1] for XP).
Not done: the per-card accept path of 407 Ornn / 435 Lucian / 331 Sentinel
Adept (Weaponmaster, in flight).

Existing tests that assert something false or cannot fail (now plans/09 section B; in the
second tranche I fixed 9.B5 Sun Disc, 9.B8 Snapvine, 9.B9 Elder Dragon and 9.B12 Stalking Wolf — the
rest are untouched):
- `C3Test.SquareUp_DoesNotForceADiscard` (c3:440): asserts no prompt with a
  discardable card in hand; passes only because the Repeat cost is
  unimplemented — it pins the gap as correct. Must change with 8.9.
- `C5Test.WeaponmasterUnitsUseTheSharedMay` tail (c5:564-570): "a unit that
  left the board is not equipped" passes no agent, so the may is declined anyway.
- `C5Test.RellMagneticPlaysTheEquipmentOutOfHand` second half (c5:820-830): the
  only check on the Equipment is inside `if (onBoard(shield))`.
- `C0Test.UnlicensedArmory_SavingTheUnitCostsPowerAndIsOptional` "doomed" block
  (c0:794-799): no agent, so the unit dies whether or not the may is offered.
- `C0Test.SunDisc_ReadiesThePlayedUnitAndIsUsedUpByIt` (c0:766): the "played"
  unit is already ready; every assertion holds if nothing happens.
- `ReviewFixC2Test.HandBuiltUnitChoicesSkipAnUnchoosableEnemyUnit`, Angle Shot
  part (c2:1278): no control that Angle Shot prompted at all.
- `C3Test.RallyTheTroops_DoesNotBuffAnOpponentsUnit` (c3:502): never checks the
  opponent's unit reached the board.
- `C4Test.CarnivorousSnapvine_ThatLeftTheBoardFightsNobody`: passes without the
  guard (enemy killed, damage reset) — superseded by my Snapvine test.
- `C6Test.ElderDragon_ChoosesUpToOnePerLocation`: nothing dies, so it does not
  cover 7.36 — superseded by my Elder Dragon test.
- Negative-only: `C0Test.MaskOfForesight_IgnoresAUnitThatLeftTheBoard`,
  `ReviewFixC1Test.MightOfDemacia_CountsUnitsAtTheConqueredBattlefield`.
- `AuditFix5Test.Lillia_NoLocationFallsBackToBase` (direct call) contradicts
  the c6 "no base fallback" claim.
- `C7Test.StalkingWolf_...`: the SnoopAgent probe has no `EXPECT_GE(asked, 1)`.
- `TargetlessTriggerAudit` is a proxy (requirements declared + no opt-in); a
  card that reads `targets` in `onTrigger` without declaring requirements, or an
  `onEquippedTrigger`, passes it. The `Plan07Item71` probe has no rows left.

## 4. Needs another stream

No `shared:` commits; nothing outside card files, my test file, the nine existing tests named above and the
plan files was changed.
- sweep-verify: the table in section 3; the 02 item 2.20 plan line (pin and card comment exist); confirm the
  8.7 virtuals are gone when ticking 8.7.
- sweep-verify (shared header): give `playLocations`, `playTokensChoosingLocations` and `tokenPlayDests` /
  `pickTokenDest` in `card_helpers.h` the `blocks_unit_play` / `units_play_base_only` filter, then delete the
  per-card masks in 704, 515, 475 and Arise! 513's private copy `playSandSoldiers`.
- cleanup / sweep-verify: the remaining 9.B tests.

## 5. New defects found in the second tranche (one bullet each, none fixed)

- `card_helpers.h` `playLocations` / `playTokensChoosingLocations` / `tokenPlayDests`: ignore `BattlefieldState::blocks_unit_play` and `PlayerState::units_play_base_only`, so every card using them offers a battlefield units can't be played to (worked around per card in 704, 515, 475, 513).
- `units/0446_fae_porter.cpp`: a Porter still on the board but at a different battlefield at resolution moves the unit to its current battlefield, not the one fixed by the move event (CR 359.3.e.12 may say "ignore").
- `spells/0475_guards.cpp`: a hidden Guards! at a battlefield with `blocks_unit_play` still plays its token there (CR 811.1.d.5 against "can't be played here": ruling not decided).
- `GameEngine` choice handling: a `MakeChoice` answer is not validated against the options offered; cards must re-validate (Promising Future 115 did not until now).
- `GameEngine::payAndQueueAbility` / `executeIntent`: an activation is not re-checked against `canActivateAbilityAt` (an Udyr in the trash can be activated by a hand-built intent; the generator never offers it).
- `units/0157_udyr_wildman.cpp`: modes "deal 2" and "stun" still resolve after Udyr leaves in response (buff already spent) — taken as intended, not pinned; confirm.
- `battlefields/0765_dusk_rose_lab.cpp`: a "banish instead" replacement of the cost kill (Smite) counts as the cost paid.
- `battlefields/0765` (slot 9) and `units/0449` (slot 7) keep a "kill was replaced" flag in `resume_data` by hand; a shared "cost could not be paid" helper would be safer.
- `units/0680_elder_dragon.cpp` no longer calls `offerableChoices`, so `ignoresDeflect()` is not consulted there (it does not override it today).
- plans/cards/batch-08.md "S3 for 63: FIXED" is true only for Deflect that is printed, temporary, from Equipment or from a hook that ran earlier (see 9.A19).
- Test fixture: `GameEngine`'s constructor marks every unit with `card_def_id == kInvalidId` as a token, so `addUnit(p, kInvalidId, ...)` units are tokens in every engine-path test (it bit Keeper's Verdict); and `updateBattlefieldControl` drops control of an empty "controlled" battlefield, so location tests need a unit there.
- `VerifyCardsTest.OptionLists_AreOfferedInAscendingObjectIdOrder` pins the sort but could pass by luck on unsorted code (hash order cannot be forced); it did fail on the unfixed cards.

The first report's section 5 list is now plans/09 section A (statuses above).
