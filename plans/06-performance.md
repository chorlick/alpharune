# 6. Performance

**Nothing here was measured.** Mechanisms are confirmed by reading; magnitudes
are not. Profile a rollout benchmark before and after each change.

- [ ] **6.1 Trace strings are built whether or not anyone is listening**
  - Where: `src/core/events.cpp:47-50`, `:90-102`. `logTrace` always runs `tagWithDecision` (snprintf plus concatenation), copies a `LogEvent` and emits a signal, with no enabled check.
  - About 885 `logTrace` call sites concatenate (`std::string` + `to_string`) before the call.
  - Worst per-decision sites: `queryAgent` (`game_engine.cpp:6457-6517`), `queryAgentForChain` (`:2243-2247`), `executePlaySpell` (`:1729-1737`), `resolveSpell` (`:1967-1971`, again on every resume), PAY_COST `:5887`, ASSIGN_DAMAGE `:4153-4161`, DAMAGE_STEP `:3931-3947`, KILL `:6283`; about 70 sites in `effect_executor.cpp`.
  - Fix direction: an `events_.traceEnabled()` flag checked at the call site (macro or lambda-taking overload) so the string is never built.

- [ ] **6.2 `recalculateAuras` re-parses card text on every cleanup pass**
  - Where: `game_engine.cpp:4513-4524`, `:4698-4705`. For every on-board object it lowercases and paren-strips the full `ability_text` into fresh strings twice, does ~15 `find`s, and copies an `AuraDef` holding a `std::string` per pattern. Runs up to 20 times per cleanup.
  - Also about ten full passes over the object map and a `granted_abilities` copy per object (`:4398`).
  - Removing the text-parsed auras (2.1) removes this; otherwise cache the parse per `CardDefId`.

- [ ] **6.3 Trigger dispatch allocates and rescans on every event**
  - `Card::firesOn` calls virtual `triggerTypes()`, which returns a `std::vector` by value (`card.h:217-227`), once per (object, trigger) check from `trigger_manager.cpp:160`.
  - Each handler scans all of `state_.objects` (deck, hand, trash included), often several times: `onCardPlayed` four passes, `onCombatStarted` a nested full scan at `:471-480`.
  - `ObjectStateChangedEvent::what_changed` is a `std::string` matched through up to 12 sequential compares (`trigger_manager.cpp:1064-1265`); keys longer than 15 characters allocate per emit. An interned enum would remove it.

- [ ] **6.4 The legal-intent vector is deep-copied three to four times per decision**
  - Where: `step_driver.cpp:83`, `step_driver.h:72`, `game_engine.cpp:163`, `:279`, `:294`, `riftbound_state.cpp:181`, `:218`, `:360`, `riftbound_state.h:170-173`.
  - `currentStep()` returns `StepResult` by value; `CurrentPlayer()` copies every `Intent` just to read `perspective`.
  - `RiftboundHeuristicEvaluator::Prior` (`mcts_agent.cpp:95-118`) calls `LegalActions` and `legalIntents` then runs an O(L²) `encodeAction` match. `LegalActions` allocates an `unordered_set` per call.
  - Fix direction: a `const StepResult&` accessor.

- [ ] **6.5 Legal-action generation rescans all objects repeatedly**
  - `canAfford` rescans all objects for runes for every hand card (`game_engine.cpp:5696`); `availableEnergy` and `canPayAdditionalCost` rescan for every ability of every on-board object (`:3525-3527`); `runesInBase` allocates a vector per call (`:6056`). Compute rune counts once per generation.
  - The ability generators (`:2983`, `:3186`, `:3500`) walk the whole object map, deck included, calling `activatedAbilities()`, which heap-allocates a vector per object (`card.cpp:744`).
  - The Hide generator rescans all objects per Hidden card (`:2865`); the equip generator is a nested objects-by-objects loop (`:2823-2854`).
  - `canActivateAbilityAt`, `activationCostReductionFor`, `empowerAbilityIndex` each rebuild `activatedAbilities()` (`card.cpp:711-742`).
  - `enumerateLegalTargets` recomputes `payableAnyPower` per enemy candidate (`card.cpp:667-669`).

- [ ] **6.6 `processLethalDamage` scans twice per pass even when nothing is damaged**
  - Where: `game_engine.cpp:5056-5057` calls `anyDamageIsLethalFor` twice, each a full scan with two `ability_text.find`s; also once per damaged target (`:2455`).

- [ ] **6.7 `~StepDriver` plays the abandoned game to the end**
  - Where: `step_driver.cpp:19-29`. It drains the fiber by feeding choice 0 until the game ends, so every engine abandoned mid-game (every discarded clone) runs the rest of the game, up to the 200-turn cap, with events and triggers firing.
  - Fix direction: unwind the fiber instead (boost.context `forced_unwind`; requires fixing the `catch (...)` in 1.7 first).

- [ ] **6.8 Card database rebuilt per agent and per seeded game**
  - Each `MctsAgent` / `IsMctsAgent` constructor calls `LoadGame` (`mcts_agent.cpp:235`, `:388`), which runs `registry_.loadAll()`, `buildFromClasses` and two deck parses (`riftbound_game.cpp:52-57`) instead of sharing the process-wide const `CardDB` / `CardRegistry`. Same per game in `openspiel_match.cpp:457`.

- [ ] **6.9 Observation and info-set extraction is allocation- and scan-heavy**
  - Where: `riftbound_state.cpp:280`, `:309`; `feature_extractor.cpp:186`, `:229`, `:251`, `:389-390`, `:541-542`, `:553`.
  - Every `ObservationTensor`, `InfoSetId`, `ObservationString` call builds a fresh 6188-float vector then copies or hashes it; ISMCTS does this per node lookup. Each extraction does about 24 full passes over `state.objects` (mostly `unitsAt` per battlefield per player, done twice).

- [ ] **6.10 Every board query is a full scan of a node-based map of large objects**
  - Where: `src/core/game_state.cpp:39-88`, most of `card_helpers.h` (`payableEnergy`, `runesControlled`, `chooseableUnits`, …), `card.cpp:637`. Each walks all of `unordered_map<GameObjectId, GameObject>` and returns a fresh vector.
  - `GameObject` holds two `std::string`-keyed `unordered_map`s, a tag `vector<string>` and about eight other vectors (`game_object.h:28,184,191`), which makes any state copy expensive.
  - Fix direction: per-zone / per-location id indexes; enum-keyed counters.

- [ ] **6.11 Smaller items**
  - `card_counters["__was_mighty"]` hashes and inserts a string key for every on-board unit on every cleanup (`game_engine.cpp:5015`).
  - `refreshFlowGrants` erases and rebuilds grants with a string-keyed lookup per trash spell on every aura pass (`:4877-4911`).
  - `advanceCombatDamage` dedups through `std::set<std::string>` keys and a recursive `std::function` (`:4060-4142`).
