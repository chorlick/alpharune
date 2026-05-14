# Additional Game State Feature Dimensions

Candidate features not yet in the feature vector. All are serialized in JSONL or trivially derivable from existing game state. Grouped by priority.

**Status**: Tier 1 (items 1–7, +116 features), the Zone Multi-hot Encoding (+3935 features for perfect-information game state), and the per-player turn-flag block (+2 features for Brynhir lockout observability) are **complete** as of the 4407-dim feature vector (`RESERVED_STATE_DIM=4608`). Implementations live in `src/ml/feature_extractor.{h,cpp}` (C++, the canonical source used by both online inference and binary training-data writer) and `scripts/train_agent.py extract_state_features` (Python, used to validate parity from JSONL). `scripts/parity_check.py` enforces byte-for-byte equality between the two.

The Zone Multi-hot Encoding occupies positions 470..4404: five 787-dim count vectors for self deck, self trash, self banishment, opp trash, opp banishment. Each card_def_id `c` in zone `Z` contributes `count` to position `(zone_offset + c - 1)`. Opponent's deck and hand remain hidden per CR.

**Directive**: Any future game mechanic, decision point, or state change added to the engine MUST also be serialized in the JSONL training data and featurized in both the Python trainer (`scripts/train_agent.py`) and the shared C++ feature extractor (`src/ml/feature_extractor.cpp`). If a new field affects gameplay decisions, it should be exposed to the model. Feature parity between Python and C++ is mandatory; the parity check must pass before training on new feature data.

## Tier 1 — High Impact, Low Dimensional Cost (✅ COMPLETE)

### 1. Combat Designation per Unit (+24 features) ✅

During combat, the model doesn't know which units are committed as attackers/defenders. The `combat` field is already on every serialized unit object but discarded during featurization.

- **Where**: Per top-3 BF unit (by might desc), both sides, all 4 BFs
- **Value**: 0=none, 1=attacker, 2=defender
- **Dim cost**: 3 units × 2 sides × 4 BFs = **+24**
- **Why it matters**: Attacker/defender status determines which units take damage, which can be targeted by combat spells, and whether retreat is possible. Without this, the model treats combat decisions as if all units are interchangeable.
- **Source**: `GameObject::combat_designation` (C++), `obj["combat"]` in JSONL

### 2. Attached Gear card_def_id per Unit (+24 features) ✅

`has_attachment` is serialized as a boolean, but a unit with Warmog's Armor (+3M on conquer) plays completely differently from one with Serrated Dirk ([Assault 2]). Equipment identity changes combat math, targeting priority, and strategic value of individual units.

- **Where**: Per top-3 BF unit, both sides, all 4 BFs
- **Value**: card_def_id of first attached gear (0 if none)
- **Dim cost**: 3 units × 2 sides × 4 BFs = **+24**
- **Why it matters**: The model needs to distinguish "3M unit with Warmog's" from "3M unit with no gear" — they have the same might but wildly different strategic value. Also matters for Weaponmaster targeting and equip decisions.
- **Source**: `GameObject::attachments` (C++), need to add `attachment_def_id` to serialized unit objects
- **Serializer change**: In `serializeObject()`, add `o["attachment_def_id"]` from first attachment's card_def_id

### 3. Might Delta (current - base) per Unit (+24 features) ✅

Tells the model whether a unit is buffed or debuffed without memorizing base stats by card ID. A 5M unit that's normally 3M (buffed +2) is very different from a 5M unit that's naturally 5M — the buff may expire, and conditional effects key off buff state.

- **Where**: Per top-3 BF unit, both sides, all 4 BFs
- **Value**: `current_might - base_might` (can be negative for debuffs)
- **Dim cost**: 3 units × 2 sides × 4 BFs = **+24**
- **Why it matters**: Drives conditional aura evaluation ("While I'm buffed, I have [Ganking]"), identifies temporary buffs that will expire at end of turn, and distinguishes permanently strong units from temporarily pumped ones.
- **Source**: `GameObject::current_might` and `GameObject::base_might` (both already serialized)

### 4. Chain Item Targets (+8 features) ✅

The model knows what cards are on the chain and who controls them, but not WHAT each spell is targeting. This is critical for counter-play — if the opponent's spell targets your best unit, you should bounce or counter it.

