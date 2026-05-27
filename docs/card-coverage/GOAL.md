# GOAL: land the last 9 Riftbound cards (Wave B finish)

Repo: `/home/pirateporkchop/code/riftbound-engine`. You are finishing a card-implementation
effort that is 23/32 done. **Your goal: get all 9 remaining cards LANDED — implemented,
tested, and committed, with the suite green.** Not analyzed. Landed.

## Mindset (read this — it's why prior attempts stalled)
- The hard engineering is already DONE. Every field/virtual/trigger these cards need is
  already in the headers and committed. You are wiring `.cpp` only. **No header edits, no
  cascades.** Builds are ~15-30s.
- **Do not re-litigate scope, defer, or "recommend a fresh context."** Implement the card.
- If a clause cannot be modeled perfectly (engine lacks a sub-mechanic), implement the
  **faithful approximation, add a one-line `// APPROX:` comment saying what's simplified, and
  move on.** An approximated-but-functional card with a passing test COUNTS AS LANDED.
- Work ONE card per fast build. Build, run the full suite, commit, next. Don't batch 9 cards
  then debug — you'll lose track.

## Definition of done (per card)
1. Card behavior wired in its `src/cards/.../<id>_*.cpp` (+ any engine `.cpp` integration).
2. A focused test in `tests/cards/test_wave_b_*.cpp` that exercises the real effect (not just a
   flag); update any stale `...Escalated`/`...DefOnly`/`...NoWiredTrigger` pin in
   `tests/cards/test_finish_batch3.cpp` (and similar) to assert the new behavior.
3. `cmake --build build --target riftbound_tests -j6` clean, `RIFTBOUND_ROOT=. ./build/riftbound_tests`
   green (no regressions), `python3 scripts/card_coverage.py --write-lists` count drops by 1.
4. `git commit` (small, per-card) ending with `Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>`.

