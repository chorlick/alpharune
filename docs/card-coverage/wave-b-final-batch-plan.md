# Wave B — final batch plan (remaining 19 cards)

Goal: land EVERY header change for the remaining 19 cards in **one speculative
batch → one cascade build (~16 min)**, then implement all `.cpp` wiring + card
bodies as fast (~2s) incremental `--target riftbound_tests` builds (no further
header edits → no further cascades). All header additions are additive and
default-safe (no behavior change until wired in `.cpp`).

Aura-derived per-player/BF flags are reset at the top of
`GameEngine::recalculateAuras` and re-asserted by each card's `applyPassiveAura`.
Turn-scoped fields reset in `PlayerState::resetTurnTracking`.

## Header additions (the single speculative batch)

### `src/effects/effect_types.h` — TriggerType (front-load)
- `WhenAFriendlyUnitChosenHere` — The Dreaming Tree (287).

### `src/cards/card.h`
- `OptionalAdditionalCost`: add `int discard_cards = 0;` + `int reduce_energy = 0;`
  (Brazen Buccaneer 002 — discard as additional cost reduces play cost).
- New `struct AltPlayCost { bool valid=false; int energy=0; int power=0;
  Domain power_domain=Domain::Fury; bool any_domain=false; };`
- `virtual AltPlayCost alternativePlayCost(const GameState&, PlayerId) const { return {}; }`
  (Jhin 651 — "play me for [B]" when condition holds).
