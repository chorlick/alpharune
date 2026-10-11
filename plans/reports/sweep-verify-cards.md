# sweep-verify-cards — final report

(Suite result and warning counts are filled in at the end of the file once the
clean build finishes; see "Result".)

## 2. Items closed (48 boxes ticked in 03 / 07 / 08, plus 2 batch boxes)

Every tick follows a read of each named card file in the current code and a
check that an engine-path test exercises it; where the stage 2 test could not
fail with the defect back, or only took the decline path, a test was added in
`tests/cards/test_review_fix_verify_cards.cpp` (66 tests, suite `VerifyCardsTest`).

- **03-cards.md — 19 ticked:** 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 3.9, 3.10,
  3.11, 3.12, 3.14, 3.16, 3.17, 3.18, 3.19, 3.20, 3.22, 3.24 (all FIXED,
  verified).
- **07-cards-second-pass.md — 26 ticked:** 7.1, 7.2, 7.5, 7.8, 7.9, 7.10, 7.11,
  7.12, 7.13, 7.14, 7.15, 7.16, 7.17, 7.18, 7.19, 7.20, 7.21, 7.22, 7.24, 7.25,
  7.27, 7.28, 7.30, 7.31, 7.34, 7.36 (all FIXED, verified).
- **08-cards-full-review.md — 3 ticked:** S1, S6, S7.
- **batch files — 2 ticked:** batch-08 "671 Blood Rose / 783 Piltover Enforcer /
  274 Fortified Position"; batch-09 "playPayingCost result ignored" (876, 196,
  112, 349).

Card fixes made by this stream (each with a test shown red without it):
- Spectral Matron 226 and Rumble 349 use `pickEffectPlayLocation` /
  `effectPlayLocation` (work item 5). Behaviour change: a controlled
  battlefield that units can't be played to is no longer offered.
- Akshan 431: the on-board check now comes before `unattachGear` (with Akshan
  gone the enemy Equipment was stripped from its bearer and then neither taken
  nor moved).
- 18 card files: unused `targets` parameter (warnings).

## 3. Items not closed (19 boxes), and who is needed

| Box | Why open | Needs |
|---|---|---|
| 3.8 | names only Chem-Baroness 556 (was in flight) | sweep-verify: verify after sweep-flow |
| 3.13 | 372 Aphelios in flight; 162 / 789 / 855 verified | sweep-verify: 372; add a re-entry test (`makeNewChoicesFor`) for 855 Jayce |
| 3.15 | 712, 272 in flight; 143, 60 verified, 143 test added | sweep-verify: 712, 272 |
| 3.21 | 421 in flight; all ten tag defs checked against the data, pinned | sweep-verify: 421 still has no `Keyword::Equip` |
| 7.6 | 481, 367, 208 in flight; 728 verified, engine gate test added | sweep-verify after sweep-costs |
| 7.7 | Weaponmaster cost in flight; 194, 741, 406 verified | sweep-verify: Weaponmaster cost; test 406 with a gear that has a Power cost |
| 7.23 | `weaponmaster_base.h` being rewritten | sweep-verify: re-check; engine-path test for 828 Riven (only `fireTriggerAs` today) |
| 7.26 | 331, 544 in flight; the other eight verified, tests added | sweep-verify: 331, 544 |
| 7.33 | Party Favors | stage 5 (action vocabulary), untouched |
| 7.35 | 72, 878 in flight; the rest verified | sweep-verify: 72, 878; engine-path tests for 895 Forgotten Relic and 756 Deceiver (direct-call tests only) |
| 8.4, 8.7, 8.8, 8.9, 8.16 | wait for the engine streams | not ticked, as instructed |
| S2 | in-flight 272, 712 and the Weaponmaster units | sweep-verify: those; off-board tests still missing for 242, 490, 864, 157, 164 / 543 activation |
| S3 | 232, 407, 550, 950 in flight; the other 35 verified | sweep-verify: those four |
| S4 | 372, 591 in flight; the rest verified | sweep-verify: those two |
| S5 | 11 in-flight cards; the other 51 verified | sweep-verify: 324, 414, 421, 448, 407, 435, 423, 540, 544, 732, 847 |

Each has a `verify-cards:` sub-bullet in the plan saying what was verified and
exactly what is left. batch-09 "Effect-played units always land in base" also
stays open (730 in flight; see Needs).

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

