# sweep-verify-cards — third tranche report

## 1. Result

- Branch `fix/sweep-verify-cards`, head `a26159e` (merged with the current `fix/integration`; working tree clean).
- Clean build (`--target clean`, PCH-safe launcher, lane B, `-j3`) of `riftbound_tests`,
  `riftbound_openspiel_tests`, `riftbound_clone_equiv_test`:
  - **`riftbound_tests`: 2444 passed, 0 failed.**
  - **`riftbound_openspiel_tests`: 20 passed, 0 failed.**
  - **clone-equivalence: 10/10 passed.**
  - Not run: the 200-game smoke, the MCTS batch, fault injection (the `riftbound` binary was not built).
- Warnings in card files: **0** (unchanged from tranche 2). The three-target build log has 113 `warning:` lines
  in total, none in a card file (they come from the targets I had not built before; see the breakdown the
  orchestrator's gate already tracks under 9.C6 / 9.F2 / 9.F3).
- `VerifyCardsTest` now has 124 tests. Red checks this tranche: 7 section G tests and the new source audit
  failed against the 8 unfixed card files; the 3 new base-only tests failed against 704 / 515 / 876 with the
  `units_play_base_only` half of the filter removed (Guards! 475's base-only test was already red in tranche 2).

## 2. Items closed in this tranche

**03 / 07 (2 more boxes, 51 in all for 03 / 07 / 08):**
- 3.15 — FIXED, verified: 272 Back-Alley Bar read after sweep-triggers (subject must still be a unit on the
  board; no trigger on a recall).
- 7.35 — FIXED, verified: 72 Solari Shrine read after sweep-triggers (`last_known.stunned` and
  `killed_by == controller`; may and exhaust at finalization).

**09 section G (5):**
- 9.G2 Fae Porter 446 — FIXED. "The same battlefield" comes from the trigger condition, so it is fixed when
  the condition is met (CR 359.3.f.3 and its Lillia example); the card notes the battlefield on the chain item
  and uses it even when the Porter has moved on, gone to base or left the board. **This overturns my own
  9.A14 NOT A DEFECT closure**: the pin is removed and the 9.A14 line is marked superseded. 4 tests, red first.
- 9.G3 Guards! 475 hidden at a "units can't be played here" battlefield — FIXED **on a rules reading the user
  should confirm**: no token is played. Basis: card text beats the rule (CR 002 / 054.1), CR 811.1.d calls the
  hidden battlefield a restriction, and the Vendetta FAQ's Rockfall Path / Perched Grimwyrm entry says two
  contradictory restrictions mean the unit can't be played at all. The rejected reading is "the hidden
  battlefield is a requirement that is dropped when impossible, token to base". Both readings say the old
  behaviour (token at Rockfall Path) was wrong; switching to the other reading is a one-line change. 1 test, red first.
- 9.G6 Udyr 157 — NOT A DEFECT: "Deal 2 to a unit at a battlefield" / "Stun a unit at a battlefield" refer to
  neither Udyr nor "here"; only the "me" modes are ignored once he has left (CR 359.3.e.12, 359.3.f.1). Pinned
  (`Pin_Udyr_LeftInResponse_DealTwoAndStunStillResolve`), comment at the code site.
- 9.G7 Dusk Rose Lab 765 — FIXED: a "banish it instead" replacement means the unit was not killed
  (CR 370.1.b / 370.1.c.1): no draw, no second attempt. 2 tests red first, 1 guard test.
- 9.G9 Elder Dragon 680 — FIXED: its own filter honours `ignoresDeflect()` as `offerableChoices` does;
  behaviour unchanged today (it does not override it), pinned.

**09 section B (7):** 9.B3 (Rell), 9.B4 (Unlicensed Armory — the test also marked the doomed unit through
`card_counters`, which the card no longer reads, so it was not marked at all), 9.B6 (Angle Shot), 9.B7 (Rally
the Troops), 9.B10 (two positive controls), 9.B11 (Lillia: rewritten on the engine path; the card is right for
every reachable case), 9.B13.
- 9.B13: `TargetlessTriggerAudit.NoCardReadsTheTargetsItsTriggerIsHanded` scans `src/cards` for an
  `onTrigger` / `onEquippedTrigger` that names its vector and uses it other than forwarding to a base class;
  `TheSourceScanFlagsAReaderAndAllowsAForward` proves the scanner can fail. **It was red on three cards with a
  dead `targets[0]` branch — Reaver's Row 280, Morgana 804, Field Musicians 813 — which I removed.**

**Item 4 (skipped red checks):** base-only tests added for Heedless Resurrection 704, Arcane Shift 515 and
Wild Claw 876 and shown red with that half of the filter removed.

## 3. Items not closed, with exactly what is left (all waiting on sweep-costs)

| Box | Left |
|---|---|
| 3.13 | 372 Aphelios |
| 3.21 | 421 Veteran Poro (no `Keyword::Equip`) |
| 7.6 | 481, 367, 208 |
| 7.7 | Weaponmaster Equip cost (331, 828, the base header) |
| 7.23 | re-check of `weaponmaster_base.h`; engine-path test for 828 Riven |
| 7.26 | 331, 544 |
| 7.33 | stage 5 |
| S2 | 324, 414, 421, 448, 407, 435 |
| S3 | 407, 950 (550 verified in code; no two-copy test of its own — it rests on the contract tests like the rest) |
| S4 | 372 |
| S5 | 324, 414, 421, 448, 407, 435, 540, 544, 732, 847 (and 779 re-read with them) |
| 9.B1, 9.B2 | left to sweep-verify after sweep-costs, as told |

The `verify-cards:` sub-bullets under S2–S5 now name only these. 8.4 / 8.7 / 8.8 / 8.9 / 8.16 were not mine to tick.

## 4. Needs another stream

No `shared:` commits. This tranche changed card files 446, 475, 157, 765, 680, 280, 804, 813, my test file,
seven existing test files (c0, c1, c2, c3, c5, `test_audit_fixes_5.cpp`, `test_review_fix_chain_triggers.cpp`)
and plan files.
- User: confirm the 9.G3 reading.
- sweep-verify (9.G1): when `playLocations(only_bf)` learns `blocks_unit_play`, it must return nothing for a
  blocked hidden battlefield, or `Guards_HiddenWhereUnitsCannotBePlayed_PlaysNoSandSoldier` fails.
- sweep-verify (9.G8): `killObject` returning whether the unit died would replace Dusk Rose Lab's read of
  `banish_if_would_die_this_turn` before the kill (needed today only to tell a banished token from a dead one).

## 5. New defects found in this tranche (none fixed)

- `TriggerManager` does not carry a move's destination on the chain item (only `__moved_from_bf` on the object): Fae Porter 446 has to take "the battlefield I moved to" at finalization, so a Porter that leaves it before finalization does nothing.
- `__moved_from_bf` lives on the object, not the chain item: a unit that moves twice before its first "when I move" trigger resolves gives both triggers the second origin (Lillia 644, Harpoon Squad 458, Akali 808 read it).
- `units/0644_lillia_fae_fawn.cpp`: `LocationId loc = BaseLocation{ctx.controller};` is a silent base default when no origin is recorded — unreachable through `TriggerManager`, but it reads like the fallback the plan says is gone.
- `battlefields/0765_dusk_rose_lab.cpp`: a future banish-instead replacement that does not use `banish_if_would_die_this_turn` would be missed for tokens (see 9.G8).
- `tests/cards/test_wave_b_score_move.cpp` `ScoreMoveTest.BackAlleyBar_GivesDepartedUnitTempMight`: direct call (`fireTriggerAs`) with a unit standing in for the battlefield card; no engine-path test shows a real move out of the Bar giving +1, so the recall tests' `temp_might_bonus == 0` assertions have no positive control.
- 550 Soraka: no test with two copies (aura and death replacement).
- `TargetlessTriggerAudit` source scan, known blind spots (stated in the test): macros, base classes outside `src/cards/<dir>/`, a same-named local or lambda parameter, `(void)targets;` counted as a read.