- `virtual bool suppressesTemporaryTriggersHere() const { return false; }`
  (LeBlanc 652 — Temporary triggers at her BF don't fire).
- `CardContext`: add `const CardRegistry* registry = nullptr;` (Reckoner's Arena 281
  needs to re-fire other units' conquer logic; also useful for Heimerdinger).
  Requires `class CardRegistry;` forward-decl in card.h. Safe: existing 5-field
  aggregate inits leave it defaulted.

### `src/core/game_object.h`
- `int spells_targeting_me_cost_reduction = 0;` (Irelia 462). Reset in recalc.
- `struct GrantedAbilityRef { CardDefId source_def_id = 0; int ability_index = 0; };`
  + `std::vector<GrantedAbilityRef> granted_abilities;` (Forge 522 / Gardens 769 /
  Heimerdinger 111). Reset in recalc; populated by granting cards' applyPassiveAura.

### `src/core/game_state.h` — PlayerState
Aura-derived (reset in `recalculateAuras` step 1a):
- `bool recall_all_on_attacker_tie = false;` (Symbol of the Solari 227)
- `bool effects_cant_ready_my_units = false;` (Mageseeker Warden 070 clause 2 — set on restricted player)
- `int spells_have_repeat_energy = 0;` `int spells_have_repeat_power = 0;`
  `Domain spells_have_repeat_domain = Domain::Chaos;` (Syndra 708)
- `int repeat_cost_reduction = 0;` (Marai Spire 525)
- `bool has_reveal_peek = false;` (Void Hatchling 341)

Turn-scoped (reset in `resetTurnTracking`):
- `bool grant_repeat_base_to_next_spell = false;` (The Academy 772)
- `int zilean_double_token_turn = -1;` (Zilean 648 once/turn)

### `src/core/game_state.h` — BattlefieldState
- `bool surcharge_enemy_multi_move = false;` (Mageseeker Investigator 725)
- `bool opp_hidden_unrevealable = false;` (Noxus Saboteur 018)
- `bool death_recall_for_pay = false;` (Altar of Blood 762)

### `src/core/intent.h`
- `bool use_alt_play_cost = false;` (Jhin 651)
- `CardDefId granted_ability_def = 0;` (aura-granted ability activation; 0 = own ability)

## Per-card `.cpp` plan (post-cascade, fast builds)

| id | card | hook(s) used | `.cpp` wiring |
|----|------|--------------|---------------|
| 002 | Brazen Buccaneer | OptionalAdditionalCost.discard_cards/reduce_energy | play flow offers discard-1; if taken, -2 energy before payment |
| 651 | Jhin, Meticulous Killer | alternativePlayCost + use_alt_play_cost + max_spell_spent_this_turn | gen emits alt-cost play when ≥4; payment pays [B] instead of printed |
| 462 | Irelia, Graceful | spells_targeting_me_cost_reduction | applyPassiveAura sets =1 on self; spell cost calc reduces by max over intent.targets (play-time targets only) |
| 725 | Mageseeker Investigator | BattlefieldState.surcharge_enemy_multi_move | aura sets flag on its BF; multi-unit enemy move to that BF surcharges [A]/extra unit (approx; single-unit moves uncosted) |
| 648 | Zilean, Time Mage | zilean_double_token_turn | createToken: friendly token UNIT while Zilean at a BF + unused this turn → spawn a duplicate (stamp first to bound once/turn + recursion) |
| 341 | Void Hatchling | has_reveal_peek | aura sets flag; reveal helpers peek top first, optional recycle, then reveal |
| 018 | Noxus Saboteur | BattlefieldState.opp_hidden_unrevealable | aura sets flag; reveal-Hidden path honors it. (Reveal-Hidden mechanic is thin/absent → restriction installed but may be inert; documented.) |
| 227 | Symbol of the Solari | recall_all_on_attacker_tie | aura sets flag; combat resolution: attacker-side tie → recall ALL units to base |
| 281 | Reckoner's Arena | CardContext.registry | WhenYouHoldHere onTrigger iterates units here, re-runs their WhenIConquer onTrigger via registry |
| 652 | LeBlanc, Everywhere at Once | suppressesTemporaryTriggersHere | TriggerManager::fireTrigger skips a Temporary unit's trigger when a card with this flag shares its BF+controller |
| 070 | Mageseeker Warden cl2 | effects_cant_ready_my_units | aura sets on opponent; EffectExecutor::readyObject refuses enemy unit/gear |
| 287 | The Dreaming Tree | WhenAFriendlyUnitChosenHere | BF-scoped, once/turn draw-1 when a spell chooses a friendly unit here (needs a "choose" emit; may be inert if absent; documented) |
| 708 | Syndra, Transcendent | spells_have_repeat_* | applyPassiveAura (gated on showdown) sets 2/1/Chaos; play path builds RepeatCost from grant when spell has no printed Repeat |
| 772 | The Academy | grant_repeat_base_to_next_spell | WhenYouHoldHere sets flag; next spell gets Repeat tranche cost = its base energy; consume on play |
| 525 | Marai Spire | repeat_cost_reduction | aura sets =1; play path reduces repeat_cost.energy (min 0) |
| 522 | Forge of the Fluft | granted_abilities + granted_ability_def | aura appends Forge's [E] to each friendly legend; activation routes to Forge::onActivate (attach equipment) |
| 769 | Gardens of Becoming | granted_abilities | aura appends Gardens' [E] to each unit here; activation → gain 1 XP |
| 111 | Heimerdinger, Inventor | granted_abilities | aura appends refs to every friendly legend/unit/gear [E] ability onto Heimer; activation routes to each source card (best-effort copy) |
| 762 | Altar of Blood | BattlefieldState.death_recall_for_pay | aura sets flag on its BF; killUnit offers pay [A][A][A] → heal+exhaust+recall instead, gated "during combat" |

## Known approximations (documented, not silent)
- Irelia: only play-time-targeted spells get the discount (deferred-target spells pick at chain time, after cost paid).
- Mageseeker Investigator: engine moves are single-unit; surcharge applies to multi-unit moves / repeated moves to the BF — approximation of "multiple at the same time."
- Noxus Saboteur / Dreaming Tree: depend on reveal-Hidden / choose-emit hooks that are thin/absent; the per-BF restriction + trigger are installed correctly but may be inert until those emits exist.
- Zilean: the "you may" is auto-applied (token creation isn't an agent decision point).
- Heimerdinger: copies friendly [E] abilities best-effort; abilities that deeply reference their own source object may behave as Heimer's.
