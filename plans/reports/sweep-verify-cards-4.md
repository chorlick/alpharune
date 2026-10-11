# sweep-verify-cards — fourth (last) tranche report

## 1. Result

- Branch `fix/sweep-verify-cards`, head `765b0a9` (merged with the current `fix/integration`; working tree clean).
- Clean build (PCH-safe launcher, lane B, `-j3`) of the three test targets:
  **`riftbound_tests` 2527 passed, 0 failed; `riftbound_openspiel_tests` 20 passed; clone-equivalence 10/10.**
  Not run: smoke, MCTS batch, fault injection (`riftbound` not built).
- Card-file warnings: 0. The build log's other `warning:` lines are `card.h` (9.F3) and three engine lines (9.F2); none is in a test of mine.
- `VerifyCardsTest`: 154 tests.
- The uncommitted change you saw after tranche 3 was real work, not a mutation leftover: the braces for two
  `-Wdangling-else` warnings in my own test file, found in the final build log after I had written the report.
  It was built, the suite re-run, and committed as `3bd0e86`.

## 2. Closed in this tranche

**03 / 07 / 08 — every box that is mine is now ticked.** New ticks: 3.13, 3.21, 7.23, 7.26, S2, S3, S4, S5
(7.6 and 7.7 were already ticked by sweep-costs). Still `- [ ]` in those files, none mine: 7.33 (stage 5),
8.8 (sweep-verify-ledger), 8.16 (engine).

**Batch boxes ticked:** batch-00 Equip cost (ten gears: declared cost equals printed cost, offered only when
payable, paid through the engine — a new table test covers all ten, the existing tests drove two);
batch-06 670 Wily Newtfish (no raw `xp +=` left in any card); batch-08 29 / 466 / 334 / 187; batch-09
"Target requirements…", "Can't be chosen… ignored", "Effect-played units always land in base"; batch-10
459 / 585 / 674 / 681 / 878.