- **Where**: Per chain slot (top 4), top-2 target card_def_ids per item
- **Value**: card_def_id of each target (0 if none)
- **Dim cost**: 2 targets × 4 slots = **+8**
- **Why it matters**: "Opponent's burn spell targets my 6M champion" demands a counter; "opponent's burn spell targets my 1M token" does not. The model is currently blind to chain targeting.
- **Source**: `ChainItem::targets` (C++). Need to serialize target_def_ids per chain item in data_serializer.cpp

### 5. Battlefields Scored This Turn — Per-BF Flags (+8 features) ✅

The count `bfs_scored_this_turn` is already featurized, but WHICH battlefields were scored matters for the Winning Point rule: Conquer requires scoring EVERY battlefield this turn (CR 466.1.b).

- **Where**: Per BF (4), per player (2)
- **Value**: 0/1 boolean — was this BF scored by this player this turn
- **Dim cost**: 4 BFs × 2 players = **+8**
- **Why it matters**: The model needs to know "I've scored BF0 and BF1 but not BF2 yet" to evaluate whether conquering BF2 would be a game-winning point. The count alone doesn't convey this.
- **Source**: `PlayerState::battlefields_scored_this_turn` (set<BattlefieldId>)

### 6. Cost Modifier Type (+4 features) ✅

Currently only `cost_modifier_count` is captured — not whether the reduction is "next spell only" or "next unit only" or "all plays this turn." The type completely changes optimal sequencing.

- **Where**: Per player
- **Value**: has_next_spell_reduction (bool), has_next_unit_reduction (bool)
- **Dim cost**: 2 × 2 players = **+4**
- **Why it matters**: "Next spell costs 1 less" means play your most expensive spell next. "All spells cost 1 less this turn" is less urgent. Both appear as `cost_modifier_count = 1` today.
- **Source**: `CostModifier::next_spell_only`, `CostModifier::next_unit_only` on `PlayerState`

### 7. Temporary Might Bonus per Unit (+24 features) ✅

The model sees `current_might` (which includes temporary buffs) but can't distinguish permanent might from this-turn-only might. A unit with `temp_might_bonus = 3` that's currently 6M is really 3M next turn.

- **Where**: Per top-3 BF unit, both sides, all 4 BFs
- **Value**: `temp_might_bonus` (int, can be negative for debuffs)
- **Dim cost**: 3 units × 2 sides × 4 BFs = **+24**
- **Why it matters**: Expiration timing is strategically crucial. "Attack now while my unit is pumped" vs "wait until next turn when buff expires" is a core tactical decision. Also, a temporarily pumped unit is a poor equip target.
- **Source**: `GameObject::temp_might_bonus` (C++). Need to serialize.

**Tier 1 subtotal: +116 features → 470 total** ✅ implemented; positions 354–469. See `kStateFeatureDim` in `src/ml/feature_extractor.h` for the exact layout.

## Tier 2 — Medium Impact

### 8. Top-3 Trash card_def_ids per Player (+6 features)

Both players' trash zones are fully public. Knowing what's been removed tells the model what threats are gone, what Deathknell/recycle targets exist, and what the opponent has already used.

- **Where**: Per player, top 3 card_def_ids from trash (most recent first)
- **Value**: card_def_id (0 if fewer than 3 in trash)
- **Dim cost**: 3 × 2 players = **+6**
- **Why it matters**: Cards like Glasc Mixologist play from trash, Black Rose Spy channels from trash, and strategic awareness of "opponent already used their removal" is high-signal.
- **Source**: `PlayerState::trash` (C++), `trash_card_ids` in JSONL (already serialized as full list)

### 9. Ready vs Exhausted Base Unit Split (+2 features)

The model knows total base unit count but can't distinguish "3 ready units that can move this turn" from "3 exhausted units stuck at base." This directly affects movement decisions and board development tempo.

- **Where**: Per player
- **Value**: count of ready (non-exhausted) base units
- **Dim cost**: 1 × 2 players = **+2**
- **Why it matters**: Ready base units represent immediate movement options. The current feature only shows total base units, so the model can't tell if it has units available to deploy to battlefields.
- **Source**: Already computed during base unit iteration in `extractPlayerFeatures()` — just need to split the count

### 10. Buff Count per Top-3 BF Unit (+24 features)

Number of active buffs on a unit. Drives conditional auras and indicates how much a unit's stats rely on temporary effects.

