# sweep-triggers — seventh follow-up report (plans/09 9.BB3: reflexive triggers)

## 1. Result

- Branch `fix/sweep-triggers` (recreated from `7ac7c13`), head `ebc3f96`, worktree clean.
- Verified against `fix/integration` at **`49854a2`**, merged in with no conflicts; integration has not moved since.
- Clean rebuild (`--target clean` first, lane B, `-j3`, the tree's `scripts/ccache_pch_safe.sh`, ccache on) of all four targets at `ebc3f96`:
  - full test suite: **2801 passed, 0 failed** (1 disabled)
  - `riftbound_openspiel_tests`: 25 passed
  - clone-equivalence: 10/10, rc=0
  - smoke: rc=0
  - fault-injection check: rc=0 without the variable, rc=1 with `RIFTBOUND_STEP_FAULT_AT=40`
- No baseline run of the fresh worktree: my first build was still compiling when I started editing headers, so it stopped. The first complete run was with my changes in.

9.BB3 is ticked in `plans/09-found-during-fix-run.md`.

## 2. What the rules require

The rules settle it; no question for the user.

- CR 388: reflexive triggers "create one or more Chain Items when their condition is met". They are recognised by "Do this:", "Do one of the following:", "Do the following".
- CR 389.1: "A new ability is created and added to the chain as a Pending Item."
- CR 388.4 and its Death From Below example: the reflexive trigger "is added to the chain when the spell is resolving, and then on finalization of the reflexive trigger its controller will decide".

So a reflexive trigger is put on the chain while its parent resolves, once the part before the phrase has been done. It is finalized on its own, with its own choices, and players get priority before it resolves. None of the three cards has a reading where no priority is due.

## 3. What changed

**Engine**

- `EffectExecutor::queueReflexiveTrigger(source, controller, index, data)` puts a chain item on the chain. `ChainItem::reflexive_index` says which of the card's reflexive triggers it is; `reflexive_data` carries what the first part produced.
- The engine always runs the finalize pass for such an item, so its "may", costs and targets are chosen at its finalization through the usual helpers.
- It resolves through a new `Card::onReflexiveTrigger(ctx, index, data)`.
- The parent's triggering subject and move are carried over.

**Cards**

| Card | First part (parent ability) | Reflexive trigger |
|---|---|---|
| Hwei 642 | draw 1, discard 1 | by the discarded card's type: draw 1 / ready up to 2 runes (named at its finalization) / +3 [M] |
| Twisted Fate, Gambler 200 | reveal and recycle the top rune | by domain: deal 2 and 1 / draw 1 / stun (target chosen at its finalization). A rune of another domain creates none. |
| Deceiver 756 | pay, play the ready Reflection there | "It becomes a copy of another unit there. Give it [Temporary]." |

One behaviour change beyond timing: Deceiver's token used to be created with [Temporary]. The text gives it in the reflexive trigger, so the token now has no [Temporary] until that resolves.

**Outside my three files:** `game_state.h` (two `ChainItem` fields), `card.h` (`onReflexiveTrigger`), and three small edits in `game_engine.cpp` (wiring the queue callback, the finalize rule, the resolve dispatch).

## 4. Tests

New, all through the engine (`SweepTriggersTest.`):

- `Reflexive_HweiTheOpponentHasPriorityAfterTheDiscard` — the opponent is given priority with the item on the chain, the discard done and the +3 not yet applied.
- `Reflexive_HweiChoosesItsRunesAtFinalization` — the runes are named after the discard and before the opponent's priority; they are readied only on resolution.
- `Reflexive_TwistedFateThePayoffIsAChainItem`, `Reflexive_TwistedFateNoPayoffForAnotherDomain`.
- `Reflexive_DeceiverTheCopyIsAChainItem` — the opponent sees a blank Reflection without [Temporary]; afterwards it is the copy with [Temporary].
- `Reflexive_CannotBeQueuedOutsideAGame`.

Six existing tests drove these cards with a bare executor, where nothing can queue a chain item. They now go through the engine with the same pickers and assertions: `AuditFix1Test.Deceiver_CreatesCopyTokenWithTemporary`, `…Deceiver_NoUnitToCopyStillMakesTemporaryToken`, `AuditFix3Test.Hwei_Discard*` (3), `RulesCards2Test.RulesCards2_Deceiver_ControllerChoosesTheUnitToCopy`.

The new tests answer every prompt explicitly and fail on an unexpected one.

## 5. Mutation runs (two builds)

| Fix disabled | Red tests |
|---|---|
| nothing is put on the chain (`queueReflexive` returns at once) | all four "chain item" tests above, plus 15 existing Hwei / Twisted Fate / Deceiver tests |
| reflexive items are not finalized on their own | `Reflexive_HweiChoosesItsRunesAtFinalization`, `Reflexive_TwistedFateThePayoffIsAChainItem` |

Under the second mutation the Deceiver test stayed green, correctly: Deceiver opts its whole card into choices at finalization, so its reflexive item is finalized either way.

## 6. Needs another stream

- **Trainer note:** these three cards now put a second item on the chain, so there is an extra priority window (and an extra finalization) per trigger. New `ChainItem` fields are not in the feature layout.

## 7. New defects found (not fixed)

- Fourteen other cards print a reflexive trigger and still resolve it inline: Forgotten Relic 895, Disintegrate 5, Dragon's Rage 255, Guards! 475, Arise! 513, Bone Skewer 701, Death from Below 747, Alpha Strike 751, Mirror Image 757, Wild Claw 876, Rell, Magnetic 347, Ivern, Nurturer 613, Keeper of Masks 643, Kharox 901. I found them by searching card text for "do this" / "do the following" / "do one of the following"; I did not read each one. Death from Below is the rule book's own example (CR 388.4). The engine piece they need now exists.
- `VerifyCardsTest.Pin_Hwei_TheRunesAreNamedAfterTheDiscardNotAtFinalization` still passes, but its name now reads oddly: the runes are named at the reflexive trigger's finalization, which is after the discard. The name should say which finalization it means.
