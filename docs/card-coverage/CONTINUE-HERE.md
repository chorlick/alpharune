# Wave B continuation prompt (paste into a fresh context)

Continue the "Wave B" effort to finish the last unimplemented Riftbound cards.

## State
- **23 of 32 done, 9 left. Suite GREEN at 1043 tests, working tree clean, HEAD on master.**
- ALL engine header scaffolding is already committed — **no more header changes / cascades are
  needed.** Every remaining card is a fast (~15-30s) incremental build.
- Authoritative design guide: `docs/card-coverage/wave-b-final-batch-plan.md`.
  Progress + per-card hooks + line numbers: memory `project-wave-b-card-completion`.

## Build / test cadence (IMPORTANT)
- Build ONLY the test target, one build at a time: `cmake --build build --target riftbound_tests -j6`
  (mold + ccache are wired into `build/`; never run parallel builds).
- Run tests: `RIFTBOUND_ROOT=. ./build/riftbound_tests`
- Coverage gate (currently 9): `python3 scripts/card_coverage.py --write-lists`
- Do NOT edit core headers (game_state.h / game_object.h / card.h / effect_types.h / intent.h)
  — all needed fields are already in. A header edit triggers a ~16-min full recompile.
- When a card is done: add a focused test under `tests/cards/test_wave_b_*.cpp`, build, run the
  full suite (no regressions), update any stale `...Escalated`/`...DefOnly` pins in
  `tests/cards/test_finish_batch3.cpp`, then commit with a "Wave B: <card>" message ending in:
  `Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>`
  Commit in small increments (per card or tight cluster), like the existing Wave B history.

## The 9 remaining (all reserved fields already exist; wire the .cpp)
1. **Irelia, Graceful (462)** — `GameObject::spells_targeting_me_cost_reduction` (set 1 in
   applyPassiveAura). Apply via `PlayerState::transient_play_discount`: in executePlaySpell,
   before payCardCost, set it = max over intent.targets of that field; subtract it in
   `canAfford` (~4790) and `beginCostPayment` (~4942); reset after. Play-time-targeted spells
   only (deferred-target spells pick at chain time — documented approximation).
2. **Jhin, Meticulous Killer (651)** — `Card::alternativePlayCost` + `Intent::use_alt_play_cost`.
   Jhin is a CHAMPION (champion_zone play block ~2139). Emit an extra play option when
   `alternativePlayCost(state,player).valid` (gate: `max_spell_spent_this_turn >= 4`) and the
   alt cost [B]=1 Mind power is affordable; in execution pay the alt cost instead of printed.
3. **Brazen Buccaneer (002)** — `OptionalAdditionalCost.discard_cards` + `.reduce_energy`.
   Offer a discard-1 at play; if taken, reduce energy by 2 BEFORE payment. Check
   maybePayOptionalAdditionalCost (~4639) ordering vs payCardCost.
4. **Mageseeker Investigator (725)** — `BattlefieldState::surcharge_enemy_multi_move` (aura).
   Moves are uncosted single-unit in the engine; enforcement is approximate/likely inert —
   set the flag faithfully and document.
5. **Altar of Blood (762)** — `BattlefieldState::death_recall_for_pay` (aura). In killUnit
   (~5328) mirror the Tactical-Retreat self-replacement block, but add a mid-kill "pay
   [A][A][A]?" agent prompt gated on the dying unit's BF flag + that BF's combat_in_progress.
   HOT/reentrant path — guard heavily.
6. **Void Hatchling (341)** — `PlayerState::has_reveal_peek` (aura). Insert peek + optional
   recycle of the top card into the reveal helpers (revealAndChoose/predict/revealUntil ~908-1060).
7. **Immortal Phoenix (037)** — `TriggerType::WhenYouKillAUnitWithASpell` (reserved). Emit it
   where a spell kills a unit (fan to the controller incl. trash), then alt-play-from-trash via
   the existing TrashReplayGrant machinery (generateTrashReplayActions ~2726, payTrashReplayGrant).
8. **Heimerdinger, Inventor (111)** — "I have all [E] abilities of friendly legends/units/gear."
   applyPassiveAura can't see the registry. Populate Heimer's `GameObject::granted_abilities`
   from `recalculateAuras` (which HAS card_registry_) as a special pass, OR change
   applyPassiveAura's signature (cascade). Uses the granted-ability pipeline already built.
9. **The Dreaming Tree (287)** — `TriggerType::WhenAFriendlyUnitChosenHere` (reserved). Needs a
   "a spell chooses a friendly unit" emit that doesn't exist — likely inert; BANNED card; lowest
   priority. BF-scoped, first-choose-each-turn → draw 1.

Caveat: the cost-system cards (1-3) touch a high-regression-risk area — wire carefully, one per
build, and lean on tests. Honor the memory note: never tweak the engine just to pass a benchmark.
