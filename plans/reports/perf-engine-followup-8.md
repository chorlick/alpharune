# perf-engine — eighth follow-up report (plans/09 section QQ, engine only)

## 1. Result

- Branch `fix/perf-engine`, head `191e7af`, working tree clean. Verified against `fix/integration` `c16ac20` (merged in).
- 9.QQ1 is done and ticked. Every Hidden Blade example passes with the `controllerAfterKill` fix from follow-up 7; this round changed no engine or card code, only tests.
- 9.QQ2 and the seat-release test were dropped by the user's scope change. I had not started either, so there was nothing to restore.
- The three `PerfEngineDecisionHeaders` tests are red against the old header: 3 of 3 failed in a build with `queryAgentForChain` writing "chain priority" for every prompt again. The file was restored from a copy afterwards.
- Final gate set, all on a clean build of the head `191e7af` (lane A lock, `-j3`, PCH-safe launcher), 0 warnings:
  - `riftbound_tests` 2998 passed, 0 failed (2993 before; 5 new).
  - `riftbound_openspiel_tests` 25 passed; clone-equivalence 10/10.
  - Smoke 200 games seed 20261010: P1 139 / P2 61 / 0 draws, 17 turns, 294 decisions — unchanged.
  - Search-microbench observation checksum `94739f84565d4d16` — unchanged.
  - MCTS batch rc 0; fault injection rc 1 as wanted.
  - Checked build (`-DRIFTBOUND_CHECK_BOARD_INDEX=ON`) at the same head: suite 2998 passed, smoke 139/61/0, no throw.
  - Trace capture at the same head: byte-identical to follow-up 7's, so no diff to explain.
  - ISMCTS: `sims=50` ×2 games 10/10 clean; `sims=20` ×8 games 10/10 clean.

## 2. 9.QQ1 — the Hidden Blade passage of the Vendetta rules FAQ

"Kill a unit at a battlefield. Its controller draws 2."

| # | FAQ example | FAQ says | Engine | Test |
|---|---|---|---|---|
| 1 | Returned to hand in reaction (Star-Crossed) | draw ignored | no draw | `PerfEngineHiddenBladeFaq.ReturnedToHandInReaction_TheDrawIsIgnored` |
| 2 | [Repeat] from the Academy, both executions on the same unit | "doesn't draw 4" | draws 2: the second execution mistargets | `.RepeatedOnTheSameUnit_ItsControllerDrawsTwoNotFour` |
| 3 | "I can't be killed." | still draws 2 | **not testable** | — |
| 4 | Moved to base in reaction (Flash) | draw ignored | not killed, no draw | `.MovedToBaseInReaction_TheDrawIsIgnored` |
| 5 | Zhonya's Hourglass in reaction | draws 2 | draws 2 | `PerfEngineHiddenHourglass.HiddenBlade_ADeathReplacedByABanishDrawsNobodyCards` (second half) |
| 6 | Smite earlier this turn | no draw | no draw (fixed in follow-up 7) | same test (first half) |

- **Example 3 cannot be tested.** No card in the engine says "can't be killed" and there is no hook for it. If one is added, a unit that stays on the board takes the same branch as example 5, which draws.
- **How "in reaction" was made.** For examples 1 and 4 the unit is returned or moved through the executor at the first priority window the defending player gets with Hidden Blade on the chain. Star-Crossed and Flash themselves were not played.
- **Control for example 2:** `.RepeatedOnTwoUnits_EachControllerDraws` — a Repeat that chooses a second unit draws again, 4 in all. So the 2 in example 2 comes from the mistarget, not from the Repeat failing to run.
- **Detonate 328** ("Kill a gear. Its controller draws 2.") is the only other card with this wording; I searched every card file for "controller draws". `.Detonate_FollowsTheSameRules`: killed, 2; returned to hand in reaction, 0; Repeat on the same gear, 2.
- The other Hidden Blade mention in the FAQ is about Akali's move trigger still resolving after she is killed; it concerns Akali, not Hidden Blade's own instructions, and I did not test it.

## 3. New findings

No new findings.

Play UI, deferred: none noticed this round.