## Rules
- Build ONLY the test target, one build at a time (mold+ccache are configured in `build/`).
- NEVER edit core headers (game_state.h/game_object.h/card.h/effect_types.h/intent.h) — all
  reserved fields already exist. (If you somehow truly need a new field, batch it; but you don't.)
- No card-specific `if (name==...)`/ability_text matching in the engine — cards set reserved
  flags via applyPassiveAura / overrides; the engine reads them generically.
- Never weaken the engine just to pass a test.

## Do them IN THIS ORDER (easiest first → build momentum)

### 1. Irelia, Graceful (462) — per-target spell cost reduction
- applyPassiveAura: `obj.spells_targeting_me_cost_reduction = 1` on each Irelia instance.
- In `executePlaySpell`, BEFORE `payCardCost`: set
  `ps.transient_play_discount = max over intent.targets of getObject(t).spells_targeting_me_cost_reduction`.
  Reset to 0 right after payCardCost.
- Subtract `ps.transient_play_discount` from `energy_needed` in `canAfford` (~4790) AND in
  `beginCostPayment` (~4942), clamped ≥ 0.
- APPROX: only play-time-targeted spells benefit (deferred-target spells pick at chain time).
- Test: a spell that targets Irelia costs 1 less.

### 2. Jhin, Meticulous Killer (651) — alternative play cost
- `alternativePlayCost(state,player)` returns `{valid=true, power=1, power_domain=Mind}` when
  `state.player(player).max_spell_spent_this_turn >= 4`, else invalid.
- Jhin is a CHAMPION → champion_zone play block (~2139): if alt cost valid AND affordable, ALSO
  push a play Intent with `use_alt_play_cost=true`.
- In the play-execution/payment path: if `intent.use_alt_play_cost`, pay the alt cost (1 Mind
  power) instead of the printed cost (skip the normal payCardCost).
- Test: after spending 4+ on a spell, Jhin is playable for [B]; otherwise not.

### 3. Brazen Buccaneer (002) — discard as additional cost to reduce cost
- `optionalAdditionalCost()` returns `{valid=true, discard_cards=1, reduce_energy=2}`.
- In `maybePayOptionalAdditionalCost` (~4639): support discard_cards (prompt; if accepted,
  discard N and apply reduce_energy). The reduction must land BEFORE base payment — if ordering
  fights you, apply the discount via `ps.transient_play_discount += cost.reduce_energy` (reuse
  Irelia's path) so it's subtracted in payment. Test: discarding makes Brazen cost 2 less.

### 4. Void Hatchling (341) — reveal peek/recycle
- applyPassiveAura: `state.player(controller).has_reveal_peek = true` while on board.
- In the reveal helpers (`revealAndChoose`/`predict`/`revealUntil`, ~908-1060): if the revealing
  player `has_reveal_peek`, first peek the top card and optionally recycle it (agent choice),
  then reveal. APPROX is fine if a full agent prompt is awkward — do "look, recycle if it would
  be revealed anyway" simplest faithful version. Test: top card can be recycled before reveal.

### 5. Immortal Phoenix (037) — play from trash when a spell kills a unit
- Emit `TriggerType::WhenYouKillAUnitWithASpell` where a spell's damage/effect kills a unit
  (TriggerManager onUnitDied or the kill path — fan to the killer's controller, reaching cards
  in TRASH too, since Phoenix is in trash). Phoenix `triggerType()` = that; onTrigger:
  confirmOptional pay [1][R] → play self from trash (reuse TrashReplayGrant machinery,
  generateTrashReplayActions ~2726 / payTrashReplayGrant, or playIgnoringCost after manual pay).
- Test: spell kills a unit → may pay to play Phoenix from trash.

### 6. Heimerdinger, Inventor (111) — proxy all friendly [E] abilities
- DECISION: populate from `recalculateAuras` (it HAS `card_registry_`), NOT applyPassiveAura.
  In recalculateAuras, after the applyPassiveAura pass: for each on-board Heimer, for every
  friendly legend/unit/gear with `activatedAbilities()`, append a `GrantedAbilityRef{thatDef, i}`
  to Heimer's `granted_abilities` for each ability index i. The granted-ability pipeline
  (generator + dispatch) already handles the rest.
- APPROX: abilities that deeply reference their own source act as Heimer's. Document it.
- Test: Heimer gains a friendly unit's [E] ability (shows up as activatable / runs).

### 7. Altar of Blood (762) — pay to survive combat death
- applyPassiveAura: set `death_recall_for_pay = true` on Altar's battlefield.
- In `killUnit` (~5328), before the actual death: if the dying unit's BF has the flag AND that
  BF's `combat_in_progress`, prompt the controller "pay [A][A][A]?" (use the canPayAdditionalCost
  / getAgent().selectAction pattern). If paid → heal (damage_marked=0), exhaust, recall to base
  (mirror the Tactical-Retreat self-replacement block) and return without dying.
- DECISION if the mid-kill prompt is unsafe/awkward: auto-pay when affordable (drop the "may"),
  mark `// APPROX: auto-pays when affordable`. Still counts as landed. Test: a unit dying in
  combat at Altar with 3 power survives (healed+exhausted+recalled).

### 8. Mageseeker Investigator (725) — multi-move surcharge
- applyPassiveAura: set `surcharge_enemy_multi_move = true` on its battlefield.
- DECISION: the engine moves single units (uncosted), so a true surcharge has nothing to gate.
  Set the flag faithfully and add `// APPROX: surcharge inert — engine moves are single-unit/uncosted`.
  Test: flag is set on the BF while Investigator is there. (This is the honest ceiling here.)

### 9. The Dreaming Tree (287) — BANNED, lowest priority
- triggerType `WhenAFriendlyUnitChosenHere`; onTrigger: once/turn (card_counters turn-stamp) →
  draw 1. If no "a spell chooses a friendly unit here" emit exists, add a minimal emit at the
  spell-target-resolution site IF trivial; otherwise wire the handler and mark
  `// APPROX: inert until a 'spell chooses a unit' event is emitted`. Test: the trigger draws 1
  (drive it directly).

## Finish
When all 9 are committed and the gate reads 0 incomplete: update
`docs/card-implementation-audit.md` (mark the Wave B finish, note the documented approximations)
and the `project-wave-b-card-completion` memory. Done = 32/32, suite green, gate at 0.