Existing tests that assert something false or cannot fail (not edited —
cleanup owns `tests/`):
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

No `shared:` commits. Nothing outside my files was changed.
- sweep-verify: everything in the "Items not closed" table; the 02 2.20 plan
  line; the 8.7 virtuals.
- sweep-verify (card work I did not take on): switch the remaining local
  location pickers to the shared helper or give `playLocations` the same
  `blocks_unit_play` / `units_play_base_only` filter (704, 515, 513, 475 through
  `playLocations`; Wild Claw 876's own list); 486 Glasc Mixologist always plays
  to base and 491 Rek'Sai forces her battlefield (no choice, CR 355.2.a).
- cleanup: the tests listed above.

## 5. New defects found (not fixed; none is in a plan)

- `units/0191_maddened_marauder.cpp` `onTrigger`: offers a unit at a
  battlefield whose units can't move to base (`moveToBaseBlocked` not checked).
- `gear/0695_blast_cone.cpp`, 191: offer an enemy unit with
  `cant_be_moved_by_enemy`, then the move silently does nothing.
- `units/0147_wildclaw_shaman.cpp`: the "whose buff to spend" pick goes through
  `pickTarget`, so it counts as choosing that unit ("when you choose a friendly
  unit" triggers fire); should be `selectObject`.
- `Card::costDueNow` (`card.cpp`): true on every re-entry when no finalize pass
  ran (item played by an effect); a non-idempotent cost followed by a prompt
  would be paid again.
- `battlefields/0765_dusk_rose_lab.cpp`: a cost kill that was replaced at
  finalization is attempted again at resolution.
- `units/0449_overzealous_fan.cpp`: a replaced "kill me" still moves the attacker.
- `gear/0021_sun_disc.cpp`: no `triggerCondition`, so every unit played puts an
  empty ability on the chain when the Disc is not armed.
- `units/0680_elder_dragon.cpp`: Deflect can be under-paid across locations
  (each list filtered against the full Power, all commits after the last pick).
- `spells/0247_icathian_rain.cpp`: a dropped play-time target re-prompts six
  units instead of five.
- `spells/0263_bullet_time.cpp`: names the battlefield by picking a unit there
  (Deflect, "chosen" bookkeeping); X is not capped at the int-choice slots.
- `units/0855_jayce_brilliant_inventor.cpp`: "ready something" offers only
  friendly objects.
- `units/0373_azir_ascendant.cpp`: the once-per-turn stamp is written only if
  both units are still on the board at resolution.
- `gear/0412_the_zero_drive.cpp`: "Banish this" cost paid at resolution
  (ENGINE-GAP comment in the file).
- `units/0446_fae_porter.cpp`: early `return` when the Porter has left drops an
  already paid move.
- `legends/0553_void_burrower.cpp`, `legends/0756_deceiver.cpp`,
  `units/0491_rek_sai_swarm_queen.cpp`: "may" and cost decided at resolution
  (no `choicesAtFinalization`); 553 and `units/0121` emit the reveal with
  `revealed_to_all = false`.
- `spells/0115_promising_future.cpp` `apply_banish`: picked id not validated
  against the top 5.
- `units/0381_ornn_blacksmith.cpp`, `units/0613_ivern_nurturer.cpp`: "draw it"
  pushes the card into the hand by hand (no draw event) — same class as 443 / 626.
- `gear/0601_soul_sword.cpp` `applyPassiveAura`: no on-board check.
- `gear/0063_spirit_s_refuge.cpp`: "if they didn't already" depends on hook order.
- `spells/0760_keeper_s_verdict.cpp`: asks a token's owner top / bottom.
- `units/0406_jayce_man_of_progress.cpp`: the play permission is not limited
  to a play from hand.
- `card_helpers.h` `pickSeveral`: "any number" stops at 12 picks (Albus Ferros).
- 13 trigger cards build their pick list in `unordered_map` order without
  sorting (234, 238, 330, 361, 453, 479, 539, 589, 618, 278, 535, 763, 63).
- `weaponmaster_base.h`: detaches an attached Equipment by editing the vectors
  instead of `unattachGear`; the "may" is asked at resolution, not finalization.
- plans/cards/batch-08.md line 93 (108 Convergent Mutation) says
  `giveTemporaryMight` ignores an off-board unit; it does not (the card is safe
  through the engine's target filter).