- **Where**: Per top-3 BF unit, both sides, all 4 BFs
- **Value**: `buff_count` (integer, already serialized)
- **Dim cost**: 3 units × 2 sides × 4 BFs = **+24**
- **Why it matters**: Conditional self-effects ("While I'm buffed, I have [Ganking]") and tactical awareness of which units are propped up by temporary buffs vs naturally strong.
- **Source**: `GameObject::buff_count` (C++), `obj["buff_count"]` in JSONL

### 11. Assault/Shield/Deflect Value Magnitudes (+72 features)

The model knows a unit has [Assault] but not whether it's [Assault 1] or [Assault 3]. Assault 3 on a 3M unit means it's effectively 6M when attacking — a massive difference.

- **Where**: Per top-3 BF unit, both sides, all 4 BFs
- **Value**: assault_value, shield_value, deflect_value (3 ints per unit)
- **Dim cost**: 3 values × 3 units × 2 sides × 4 BFs = **+72**
- **Why it matters**: Combat math depends heavily on these magnitudes. [Shield 4] absorbs huge hits; [Shield 1] barely matters. The keyword count alone says "1 unit has Shield" but not how strong it is.
- **Source**: `GameObject::assault_value`, `shield_value`, `deflect_value` (C++). Already on every object but not serialized for units.

### 12. Cost Modifier Magnitude (+2 features)

The total energy reduction available from all active cost modifiers, not just count.

- **Where**: Per player
- **Value**: sum of `CostModifier::energy_reduction` across active modifiers
- **Dim cost**: 1 × 2 players = **+2**
- **Why it matters**: A -2 energy reduction opens plays that -1 doesn't. Currently only count is captured.
- **Source**: `PlayerState::cost_modifiers` (C++), iterate and sum

### 13. Contested By Self (+4 features)

The `is_contested` flag is already captured, but not WHO is contesting — are you the contester (pushing to take control) or the defender (holding control while opponent pushes)?

- **Where**: Per BF
- **Value**: 1 if perspective player is the contester, 0 otherwise
- **Dim cost**: 1 × 4 BFs = **+4**
- **Why it matters**: Contesting vs defending at a BF drives completely different play patterns (deploy more units to contest vs protect your existing lead).
- **Source**: `BattlefieldState::contested_by` (C++). Need to add to serializer.

### 14. Focus Holder (+1 feature)

During showdowns, who holds focus determines who plays the next spell. The model doesn't currently know whether it has focus or the opponent does.

- **Where**: Global
- **Value**: 1 if perspective player holds focus, 0 otherwise, -1 if no showdown
- **Dim cost**: **+1**
- **Why it matters**: Having focus means you act first in the showdown — affects whether to pass (cede the showdown) or play another spell.
- **Source**: `TurnState::focus_holder` (C++). Need to serialize.

### 15. Additional Turn Queue Depth (+2 features)

The global `is_additional_turn` flag tells the model about the current turn, but not how many extra turns are queued per player.

- **Where**: Per player
- **Value**: count of queued additional turns
- **Dim cost**: 1 × 2 players = **+2**
- **Why it matters**: Your opponent having 2 queued extra turns vs 0 changes threat assessment and urgency significantly.
- **Source**: `PlayerState::additional_turns` (C++). Need to serialize.

### 16. Temp Buff Count per Unit (+24 features)

Separate from permanent `buff_count` — tracks how many buffs expire at ExpirationStep. A unit with 2 temp buffs is less reliable than one with 2 permanent buffs.

- **Where**: Per top-3 BF unit, both sides, all 4 BFs
- **Value**: `temp_buff_count` (int)
- **Dim cost**: 3 units × 2 sides × 4 BFs = **+24**
- **Why it matters**: "Attack now while I have temp buffs" vs "wait" is a core tempo decision.
- **Source**: `GameObject::temp_buff_count` (C++). Need to serialize.

**Tier 2 subtotal: +163 features → 633 total (if all tiers implemented)**

## Deferred — Observation Tracking (Phase 10 prereq)

**Status**: identified, not implemented. Adds private-information observability that comes from reveal effects.

Currently the model only sees zone *end-states* (trash, banishment, on-board). It does NOT capture what a player has *observed* but is no longer in a tracked zone:

