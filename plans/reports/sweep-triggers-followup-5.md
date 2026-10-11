# sweep-triggers — fifth follow-up report (plans/09 9.Q1–9.Q4)

## 1. Result

- Branch `fix/sweep-triggers`, head `d845124`, worktree clean, `fix/integration` merged in. One conflict resolved, in `plans/09` only.
- Clean rebuild of all four targets (lane B, `-j3`, safe launcher) at `fd`-merge commit before the last two commits:
  - full test suite: 2642 passed, 0 failed (1 disabled)
  - `riftbound_openspiel_tests`: 24 passed
  - clone-equivalence: 10/10, rc=0
  - smoke: rc=0
  - fault-injection check: rc=0 without the variable, rc=1 with `RIFTBOUND_STEP_FAULT_AT=40`
- After that I made one test-only commit and merged integration once more (Fox-Fire 253 and a verify-cards test file). That head was built incrementally, not from clean: **2647 passed, 0 failed**. The other three checks were not re-run on it.

All four boxes are ticked in `plans/09-found-during-fix-run.md`.

## 2. Items closed

| Item | Outcome | What changed | Test (`SweepTriggersTest.`) |
|---|---|---|---|
| **9.Q1** predicates can drift | FIXED | `applyReplacement` in 77, 374, 264, 762 and 550 now starts with its own `replacementApplies`, so there is one copy of the conditions. The default `Card::replacementApplies` applies to nothing. | `ReplacementEffects_EveryCardDefinesWhenItApplies` |
| **9.Q2** two Zileans, one token play | FIXED | The rules settle it: both apply, three tokens. Each Zilean's "may" is asked on its own; a declined one is still available later that turn. | `Zilean_EachCopyIsOnceEachTurnOnItsOwn`, `Zilean_ADeclinedCopyIsStillAvailableLaterThatTurn` |
| **9.Q3** stale "killed with a spell" marker | FIXED | The marker and the damage attribution are forgotten when damage is healed and whenever either damage path adds damage to an object that has none. That covers cards that zero `damage_marked` themselves, so no card needed editing. | `KillWithASpell_HealedSpellDamageIsForgotten` |
| **9.Q4** real tie-recall test | FIXED (test added) | — | `CombatParticipants_ARecalledAttackerOfARealTieWasInTheCombat` (through `runCombat`, both sides stunned) |

**9.Q2 rules basis** (`rules/core-rules.md`):

- CR 370.2: "A Replacement Effect can only be applied once to an event, or to any Game Actions or events that replace that event." Its example has two Zhonya's Hourglasses both applying to one death.
- CR 372: when more than one applies to the same event, the controller orders them.
- CR 371.2.b: a "may" that is declined "has not been applied this turn".

So each Zilean applies once to the play (or to what replaced it) and adds one copy.

**9.Q1 registry test.** It cannot detect an override directly. It asserts that the registry's cards with a replacement effect are exactly those five, that none applies to an unrelated death, and that the one that does apply is used without a prompt. A new replacement card has to be added to that list by hand.

**Outside my files:** one line in `game_engine.cpp` (`applyCombatDamage` calls `EffectExecutor::forgetDamageSources` where it used to zero the attribution itself).

**Stricter test agent (your heads-up).** My test file no longer uses `EnginePathAgent::choosing`; it has its own picker with the old semantics, the Predict "recycle" answer is explicit, and the death-replacement default is an explicit `script({0})`. I have not seen cleanup's change, so I could not run against it. Its other announced change — a post-intent cleanup inside `fireTriggerThroughEngine` / `resolveThroughEngine` — may still affect tests of mine that leave lethal damage marked.

## 3. Mutation run (one build)

| Fix disabled | Red test |
|---|---|
| Zhonya's predicate always true | `ReplacementEffects_EveryCardDefinesWhenItApplies` (and `WaveABatch3Test.ZhonyasIgnoresEnemyDeath`) |
| at most one copy per play | `Zilean_EachCopyIsOnceEachTurnOnItsOwn`, `Zilean_ADeclinedCopyIsStillAvailableLaterThatTurn` |
| spell marker not forgotten in `dealDamage` | `KillWithASpell_HealedSpellDamageIsForgotten` |
| combat participants not recorded | `CombatParticipants_ARecalledAttackerOfARealTieWasInTheCombat`, `…AUnitRecalledBeforeTheEndWasInTheCombat` |

Not mutated: the `forgetDamageSources` call in `applyCombatDamage`. My test reaches the executor path only; no test of mine heals a unit and then kills it with combat damage.

## 4. Needs another stream

- **Trainer note:** with two Zileans a single token play can now ask two yes/no prompts and produce three tokens.

## 5. New defects found

- The combat-damage half of 9.Q3 (heal, then lethal combat damage) is fixed by the one-line call but has no test.
