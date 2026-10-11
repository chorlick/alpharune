# Stream report: sweep-verify-ledger (stage 3)

## 1. Result

- Branch `fix/sweep-verify-ledger`; head is the commit that adds this report (`git log -1 fix/sweep-verify-ledger`).
- `fix/integration` merged in before the final run (one conflict, `plans/09`: integration's version kept, own ticks re-applied).
- Final run on a clean build (`cmake --build build --target clean` first), lane A, `-j3`:

  - `riftbound_tests`: **2547 passed, 0 failed** (1 disabled, as on integration).
  - `riftbound_openspiel_tests`: **24 passed, 0 failed**.
  - `riftbound_clone_equiv_test`: 10 / 10.
  - Smoke, 200 games fiora vs yi_rebuild, seed 20261010, 3 threads: exit 0, P1 137 / P2 63 / 0 draws.
  - The clean build log has 163 `warning:` lines over the four targets (none in files this stream wrote; 9.C6 is the box).
  - The run was made at `e5c8b6e`; the commits after it change only `CLAUDE.md`, `plans/` and this report.
  - Then, in the same run, `Keyword::Equip` was put back on Yone 544: `LedgerTest.Defs_AKeywordPrintedOnlyInReminderTextIsNotOnTheCard` and `.Defs_AKeywordTheTextGivesToSomethingElseIsNotOnTheCard` failed; restored, all 13 `LedgerTest` tests pass and the four targets were rebuilt.

## 2. Items closed

| Item | Outcome |
|---|---|
| Carry-over audit | `plans/reports/carry-over-audit.md`: 108 rows audited (45 DONE, 13 DUPLICATE, 2 OPEN, 11 NOTE, 2 USER, 35 MIXED), 41 new lines (now plans/09 9.M1–9.M41), 3 questions for the user. Three read-only helpers took a slice each; I spot-checked their findings against the code (Angle Shot's hand-written attach, `recallMisplacedObjects`, the bare reaction-play branch, the Zilean field names, Ashe's delayed ability, the 12 / 10 caps) and merged four duplicates. The helpers built and ran nothing. |
| 08 **8.8** | FIXED — 73 defs. Readers of every removed keyword checked; no engine file needed a change. |
| 09 **9.N1** | FIXED with 8.8 (Yone 544 `Equip`); general reminder-text check added. |
| 09 **9.C4** | NOT A DEFECT — pinned (two tests) and clarified at the three cards. |
| 09 **9.C2** | ALREADY FIXED — `scripts/generate_replays.py` run end to end twice (6 and 4 replays, exit 0); unchanged. |
| 09 **9.E1** | ALREADY FIXED (not reproduced) — 600 random games with every resumable decision resumed, 300 in-process and 520 CLI ISMCTS games, no failure. Cause not identified. Bounded tests kept. |
| 09 **9.E2** | FIXED — cause confirmed by experiment; `IsMctsAgent` runs an order-independent implementation of the same search. |
| 09 **9.D12** | FIXED — `CLAUDE.md`. |
| 02 item 2.20 plan line | rewritten in the `NOT A DEFECT: ...; pinned by ...; clarified in ...` form. |
| Carry-over "1.3 test not confirmed to fail" | shown red by mutation (`mainPhaseLoop(0)` in `runTurnFromPhase`), test unchanged. |
| `docs/trainer-notes-2026-10.md` | written; Appendix A lists the 210 card files whose prompt surface changed since `c7143f1`, generated from the sources. |

Tests added: `tests/cards/test_review_fix_ledger.cpp` (13, `LedgerTest.*`); in `src/openspiel/openspiel_tests.cpp`: `IsMctsAgentTest.TheSameSeedsPlayTheSameGame`, `MctsAgentTest.TheSameSeedsPlayTheSameGame`, `ResumeStress.EveryResumableDecisionOfAGameResumesToItself`, `ResumeStress.IsMctsGamesAcrossSeedsNeverFailToResume`.

Shown red first: the three registry / list def tests, the Mystic Vortex tax test and the Ambush def assertion (against the unfixed defs); the reminder-text test (with `Keyword::Equip` put back on 544); `IsMctsAgentTest.TheSameSeedsPlayTheSameGame` (with `ISMCTSBot`); the 1.3 test (mutation). Pins that pass before and after by design: the other seven `LedgerTest` tests, the MCTS reproducibility test, the two `ResumeStress` tests.

## 3. Items not closed

None of the items given to this stream is open.

Limits of what was done, stated plainly:

- **9.E1**: the failure was not reproduced, so its cause is unknown and "gone" rests on 1,420 games without it.
- **9.E2**: I did not find out why, before the change, the single-game path (search on the main thread) was repeatable while the batch path was not. The fix does not depend on the answer.
- **8.8**: the new general check sees a keyword printed only in reminder text. A keyword the rules text gives to another object ("Give a unit [Assault 3]") is covered by a list of the 33 instances found, not by a rule; a new card with that mistake would not be caught.
- **Trainer notes, Appendix A** shows which helper calls each card file gained or lost, not the order of prompts at run time.
- `CLAUDE.md`: what I added was checked against the code; I did not re-verify every sentence other streams added after my merge of `fix/integration`.

## 4. Needs another stream

No `shared:` commit. No engine file or shared header was edited (an engine file was mutated twice for red runs and restored; `git diff` on `src/engine` is empty).

**For sweep-verify-engine — two comments (9.M2, 9.M10).** Exact text:

At `GameEngine::isScoreGatedByTurn` (`src/engine/game_engine.cpp`), above the function:

```cpp
// "Players can't score here until their third turn" (Forgotten Monument 523,
// through BattlefieldCard::minTurnToScore). Conquer and Hold are the two ways
// of Scoring (CR 471), so while this returns true there is no Score at all:
// no point, no ScoreEvent, the battlefield is not marked scored and no
// Conquer / Hold ability triggers (CR 473.2.c: "These will only trigger when
// the Battlefield is Scored"). That is deliberately different from
// `cannot_gain_points` ("can't gain points"), where the battlefield IS scored
// and only the point is withheld (CR 470.1). Pinned by
// VerifyCardsTest.Pin_2_20_ForgottenMonument523_NoScoreMeansNoScoreEventAndNoConquerTrigger.
```

At `Card::hasReactionTiming()` (`src/cards/card.h`), replacing the doc comment:

```cpp
    /// Whether this PERMANENT card itself has [Reaction] (CR 813.1.c.1: "This
    /// can be played during Closed States on any player's turn") — Shen,
    /// Kinkou; Rengar, Pouncing; Janna, Savior; Sumpworks Map. Such a card
    /// also carries Keyword::Reaction on its def. A permanent that only has
    /// a [Reaction] ABILITY (the Seals, Gold) does not — that is
    /// ActivatedAbility::is_reaction — and neither does an [Ambush] unit
    /// (CR 822.1.b); LedgerTest.Defs_APermanentHasReactionOnlyWhenTheCardItselfDoes
    /// holds the two in step. When true the engine offers the card at
    /// Reaction and Action timing, to wherever it may normally be played
    /// (CR 813.3.a). Spells use Keyword::Reaction; [Quick-Draw] gear needs no
    /// override (CR 819.1.b).
```

**Everything else found for other streams is in plans/09 section M** (owners named there): 12 lines for sweep-verify-engine, 13 for sweep-triggers, 2 for sweep-costs, 11 for sweep-verify-cards, 3 for cleanup. `plans/reports/carry-over-audit.md` also lists eight boxes that are still open although their work is done and tested (batch-00 "Equip cost", batch-06 670, batch-08 29 / 466 / 334 / 187, 9.B1, 03 3.13 and 3.21, 07 7.33, 08 8.16 and S3), and six ticked boxes whose wording is stale.

**For the trainer:** `IsMctsAgent`'s play is not move-for-move what `ISMCTSBot`'s was (same algorithm, different random stream), and it is now repeatable. `mcts_agent.cpp` is still one file with no new dependency.

## 5. New defects found (not fixed, not in section M)

- `src/cards/battlefields/0947_mystic_vortex.cpp` / `extraAnyPowerCost` (`game_engine.cpp`): the text is "During showdowns here"; the card publishes its tax while `showdown_in_progress || combat_in_progress` at its battlefield, and the engine charges it for a [Reaction] card played anywhere while any battlefield's tax is up. Whether the combat half and "anywhere" match the card was not checked against the CR. Mystic Vortex had no test at all before `LedgerTest.MysticVortex_*`.
- `tests/test_review_fix_play_combat.cpp`: the test card `TAbilityOnlyReaction` ("Keyword::Reaction on the def because an ABILITY has it (the Seals, Gold)") describes a shape no real card has since 8.8 (part of 9.M10).

For the user (also in the audit): the reprint region tags (539, 543, 546, 547, 537, 548), Karma, Channeler 235's tags, and the hidden Guards! 475 reading (9.I7).