**plans/09 ticked:** 9.I4 (real move out of Back-Alley Bar gives +1), 9.I5 (two Sorakas), 9.B1 (Square Up test
as changed by sweep-costs can fail three ways), 9.B2 (vacuous tail removed, replaced by a test with an
accepting agent and a positive control), 9.J6 (Stalking Wolf: the C7 test still pins gate, choice and
kill-before-landing; its probe no longer runs, so a new test executes the engine's own [Ambush] offer to the
victim's battlefield), 9.K1 (Lillia test renamed to what it tests).

**One card fix:** Undying Loyalty 730 offered a controlled battlefield where units can't be played, and any
battlefield under "units only to base" — given the same mask as 704 / 515. Two tests red against the unfixed card.

**Tests added (30)** for cards whose stage tests were missing, direct-call only, or could not fail: Aphelios
372 (re-ask after `makeNewChoicesFor`; buff mode), Veteran Poro 421 pin, Riven 828 ×3 (first engine-path
tests), Weaponmaster off-board ×2 and non-first Equipment over all seven units, ten-gear Equip table,
can't-be-chosen for 511, 539, 200, 459, 674, 681, play location for 37, 194, 730 ×3, Ornn 407 two copies,
Risen Altar in both slots, Bard 540, Atakhan 732 cost pick, Sky Crusier 847 (first engine-path test),
Irelia 462 Power-or-Energy, Stalking Wolf Ambush, Back-Alley Bar, Soraka.

## 3. Not closed

- **batch-05 "462 Irelia, Graceful" — cannot be ticked; needs the engine.** The "[1] or [A]" choice works and
  is now tested for spells whose target is known when the cost is paid. But `GameEngine::executePlaySpell`
  computes the discount from `intent.targets` / the `needsPlayTimeTarget()` target only: a spell that chooses
  in its finalize pass (`kFinalizeResolve` + `pickTarget`, e.g. Resonating Strike 821) is paid for before that
  pass runs, so choosing Irelia with it gives no discount. Fix: make such a spell's choices before
  `payCardCost` and feed them to `targetCostDiscount` / `canAffordChoosing`. Recorded under the box.
- 9.A19 (Spirit's Refuge aura order), 9.A21 (Jayce 406 "from hand"): engine, as reported before.
- 9.I7: the Guards! reading waits for the user.

## 4. Needs another stream

No `shared:` commits. This tranche touched card file 730, my test file, `test_review_fix_c5.cpp` (9.B2 tail),
`test_audit_fixes_5.cpp` (9.K1 rename) and plan files. I did not touch 704, 515, 475, 513, 765, 449, the 13
sorted-list cards, or any keyword list.
- sweep-verify-engine: 462 above; when the play-location filter moves into `playLocations`, the mask in 730
  can go with those in 704 / 515 / 475 (the three Undying Loyalty tests judge where the unit lands, not mode
  numbers).
- sweep-verify-ledger: Yone 544 keyword (first bullet below).

## 5. New defects found (none fixed)

- `units/0544_yone_blademaster.cpp` `def_`: sets `Keyword::Equip` on a Weaponmaster unit whose text mentions [Equip] only in reminder text — the 3.21 defect fixed on 421; the C5 keyword test does not list 544.
- `GameEngine::executePlaySpell`: a "choose me" cost discount (Irelia 462) is not applied to a spell that chooses in its finalize pass (see section 3).
- `game_engine.cpp`, the two `// Ambush:` loops (showdown and Closed State generators): neither checks `PlayerState::units_play_base_only`, so an [Ambush] unit can be played to a battlefield under an opposing Mageseeker Warden; the other unit-play generators check it.
- `units/0878_corrupted_dragon.cpp` `eligible`: offers enemy units that cannot be moved to base (`cant_be_moved_by_enemy`, `moveToBaseBlocked`); `commitChosenObject` pays Deflect and counts the move, then `moveToBase` refuses it.
- `units/0067_blitzcrank_impassive.cpp` (`elsewhere`) and `units/0674_irresistible_faefolk.cpp` (`legal`): offer an enemy unit with `cant_be_moved_by_enemy`; the choice is made (Faefolk pays Deflect) and the move is refused.
- `spells/0334_angle_shot.cpp` `onResolve`: attaches the Equipment by hand instead of `EffectExecutor::attachGear` (no `is_rules_text_inactive` handling, no on-board checks).
- `units/weaponmaster_base.h` `weaponmasterChoices`: an Equipment with no card behind it (`card_def_id == kInvalidId`) is offered and attached for free, where CR 821.1.c.4 says a card with no Equip cost can't be equipped this way; only fixture-built gear has that shape, and several tests rely on it.
- Stale comments: `units/0544_yone_blademaster.cpp` `onTrigger` ("not opted into finalization" — it is, through the base); `units/0540_bard_mercurial.cpp` class header (still says ENGINE-GAP, cost at trigger finalization — it is paid in `payAdditionalPlayCost`).
- Tests that are direct-call only or cannot fail: `EnginePathTest.WeaponmasterDoesNotEquipAUnitThatLeftTheBoard` (`fireTriggerAs`, no accepting agent, in the engine-path suite); `VendettaA.Vendetta_Riven_WeaponmasterEquipsOnPlay` and `…AttackDamagePerAttachedEquipment`; `AuditFix7Test.Ornn_*`; `VendettaB.Vendetta_SkyCrusier_*`; `C7Test.StalkingWolf_…`'s `SnoopAgent` probe never runs and can be deleted; `test_choice_helpers.cpp` comment quotes Sentinel Adept with text it does not print.
- Observation, not confirmed as a defect: with a lethally damaged unit beside Soraka, the engine's lethal-damage pass recalls it correctly, but I saw no `"recalled"` state event from that path and no `UnitDiedEvent` from a later `killObject` of the same unit at base in a throwaway test — worth one look by whoever owns `killUnit` / `killObject` events.
