# sweep-triggers — follow-up report (plans/09 section D)

## 1. Result

- Branch `fix/sweep-triggers`, head `e699d94`, worktree clean. `fix/integration` (`a1e472c`) is merged in.
- Clean rebuild (`--target clean` first, lane B, `-j3`, `ccache_pch_safe.sh`) of `riftbound_tests`, `riftbound` and `riftbound_openspiel_tests` at `d2fdfe7`:
  - **full test suite: 2430 passed, 0 failed** (1 disabled, as before)
  - `riftbound_openspiel_tests`: 20 passed, 0 failed
  - smoke: 40 random games, yi_rebuild vs fiora, seed 7, rc=0
- The two commits after that (`551275c`, `e699d94`) change only files under `plans/`; I did not rebuild for them.
- Not run: clone-equivalence test, `RIFTBOUND_STEP_FAULT_AT` gate check.
- One merge conflict, resolved: `spells/0247_icathian_rain.cpp` (integration's `kLost` guard kept, my death batch placed before the kill loop).

All eight boxes assigned to me are ticked in `plans/09-found-during-fix-run.md`.

## 2. Items closed

| Item | Outcome | What changed | Test |
|---|---|---|---|
| **9.D1** Astral Heron 831 | FIXED | "Your first card each turn" is judged from the played card's play ordinal in `triggerCondition`, not from the plays the Heron has heard of. A countered response can no longer hide the first card. The Heron can also be its own first card (`WhenYouPlayMe`). | `C7Test.AstralHeron_AFirstSpellWhoseResponseWasCounteredStillCounts`, `…AnsweredByYourOwnReactionStillCounts`, `…APlayOrdinalFromAnEarlierTurnDoesNotCount`; `VendettaA.Vendetta_AstralHeron_*` moved to the engine path |
| **9.D2** multi-kill effects are one batch | FIXED | New `DeathBatch` guard (`effect_executor.h`) declared before the kill loop in 27 cards. | `SweepTriggersTest.KillAllEffect_UnitsItKillsDiedTogether` (The Ruination vs two Loyal Poros, a Lonely Poro and Battle Mistress) |
| **9.D4** missing tests | FIXED | Tests only, plus two existing tests extended. `recycle_self` is **not** dead — a test gear in `test_review_fix_play_combat.cpp` uses it — so I tested it rather than deleting it. | `SweepTriggersTest.Mulligan_AnnouncesTheRecycleOnce`; `PlayCombatFixTest.RecycleSelfCost_*` (now assert the announcement); `SweepTriggersTest.ReactionPlay_GoesThroughThePlayCallbackAndAFailedPlayIsNotReoffered`; `…ReactionPlay_ThroughTheEngineIsAFullPlay` |
| **9.D5** red-before evidence | FIXED | Three mutation builds; see section 3. | — |
| **9.D7** Stacked Deck 183 | NOT A DEFECT | Card text is "Put 1 into your hand", not "draw". Code site commented. While there I added the missing knowledge-tracker call for the move to hand. | pin: `SweepTriggersTest.StackedDeck_PuttingACardIntoHandIsNotADraw` |
| **9.D9** token card set after the event | FIXED | `createToken(..., token_card)` sets the card before `TokenCreatedEvent`, and on Zilean's copy. Seven sites migrated: Zed 810 / 899, Death Mark 931, `createGoldToken`, `createGoldExhausted`, Bloodharbor Ripper 746, Card Sharp 403. | `SweepTriggersTest.CreateToken_TheTokenCardIsSetBeforeThePlayIsAnnounced` |
| **9.D10** re-asking after a play that adds no item | FIXED for plays | A play that did not happen is not offered again in that priority window. Activated abilities are left alone on purpose: a cost-only ability (a rune's Add) legitimately adds no item and may be used again. | `…ReactionPlay_GoesThroughThePlayCallbackAndAFailedPlayIsNotReoffered` |
| **9.D11** log audience audit | FIXED | Audited every log line that prints a card name in the 78 card files touching a hand, deck or facedown zone (40 lines). All name cards that are revealed, on the board, in a trash or banished. The real leak was in the shared prompt helpers: `Card::pickTarget` / `pickTargetPair` listed their options by name publicly. Those lines are now private to the player asked when any option is in a hand, a deck or facedown. | `SweepTriggersTest.PromptLines_NamingHiddenCardsArePrivate` |

9.D2 cards: 41, 127, 128, 133, 148, 159, 190, 200, 209 (both loops), 247, 248, 253, 255, 300, 306, 316, 402, 436, 634, 672, 680, 742, 751, 768, 854, 870, 877. Five other cards call `killObject` twice without a loop (179, 251, 726, 149, 481); I read each and none kills two units of the same controller in one effect, so none needs a batch.

## 3. Mutation runs (9.D5)

Each mutation disables one fix in a `.cpp` file; the tree was restored with `git checkout` after each build. Logs are in the session scratchpad (`sweep-triggers/mutA.log`, `mutB.log`, `mutC.log`).

**Two tests stayed green in build A with their fix disabled**, so they were not testing it. Both were rewritten and then shown red in build B:

- `IgnoresDeflect_IsHonouredForATargetChosenAsTheSpellIsPlayed` — the spell re-picked its target at resolution either way. It now gives the player a spare [A] and asserts it is not spent.
- `ReactionPlay_ThroughTheEngineIsAFullPlay` — the old fallback path produced the same state. It now plays a [Repeat] spell in response; only the engine's play path offers Repeat.

### Build A — 29 mutations, 37 sweep tests red

| Fix disabled | Red test (`SweepTriggersTest.` unless noted) |
|---|---|
| `onTokenCreated` returns at once | `TokenUnit_IsAUnitBeingPlayed`, `TokenUnit_IsAUnitTheOpponentPlayed`, `Weaponmaster_*` (2), `Svellsongur_TheBearersTriggeredAbilityTriggersTwice` |
| Zilean's "may" never asked | `Zilean_AdditionalCopyIsAskedAndPlacedByChoice`, `Zilean_AdditionalCopyCanBeDeclined` |
| `killerOf` returns nobody | `SolariShrine_TriggersOnlyWhenItsControllerKilledTheUnit`, `…DoesNotTriggerWhenTheOpponentKilledTheirOwnUnit`, `C0Test.SolariShrine_*` |
| unit-carried death replacements skipped | `UnlicensedArmory_ReplacementAppliesWithTheGearGone`, `C0Test.UnlicensedArmory_*` |
| keyword instances capped at 1 | `Vision_EachInstanceTriggersSeparately` |
| `onStateSettled` returns at once | `Renekton_IsEmpoweredWhenAnotherEffectTakesHimToTen`, `C7Test.RenektonBrute_*` (2) |
| `spendBuff` does not announce | `FaeDragon_PlaysGoldWhenItsControllerSpendsABuff` |
| `onUnitStunned` returns at once | `WhenYouStun_FiresForAnEnemyUnitWithItAsTheSubject` |
| Darius without `WhenYouPlayMe` | `Darius_SeesHimselfAsTheSecondCard` |
| standing facedown look ignored | `ScuttleCrab_ShowsCardsHiddenLaterThatTurn` |
| old delayed-ability expiry test | `DelayedAbility_DefaultExpiresAtEndOfTurn` |
| `recycleCards` skips board handling | `RecycleCards_APermanentLeavesTheBoardProperly`, `ReviewFixC2Test.DivineJudgment_*` |
| `playIgnoringCost` ignores `entersReadyOnPlay` | `PlayIgnoringCost_AsksTheUnitWhetherItEntersReady` |
| temp Deflect not recorded | `TemporaryDeflect_EndsAtTheExpirationStep`, `ExecutorReviewFixTest.TemporaryDeflect_*` |
| `attachGear` on-board check removed | `AttachGear_RefusesAUnitThatIsNotOnTheBoard` |
| no end-of-turn equipped triggers | `BlightedBattleaxe_EndOfTurnAbilityFiresWhileAttached`, `MissingFixTest.BlightedBattleaxe_*` |
| `drawHeldCard` counts no draw | `CalledShot_DrawingOneCountsAsADraw`, `RevealAndDrawIt_CountsAsADraw` |
| Shepherd's Heirloom back to raw `xp +=` | `GainXp_IsCountedForTheTurn` |
| replayed-spell detection off | `ReplayedSpell_StaysOnTheChainUntilTheNewPlayResolves` |
| private log lines made public | `LogEvent_ADrawIsVisibleOnlyToThePlayerWhoDrew`, `SweepTriggersLogAudience.SaysExactlyWhichPlayersMayRead`, `PlayServerProtocol.FairPlayFramesWithhold…` |
| `textCopiesOn` returns 0 | `Svellsongur_*` (5 of 6; the sixth is a pin) |
| Heron accepts any ordinal | `C7Test.AstralHeron_*` (3), `VendettaA.Vendetta_AstralHeron_*` |
| `DeathBatch` takes no snapshot | `KillAllEffect_UnitsItKillsDiedTogether` |
| recycle-self announcement removed | `PlayCombatFixTest.RecycleSelfCost_GoesToTheOwnersDeck…` |
| mulligan announcement removed | `Mulligan_AnnouncesTheRecycleOnce` |
| play callback bypassed | `ReactionPlay_GoesThroughThePlayCallbackAndAFailedPlayIsNotReoffered` |
| token card not set in `createToken` | `CreateToken_TheTokenCardIsSetBeforeThePlayIsAnnounced` (and 12 Gold-token tests in other suites) |
| prompt lines always public | `PromptLines_NamingHiddenCardsArePrivate` |
| `ignoresDeflect` not consulted | none in A — see above; red in B |

### Build B — 10 mutations

| Fix disabled | Red test |
|---|---|
| Vision ability does nothing | `Vision_TheControllerChoosesWhetherToRecycle`, `AuraVisionTest.Forecaster_*`, `C4Test.GemcraftSeer_*` |
| no keyword-level Weaponmaster | `Weaponmaster_ASandSoldierTokenMayEquipAsItIsPlayed`, `Weaponmaster_IsAMay` |
| play server ignores the audience | `PlayServerProtocol.FairPlayFramesWithholdOpponentHandAndDeckOrder` |
| Forecaster's def keyword put back | `Forecaster_HasOneInstanceOfVision` |
| Stacked Deck counts a draw | `StackedDeck_PuttingACardIntoHandIsNotADraw` |
| gear token announced as a unit | `TokenGear_DoesNotTriggerPlayAUnit` |
| `ignoresDeflect` not consulted | `IgnoresDeflect_IsHonouredForATargetChosenAsTheSpellIsPlayed` (rewritten) |
| play callback bypassed | `ReactionPlay_ThroughTheEngineIsAFullPlay` (rewritten) |
| replacement's turn check removed | `UnlicensedArmory_ReplacementEndsWithItsTurn` |
| Darius accepts any ordinal | `Darius_IsNotTriggeredAsTheFirstCard` |

### Build C — 1 mutation

| Fix disabled | Red test |
|---|---|
| failed play not remembered | `ReactionPlay_GoesThroughThePlayCallbackAndAFailedPlayIsNotReoffered` |

### Not mutated

- `Svellsongur_AnActivatedAbilityIsOfferedOnce` and `LonelyPoro_DrawsWhenItDiesAlone` pin behaviour that no fix changed, so there is nothing to disable.
- The 12 tests shown red before the first report (recall, move-block, score doubling, lethal-damage death batch, LeBlanc) were not re-mutated.

## 4. Needs another stream

- No new `shared:` commit. This follow-up touched two files outside my three, inside ordinary commits: `src/cards/card.cpp` (`promptAudience`, 9.D11) and `tests/test_review_fix_play_combat.cpp` (two assertions, 9.D4).
- **sweep-costs**: `createGoldToken` / `createGoldExhausted` in `card_helpers.h` now pass `kGoldGearCardDefId` to `createToken` instead of assigning it afterwards. If its branch edits those helpers, keep the argument — 12 Gold tests fail without it.
- **Trainer note**: `TGT_PROMPT` / `TGT_PAIR_PROMPT` log lines can now be private to one player.

## 5. New defects found (not fixed)

- `DeathBatch` lives to the end of the enclosing block, not just the kill loop. A unit listed in it that survives and is killed later in the same block keeps the earlier last-known snapshot. No current card does this.
- Astral Heron 831: if the first card played is itself countered, it never triggers and the second card is not treated as "your first card". Whether a countered card counts as played for this text needs a rules decision.
- Bloodharbor Ripper 746 and Card Sharp 403 still set `is_exhausted` on their Gold token after `createToken` has announced it, so a play-time watcher sees the token ready. `createToken` has no "enters exhausted" for gear.
- `ChainManager::stepExecuteAndPass` (from reading the loop, not reproduced): each cost-only ability used in response consumes one of the 10 priority passes (`kMaxPriorityPasses`), so a player who uses about ten of them in one window is cut off and the chain resolves.
