# sweep-triggers — second follow-up report (plans/09 sections H and I)

## 1. Result

- Branch `fix/sweep-triggers`, head `ae7abdc`, worktree clean. `fix/integration` (`5457c1e`, which now includes sweep-costs) is merged in with no conflicts.
- Clean rebuild (`--target clean` first, lane B, `-j3`, `ccache_pch_safe.sh`) of all four targets at `121f26d`:
  - **full test suite: 2497 passed, 0 failed** (1 disabled, as before)
  - `riftbound_openspiel_tests`: 20 passed, 0 failed
  - **clone-equivalence: 10/10 passed**, rc=0
  - smoke: 40 random games, yi_rebuild vs fiora, seed 7, rc=0
- `ae7abdc` on top of that merges one integration commit that changes only `plans/`; I did not rebuild for it.
- Not run: the `RIFTBOUND_STEP_FAULT_AT` gate check.

All seven boxes assigned to me (9.H1–9.H4, 9.I1–9.I3) are ticked in `plans/09-found-during-fix-run.md`.

## 2. Items closed

| Item | Outcome | What changed | Test (`SweepTriggersTest.` unless noted) |
|---|---|---|---|
| **9.H1** `DeathBatch` lifetime | FIXED | Two parts. Every `DeathBatch` in a card now sits in a block of its own with its kill loop (28 loops). And the engine no longer trusts a stale snapshot: a unit that was listed in a batch, survived and was changed since is snapshotted afresh when it dies. | `SweepTriggersCardTest.DeathBatch_ASurvivorThatChangedIsSnapshottedAfresh` — a test spell that kills one unit, buffs the survivor, then kills it, all inside one open batch |
| **9.H2** Heron when the first card is countered | NOT A DEFECT | The rules settle it; no rules question for the user. Comment added at the code site. See below. | pin: `AstralHeron_ACounteredFirstCardStillCountsAsPlayed` |
| **9.H3** Gold tokens announced ready | FIXED | `createToken(..., enters_exhausted)`: the token is exhausted when its play is announced, unless "your tokens enter ready" replaces that. Used by `createGoldExhausted`, Bloodharbor Ripper 746 and Card Sharp 403. | `GoldToken_PlayedExhaustedIsExhaustedWhenAnnounced` (a listener on the play event, through both cards), `GoldToken_EntersExhaustedYieldsToTokensEnterReady` |
| **9.H4** cost-only activations use up passes | FIXED | An activation that adds no chain item no longer counts against the window's 10 passes. A separate bound of 512 stops an activation that never changes anything. | `PriorityWindow_CostOnlyActivationsAreNotPasses` — 12 activations, then a real response in the same window |
| **9.I1** move not on the chain item | FIXED | `ChainItem::triggering_move` (origin and destination) is set on every ability a move triggers: the unit's own, its Equipment's, watchers', battlefield cards', delayed abilities. Cards read it with `triggeringMove()`. Fae Porter 446 takes "the same battlefield" from it. | `MoveTrigger_FaePorterUsesTheDestinationOfItsMove` (Porter gone from the battlefield before finalization) |
| **9.I2** two moves share one origin | FIXED | The `__moved_from_bf` counter on the unit is removed. Lillia 644, Harpoon Squad 458 and Akali 808 read the trigger's own move. | `MoveTrigger_TwoMovesGiveEachTriggerItsOwnOrigin`, `MoveTrigger_AkaliChoosesAtTheEndsOfThatMove`, `C5Test.HarpoonSquadOnlyWhenItMovesFromABattlefield` |
| **9.I3** Lillia's silent base default | FIXED | Lillia 644, Fae Porter 446 and Akali 808 throw `std::logic_error` if a move trigger resolves without a move. | `MoveTrigger_WithoutAMoveIsAnError` |

**9.H2 evidence.** Two passages in `rules/core-rules.md`:

- CR 420.4.a.1: "If a game effect prevents the resolution of the card—for example, because the card was countered—abilities that trigger on playing cards will not trigger." The Heron's text is a triggered ability, so it does not trigger for a countered first card.
- CR 420.4.b and its two Defy examples: a countered spell still counts as played for checks ("Any Legion abilities … will be active"; Battering Ram is costed as if another card had been played). So the card played after it is the second card, not "your first card".

That is what the code already does.

**Side effects worth knowing:**

- Bloodharbor Ripper 746 used to force its Gold exhausted even with "your tokens enter ready" in play; it now follows that replacement like the other Gold sources.
- 11 existing tests fired a move trigger without any move. They now state the move with a new fixture helper, `AnnouncedMove` (`tests/cards/card_test_fixture.h`); none was weakened.

## 3. Mutation run (one build, six mutations)

| Fix disabled | Red test |
|---|---|
| re-snapshot of a changed survivor | `SweepTriggersCardTest.DeathBatch_ASurvivorThatChangedIsSnapshottedAfresh` |
| `enters_exhausted` ignored | `GoldToken_PlayedExhaustedIsExhaustedWhenAnnounced`, `GoldToken_EntersExhaustedYieldsToTokensEnterReady` (and 14 Gold tests in other suites) |
| cost-only activation counts as a pass | `PriorityWindow_CostOnlyActivationsAreNotPasses` |
| Heron accepts any play ordinal | `AstralHeron_ACounteredFirstCardStillCountsAsPlayed` (pin), `C7Test.AstralHeron_*` (3), `VendettaA.Vendetta_AstralHeron_*` |
| move not copied onto the chain item | `MoveTrigger_TwoMovesGiveEachTriggerItsOwnOrigin`, `MoveTrigger_FaePorterUsesTheDestinationOfItsMove`, `MoveTrigger_AkaliChoosesAtTheEndsOfThatMove` |
| Lillia's base default put back | `MoveTrigger_WithoutAMoveIsAnError` |

Not mutation-tested: the block scoping of the 28 `DeathBatch` guards. With the re-snapshot in place, widening a scope changes nothing observable, so no test can go red for it. The re-snapshot is the part that is proven.

## 4. Needs another stream

- No `shared:` commit. Files outside my three that these commits touch: `src/core/game_state.h` and `game_object.h` (new fields), `src/cards/card_helpers.h` (`triggeringMove`, `battlefieldOfLocation`), `tests/cards/card_test_fixture.h` (`AnnouncedMove`), and five existing test files.
- **Trainer / checkpoint note**: new state, not in the feature layout — `ChainItem::triggering_move`, `ChainState::move_being_announced` (empty outside one engine call), `LastKnown::batch_stamp`.

## 5. New defects found (not fixed)

- `AuditFix5Test.Lillia_NoLocationFallsBackToBase` still exists and passes. Its name describes the base fallback that 9.I3 removed; I did not read what it now asserts. It should be renamed or checked by whoever owns that file.
- `card_helpers.h` `spendBuff(ctx, unit)` (9.D13, sweep-costs' item): after the costs merge I could not find the helper by that name in `card_helpers.h` or `card.cpp`. I did not check where the spell alternative-cost path now spends the buff, so whether Wallop 146 / Call to Glory 207 credit the right spender is unverified from my side.
- The `DeathBatch` stamp leaves out Might and keywords on purpose (an aura ending mid-batch must not look like a change). A survivor whose only change is an aura-granted keyword or Might from another source, and which then dies inside the same open batch, would still use the batch snapshot. No current card does this.
