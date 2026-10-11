# sweep-triggers — sixth follow-up report (plans/09 9.U3, 9.V1; incident for 9.X1)

## 1. Result

- Branch `fix/sweep-triggers`, head `dfe546a`, worktree clean.
- Clean rebuild of all four targets at `dfe546a`, lane B, `-j3`, **with ccache disabled** (`CCACHE_DISABLE=1`, because of the incident below):
  - full test suite: **2745 passed, 0 failed** (1 disabled)
  - `riftbound_openspiel_tests`: 25 passed
  - clone-equivalence: 10/10, rc=0
  - smoke: rc=0
  - fault-injection check: rc=0 without the variable, rc=1 with `RIFTBOUND_STEP_FAULT_AT=40`
- `fix/integration` was merged before that run (one conflict, in `plans/09` section U). It has moved 17 commits since; I have not merged those.

Both boxes are ticked in `plans/09-found-during-fix-run.md`.

## 2. Items closed

| Item | Outcome | What changed | Test (`SweepTriggersTest.`) |
|---|---|---|---|
| **9.U3** dropped play after a god-mode edit | FIXED | The dropped plays are now a `ChainManager` member and are cleared deliberately on an outside edit: `GameEngine::noteEditWhileParked` calls `ChainManager::forgetDroppedPlays`. The reason is stated at both sites — the edit may have removed what made the play fail, and the rebuilt list already offers it. | `PriorityWindow_AnOutsideEditBringsBackADroppedPlay`, `PriorityWindow_TheEngineForgetsDroppedPlaysOnAnEdit` |
| **9.V1** combat half of the spell-kill marker | FIXED (test added) | — | `KillWithASpell_HealedSpellDamageThenCombatDamageIsNotASpellKill` (through `runCombat`), with a control, `KillWithASpell_SpellDamageStillMarkedCountsInCombat` |
| **9.V1** registry finds the replacement cards | FIXED | `Card::replacementAppliesTo` returns `NotDefined`, `No` or `Yes`. The five cards answer Yes / No; the base class answers NotDefined. | `ReplacementEffects_EveryCardDefinesWhenItApplies` now walks the registry: every card with `hasReplacementEffect()` must not answer NotDefined, every other card does |

**9.U3 limits, stated plainly:**

- There is no live play-server test. A play cannot fail through the real engine — the generator's affordability gate comes first — so a live edit has nothing to bring back. The behaviour is tested at `ChainManager` level.
- The second test checks the hook by reading `game_engine.cpp` for the call inside `noteEditWhileParked`. It is a source check, not a behavioural one.

**Outside my files:** one line in `game_engine.cpp` (`noteEditWhileParked` calls `forgetDroppedPlays`), and `card.h` (the `Applies` enum and the non-virtual `replacementApplies` wrapper).

## 3. Mutation run (one build, uncached)

| Fix disabled | Red test |
|---|---|
| `forgetDroppedPlays` does nothing | `PriorityWindow_AnOutsideEditBringsBackADroppedPlay` |
| hook removed from `noteEditWhileParked` | `PriorityWindow_TheEngineForgetsDroppedPlaysOnAnEdit` |
| `forgetDamageSources` removed from `applyCombatDamage` | `KillWithASpell_HealedSpellDamageThenCombatDamageIsNotASpellKill` |
| Soraka's `replacementAppliesTo` override removed | `ReplacementEffects_EveryCardDefinesWhenItApplies` (and three Soraka tests) |

## 4. Stale object incident (for 9.X1)

- **What failed:** `SweepTriggersTest.ReplacementEffects_EveryCardDefinesWhenItApplies`. The engine listed four replacements for a death only one applied to.
- **Why that test:** my `card.h` change replaced the virtual `bool replacementApplies` with a virtual `Applies replacementAppliesTo` in the same vtable slot. A stale `effect_executor.cpp.o` called the old slot and read `No` (1) as `true`.
- **It survived a clean build:** the same failure appeared after `cmake --build build --target clean` with the launcher in place.
- **How I confirmed it:** touched `effect_executor.cpp` and rebuilt with `CCACHE_DISABLE=1`; that one file recompiled and the test passed with no source change.
- **Effect on this report:** every number in section 1 and section 3 is from uncached builds.
- **Earlier reports:** I did not re-check them against this. My earlier header changes added members rather than reusing a slot, and their tests exercised the new members, so a stale object would have failed them. That is an argument, not a re-run.

## 5. Needs another stream

- Nothing new.
- My fifth report (`sweep-triggers-followup-5.md`) has a garbled phrase in its Result section ("at `fd`-merge commit"); it should read "at the merge commit before the last two commits". The numbers in it are right.

## 6. New defects found

- None in the engine.