- **Aurora-style top-of-deck reveals**: Aurora reveals cards from the top of the deck until it finds a unit. The non-units are recycled back to the bottom of the deck. Both players briefly saw those specific cards, but after recycling they're back in the (hidden-to-opponent) deck. The opponent should *remember* which cards came off the top, but the engine currently has no mechanism for short-term observation memory.
- **Mindsplitter / Sabotage hand reveals**: Force opponent to reveal their hand; the controller sees opp's hand contents, picks one to discard or recycle. The discarded card lands in trash (tracked). The *other* revealed cards stay in opp's hand — observed by the controller, but invisible to the model.
- **Predict / Stacked Deck / Vision**: Player looks at top N of their own deck. Their own information state changes; the model has no slot for it.

### Proposed implementation

1. Add to `PlayerState`:
   - `std::array<uint8_t, 788> observed_in_opp_hand`  — counts of cards I've seen in opp's hand that they still possess
   - `std::array<uint8_t, 788> observed_in_opp_deck`  — cards I've seen leave/return to opp's deck top
2. Hook into reveal events:
   - `CardRevealedEvent` (new): emitted by Aurora, Mindsplitter, Sabotage, Predict, Vision, etc.
   - `ObservationTracker` subscribes and updates the per-player vectors
3. Decay rules:
   - `observed_in_opp_hand[c]`: decrement when opp plays/discards/banishes/recycles a card of that def_id
   - `observed_in_opp_deck[c]`: decrement when opp draws (can't know which card was drawn → probabilistic decay across the vector)
4. Featurization:
   - Add 2 × 787 = 1574 dims per player (×2 perspectives in own view) → +1574 state dims
   - Disk cost: record grows from 23.4 KB → ~30 KB per decision; 10K-game dataset goes from ~57 GB → ~80 GB
5. Lockstep changes in JSONL serializer, C++ feature extractor, Python feature extractor, parity check

**Why defer**: tier 1 + zone multi-hot already brought ~3935 features online; the marginal value of observation tracking is real but smaller than what we've already added. Adding it after the v002 supervised baseline lets us A/B test whether it earns the +4× disk cost.

## Tier 3 — Lower Priority or Higher Dimensional Cost

### 17. Per-Unit Keyword Bits for Top-3 BF Units (+144 features)

Currently we aggregate keyword counts per side (num_tank, num_backline, etc.) but don't associate keywords with specific units. Knowing WHICH unit has Tank matters for targeting decisions — you want to kill the Tank to expose Backline units.

- **Where**: Per top-3 BF unit, both sides, all 4 BFs
- **Value**: 6 keyword bits (Tank, Backline, Ganking, Assault, Shield, Deflect)
- **Dim cost**: 6 × 3 units × 2 sides × 4 BFs = **+144**
- **Why it matters**: Targeting priority. "Kill the 2M Tank to expose the 5M Backline" vs "kill the 5M unit directly" is a critical combat decision that requires knowing keyword assignment per unit.
- **Alternative**: Could reduce to 3 keywords (Tank, Backline, Ganking) that most affect targeting = +72 features

### 18. Damage Assignment Featurization (+8 to action features)

`AssignCombatDamage` intents carry `damage_assignments` (ordered list of unit→damage pairs) but we don't include them in the 25-dim action features. The model sees the action type but not how damage is distributed across defenders.

- **Where**: Action features for AssignCombatDamage intents
- **Value**: Per-assignment target_def_id + damage amount, padded to max defenders
- **Dim cost**: If max 4 defenders: 4 × 2 = **+8** to action features (→ 33-dim)
- **Why it matters**: Distinguishes "spread 3 damage evenly" from "focus-kill the 2M Tank" — fundamentally different combat outcomes from the same action type.
- **Complication**: Changes ACTION_FEATURE_DIM, requires retraining all models

### 19. Chain Item Classification (+12 features)

Each chain item is a spell, permanent, or ability — this determines whether it can be countered and how it resolves. Currently only card_def_id and controller are captured.

- **Where**: Per chain slot (top 4)
- **Value**: 3 classification bits (is_spell, is_permanent, is_ability)
- **Dim cost**: 3 × 4 slots = **+12**
- **Why it matters**: Spells can be countered, abilities generally cannot. A chain with 2 spells is a counter opportunity; a chain with 2 abilities is not.
- **Source**: `ChainItem` classification fields (C++)

### 20. Chain Item Status (Pending/Finalized) (+4 features)

Whether each chain item is still pending (can be interacted with) or finalized (locked in, resolving).

- **Where**: Per chain slot (top 4)
- **Value**: 0=pending, 1=finalized
- **Dim cost**: 1 × 4 slots = **+4**
- **Why it matters**: A finalized item cannot be countered — affects whether to spend resources on a counter.
- **Source**: `ChainItem::status` (C++)

### 21. Delayed Ability Identity (+6 features)

Currently only `delayed_ability_count` is captured. The identity of what's waiting to fire and when changes threat assessment significantly.

- **Where**: Top 2 delayed abilities
- **Value**: card_def_id + trigger_type_int + expires_this_turn_bool (3 per ability)
- **Dim cost**: 3 × 2 = **+6**
- **Why it matters**: "Iascylla will move an enemy unit at your next Main Phase" is very different from a harmless delayed buff.
- **Source**: `GameState::delayed_abilities` (C++)

### 22. Facedown Card Age (+4 features)

Each facedown card at a BF was hidden on a specific turn. If `current_turn > hidden_on_turn`, the card has gained [Reaction] and can be played as an ambush.

- **Where**: Per BF (1 per BF)
- **Value**: turns_since_hidden for oldest facedown card (0 = hidden this turn = can't react yet, 1+ = can be played as Reaction)
- **Dim cost**: 1 × 4 BFs = **+4**
- **Why it matters**: A facedown card hidden last turn is an active threat (can ambush); one hidden this turn is inert. The model currently sees `facedown_count` but not threat level.
- **Source**: `GameObject::hidden_on_turn` (C++)

### 23. Hand Card Power Costs (+20 features)

The model sees hand card energy costs but not power costs. A card costing 2 energy + 2 Fury power is unplayable without Fury runes, even if you have the energy.

- **Where**: Per player, padded to 10
- **Value**: power_cost from CardDef
- **Dim cost**: 10 × 2 players = **+20**
- **Why it matters**: Power costs determine which rune domains need to be recycled. A hand full of Fury power cards with only Calm runes channeled means nothing is playable despite having energy.
- **Source**: `CardDef::power_cost` via `CardDB::get()`

### 24. Total Battlefield Count (+1 feature)

Number of battlefields currently in play (normally 2, can grow with tokens).

- **Where**: Global
- **Value**: `state.battlefields.size()`
- **Dim cost**: **+1**
- **Why it matters**: With 3 BFs from a token, Conquer win condition requires scoring all 3 — changes strategy. The model can infer this from BF features being non-zero, but an explicit count is cleaner.
- **Source**: Already available as `state.battlefields.size()`

### 25. Combat/Showdown Staged Flags (+8 features)

Whether combat or showdown has been declared but not yet started at each BF.

- **Where**: Per BF
- **Value**: combat_staged (bool), showdown_staged (bool)
- **Dim cost**: 2 × 4 BFs = **+8**
- **Why it matters**: If combat_staged=true, units are committed and spells played now resolve before that combat starts. Affects reaction window decisions.
- **Source**: `BattlefieldState::combat_staged`, `showdown_staged` (C++)

**Tier 3 subtotal: +227 features → 808 total (if all tiers implemented)**

## Implementation Notes

- Tier 1 items 1-3 and 7 extend the per-unit loop in `extractSideFeatures()` (C++) and `_extract_side_features()` (Python).
- Items 2 and 7 require serializer changes to emit new fields in `serializeObject()`.
- Items 4-6 require serializer changes to emit chain targets, BF scoring flags, and cost modifier types.
- All feature additions must be mirrored in both Python and C++ to maintain parity.
- Adding Tier 1 alone brings the total to ~470 features. With hidden_dim=512, the state encoder grows from ~182K to ~242K params — well within GPU memory.
- Adding all tiers would reach ~808 features — still manageable with hidden_dim=512 (~415K params), well within a single GPU.

## Feature Parity Mandate

When adding any new feature:
1. Add to `DataSerializer::serializeState/serializeObject/serializeIntent` (C++ JSONL output)
2. Add to `extract_state_features()` / `extract_player_features()` / `_extract_side_features()` / `featurize_single_action()` (Python trainer)
3. Add to `ModelAgent::extractStateFeatures()` / `extractPlayerFeatures()` / `extractSideFeatures()` / `featurizeAction()` (C++ inference)
4. Update `STATE_FEATURE_DIM` / `ACTION_FEATURE_DIM` constants in both files
5. Update this document and `docs/ml-training-design.md` with new dimension count
6. Regenerate training data — old JSONL files will be missing the new fields (Python must handle defaults gracefully)
