# Follow-up 3

Stream cleanup, branch `fix/cleanup`, head `eec0d21`, working tree clean.

**Verified against integration `9754b99`**, which is contained in `eec0d21` and had not moved when the run ended. From a clean build at `-j3` under the lane A lock:

- `riftbound_tests`: 2697 passed, 0 failed, 1 disabled.
- `ctest -R 'riftbound_one_card_table|^riftbound_openspiel_tests$'`: 2 of 2 passed.
- Warnings in the build log: 8, all in third-party OpenSpiel sources; none in engine, card or test code.

9.S1, 9.S2 and 9.S3 are closed and ticked in `plans/09-found-during-fix-run.md`.

## Tests converted after the red gate

The twelve tests from your gate, plus two more that arrived with a later merge. No assertion was weakened; two gained one.

| Test | What it met | Conversion |
| --- | --- | --- |
| `VendettaA.Vendetta_Riven_WeaponmasterEquipsOnPlay`, `AuditFix7Test.Ornn_WeaponmasterEquipsEquipmentGear`, `EnginePathTest.WeaponmasterDoesNotEquipAUnitThatLeftTheBoard` | Weaponmaster "may" | `May::Yes` |
| `VerifyCardsTest.Blitzcrank_...`, `.Faefolk_DoesNotOfferAnEnemyUnitItCannotMove` | the card's own "may" | `May::Yes` |
| `VerifyCardsTest.Pin_{OrnnBlacksmith,DoubleTrouble,FateWeaver,RiftHerald}_...HasNoHatchlingLook` | the card's own "may" | `May::Yes` |
| `VerifyCardsTest.TeemoStrategist_VoidHatchlingLooksOnce...` | unlabelled 0/1 look prompt | explicit picker: the enemy unit, then 1 (recycle), as its comment already said |
| `ReviewFixC1Test.LastRites_ThePlayedUnitIsPaidFor`, `LastRitesTest.ConquerOrHoldTriggerPlaysUnitFromTrash` | new destination prompt (Base / Battlefield #0) | explicit: Base; each now also asserts the unit is at base |
| `VerifyCardsTest.AlphaStrike_...`, `.VolibearFurious_ThroughTheEngine_AllTheDamageWhereTheControllerPutsIt` | "No more targets" after the target was chosen | fixture rule below |

In each "may" case the prompt names the card under test and the test's own assertions expect the yes branch (several assert "the may, then the unit": two prompts), so announcing it does not change what the test proves. For the two Last Rites tests the destination prompt did not exist when they were written and neither asserted a location: the old picker would have taken Battlefield #0 by accident (value 1). They now choose and assert Base.

Three of my earlier conversions conflicted with the owning streams' own adaptations (`test_review_fix_sweep_triggers.cpp`, `test_review_fix_verify_engine.cpp`); I took their versions.

## The "none" rule in `choosing(id)`

1. **When it fires:** only after this picker has already chosen `id` once, and only at a prompt of objects plus exactly one option that names none, where `id` is not offered. If `id` was never offered, the test fails as before. My first version fired in both cases; that was the hole you described and it is closed.
2. **Which tests met it:** the Alpha Strike and Volibear, Furious tests above. Each chooses one enemy unit and intends all the damage on it: after that unit is picked the card offers "No more targets" or another unit, and the test means stop.
3. **Pinning tests:**
   - `FixtureContractPickers.ChoosingStopsAtANoneOptionAfterItsObjectWasChosen`
   - `FixtureContractPickers.ChoosingFailsAtANoneOptionWhenItsObjectWasNeverOffered` (the negative case: `id` never offered, "none" available, the test fails)

**"Choose none" first for Fox-Fire 253, Flash 309, Disposal Order 665:** every test that plays those spells uses a scripted picker that names its objects (`script({O(...)})`, `StepAgent`, or a lambda). None uses the no-picker agent or `accepting()`, so none now silently chooses nothing; all pass. The no-picker agent and `accepting()` do still take the first option of such a prompt, which is now "none". A future test that plays one of these spells with either of them will choose nothing unless it passes a picker. I did not change that default.

## 9.S1

- `src/io/batch_report.h`: a batch that stops on a failed game prints `Results (k of n games completed):`, with averages over the completed games, then the error. Exit status stays 1.
- Tests: `BatchRunnerFaultInjection.OneThread_EarlierGamesCountLaterGamesAreSkipped` (table before error, averages over 2 games), `BatchReport.ACompleteBatchPrintsTheUsualTable`.
- `riftbound_one_card_table` shown red: against a `main.cpp` that builds its own registry and database the binary prints "Card tables built in this process: 2" and the ctest fails; with the file restored it passes.

## 9.S2

- `PlayServerGodMode.EditAtAChainPriorityDecisionRecomputesAndAStaleClickDoesNotEndTheGame` (live server): same scenario as the showdown test. Both repeated 10 times green.
- `accepting()` at a Predict prompt stays "recycle", stated at the picker (to Predict is to choose whether or not to Recycle, CR 437.1; accepting takes the action offered). Pinned by `FixtureContractTest.AcceptingRecyclesAtAPredictPrompt`.

## 9.S3

`scripts/check_ccache_pch_launcher.sh` runs both reproductions and the hit/miss checks on scratch copies with their own cache. CLAUDE.md "ccache" says to run it after a ccache or compiler upgrade and after any change to the launcher.

| Check | `ccache_pch_safe.sh` | plain `ccache` (`--launcher ccache`) |
| --- | --- | --- |
| A. Two copies, one cache: each PCH has only its own change | ok | FAIL (copy B has copy A's change, lacks its own) |
| B1 first build: miss | ok | ok |
| B2 clean, no change: hit | ok | ok |
| B3 declaration added: miss | ok | ok |
| B4 comment reworded in place, clean: miss, and a new unit compiles | ok | FAIL (hit; `redefinition of kInvalidId`) |
| B5 original bytes again: hit | ok | ok |
| Exit status | 0 | 1 |

## One process note

One of my "final" runs in this follow-up was invalid: a merge of integration had conflicted and I started the build without checking the merge result. I caught it from the build errors, resolved the conflicts and re-ran. The numbers above are from the last run, on a clean tree with no conflict markers.
