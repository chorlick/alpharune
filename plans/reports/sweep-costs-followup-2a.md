# sweep-costs follow-up 2 — interim report (2a)

All eleven items are done and ticked: 9.L1, 9.L2, 9.M26, 9.M27, 9.N2, 9.N5, 9.R2, 9.T1, 9.T2, plus the batch-05 Irelia box and the batch-07 Sky Crusier correction. The delay was the lane A queue and three rounds of re-merging a moving integration.

**Mergeable now:** `fix/sweep-costs` at `8ac9ada`. That commit is merged with integration as it stood before 9.T4 landed. Clean rebuild there: `riftbound_tests` 2711/2711, openspiel 25/25, clone-equivalence 10/10, smoke rc 0, MCTS batch rc 0, fault injection rc 1.

**In progress:** the merge with integration that contains 9.T4 (`chooseTargetsAtFinalize` wrapper, Repeat choices at finalization). One conflict in `runFinalizePass`, resolved by keeping my `collectFinalizeChoices` split and taking the nested-resume save/restore from integration. The gate on that tree is running; the full report (`sweep-costs-followup-2.md`) follows when it is green.

How 9.N2 / 9.T1 meet 9.T4, as merged:
- A `kFinalizeResolve` spell's first-execution choices are made in `executePlaySpell` before `payCardCost` and kept in `prefinalized_spell_`.
- They are adopted in `chooseFirstExecution` when the chain's Finalize step reaches the item, so the item is not asked again. I did not use `finalized_early_`; adoption does the same job and also announces the held "chosen" events.
- The Repeat block is left where 9.T4 put it, at the Finalize step, after `repeats_paid` is set.
- Prompt order for these spells is therefore: first-execution choices, cost prompts (additional costs, Repeat), then each Repeat execution's choices.

One thing to know before merging `8ac9ada`: commit `0999945` on the branch contains conflict markers in `spells/0253_fox_fire.cpp` and `plans/09` (a script committed through a failed merge). The next commit, `14776ab`, fixes both. Do not build or bisect at `0999945`.
