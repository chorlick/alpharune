# ML Agent Training Pipeline — Design Document

## 1. Overview

Train per-legend game-playing agents for Riftbound via self-play reinforcement learning. Each legend gets its own model that learns optimal play for decks built around that legend. Agents are versioned, Elo-rated, and evolved through tournament selection.

## 2. Training Data Format

### 2.1 State Features (354 floats — implemented)

**Global features (10):**

| Feature | Size | Notes |
|---------|------|-------|
| Turn number | 1 | |
| Phase | 1 | 0=Mulligan, 1=Awaken, 2=Beginning, 3=Scoring, 4=Channel, 5=Draw, 6=Main, 7=Ending, 8=Expiration |
| Is turn player | 1 | 1 if perspective player's turn |
| Went first | 1 | 1 if perspective player won coin toss (starting_player) |
| Score differential | 1 | self_score - opponent_score |
| Chain length | 1 | Number of items on the chain |
| NS state | 1 | 0=neutral, 1=showdown |
| OC state | 1 | 0=open, 1=closed |
| Is additional turn | 1 | 0/1 flag |
| Delayed ability count | 1 | Number of pending delayed abilities |

**Chain item features (4 × 2 = 8):**

| Feature | Size | Notes |
|---------|------|-------|
| Source card_def_id | 1 | Card that created this chain item (0 if empty slot) |
| Controller is self | 1 | 1 if perspective player controls, 0 otherwise |

Padded to MAX_CHAIN_ITEMS=4.

**Per-player features (70 × 2 = 140):**

| Feature | Size | Notes |
|---------|------|-------|
| Score, XP | 2 | |
| Hand size, deck size, rune deck size | 3 | |
| Energy | 1 | Available rune pool energy |
| Burned out | 1 | 0/1 flag |
| Cards played this turn | 1 | |
| Champion card_def_id, in_zone | 2 | |
| Legend card_def_id, exhausted | 2 | |
| Power by domain | 7 | Fury, Calm, Mind, Body, Chaos, Order + universal |
| Hand card IDs | 10 | card_def_ids padded to 10. Opponent's all 0 (secret). |
| Hand card costs | 10 | Energy costs padded to 10. Opponent's all 0. |
| Ready runes, exhausted runes | 2 | Counts |
| Rune domain breakdown (ready) | 6 | Count of ready runes per domain |
| Rune domain breakdown (exhausted) | 6 | Count of exhausted runes per domain |
| Base unit count, total might, total damage, num exhausted | 4 | |
| Base unit IDs top 3 | 3 | card_def_ids sorted by might desc |
| Base gear count | 1 | |
| Base gear IDs top 2 | 2 | card_def_ids |
| Trash size | 1 | Number of cards in trash zone |
| Banishment size | 1 | Number of cards in banishment zone |
| Has discarded this turn | 1 | 0/1 flag |
| BFs scored this turn | 1 | Count of battlefields scored |
| Num BFs controlled | 1 | Count of battlefields this player controls |
| Legion active | 1 | 1 if cards_played >= 2 |
| Cost modifier count | 1 | Number of active cost modifiers |

**Per-battlefield features (49 × 4 = 196):**

| Feature | Size | Notes |
|---------|------|-------|
| Battlefield card_def_id | 1 | |
| Controller | 1 | 0=none, 1=P1, 2=P2 |
| Contested, combat, showdown | 3 | 0/1 flags |
| Facedown count | 1 | Hidden cards at this BF |
| Attacker is self | 1 | 1=self attacking, 0=opp attacking, -1=no combat |
| Combat phase | 1 | 0=none, 1=showdown, 2=damage, 3=resolution |
| Is token | 1 | 0/1 flag |
| Per side (P1 then P2): | 20 × 2 | |
| — Unit count | 1 | |
| — Total might | 1 | |
| — Total damage | 1 | |
| — Num exhausted | 1 | |
| — Num stunned | 1 | |
| — Keyword counts: Tank, Backline, Ganking, Assault, Shield, Deflect | 6 | |
| — Top 3 unit card_def_ids | 3 | Sorted by might desc |
| — Top 3 unit current_might | 3 | |
| — Top 3 unit damage_marked | 3 | |

**Future additions (not yet implemented):**

| Feature | Notes |
|---------|-------|
| Observation history | One-hot of cards opponent has played/revealed (787-dim) |
| Hand card features | Card feature vectors from registry.json instead of raw IDs |

### 2.2 Action Features (25 floats per action, up to 64 actions)

Each legal action is featurized as a 25-dim vector:

| Feature | Size | Notes |
|---------|------|-------|
| Action type one-hot | 14 | EndTurn, PassPriority, PassFocus, PlayCard, PlayActionCard, PlayReaction, StandardMove, ActivateAbility, ActivateActionAbility, MulliganDecision, AssignCombatDamage, HideCard, MakeChoice, ChooseBattlefield |
| Card card_def_id | 1 | Card involved in this action (0 if none) |
| Target card_def_ids | 2 | Up to 2 targets, padded with 0 |
| Ability source card_def_id | 1 | Source of activated ability (0 if none) |
| Destination battlefield | 1 | BF ID for movement (-1 if none) |
| Play location battlefield | 1 | BF ID for play-to-location (-1 if none) |
| Mulligan count | 1 | Number of cards to mulligan (0 if not mulligan) |
| Unit def IDs | 2 | Units to move (StandardMove), padded with 0 |
| Chosen object def ID | 1 | First chosen_object card_def_id (MakeChoice) |
| Chosen battlefield ID | 1 | ChooseBattlefield target (-1 if none) |

The model scores each legal action independently. Up to `MAX_LEGAL_ACTIONS=64` actions per decision point; excess truncated, padding masked with `-inf`.

### 2.3 JSONL Output Format (implemented)

```json
{
  "type": "decision",
  "decision_index": 42,
  "turn": 7,
  "phase": "MainPhase",
  "turn_player": "P1",
  "state": {
    "starting_player": "P2",
    "player1": { "score": 3, "power": {"Fury": 2}, "trash_card_ids": [101], ... },
    "player2": { ... },
    "battlefields": [ { "facedown_count": 1, "units": [...], ... } ],
    "chain": [ ... ]
  },
  "legal_actions": [
    {"type": "PlayCard", "card_def_id": 123, "target_def_ids": [456], "play_bf": 0},
    {"type": "EndTurn"},
    ...
  ],
  "chosen_action": {"type": "PlayCard", "card_def_id": 123, ...},
  "chosen_index": 0
}
```

Each game produces one JSONL file. Per-game files enable parallel loading via memmap.

## 3. Model Architecture

### 3.1 Baseline (Phase 1): Per-Action Scoring MLP (implemented)

```
State encoder:  [state_features: 354] → Dense(hidden) → ReLU → Dense(hidden/2) → ReLU → state_enc

Action scorer:  [state_enc ⊕ action_features: hidden/2 + 25] → Dense(hidden/2) → ReLU → Dense(1) → score
                (applied independently to each of 64 legal actions)
                Masked with -inf for padding actions, then softmax for training loss.

Value head:     state_enc → Dense(64) → ReLU → Dense(1) → Tanh → win probability [-1, 1]
```

Default hidden_dim=256 (132K params). Recommended hidden_dim=512 (520K params) for richer feature set.
Training: CrossEntropyLoss on action scores + MSELoss on value × 0.5. Adam optimizer, cosine LR schedule.

### 3.2 Intermediate (Phase 2): Attention over cards

```
Card embeddings: each card in hand/board → 153-dim → Dense(64)
Board state: positional encoding per location (base, BF0, BF1, ...)
Attention: self-attention over all visible cards
  ├── Policy head
  └── Value head
```

### 3.3 Advanced (Phase 4): Memory-augmented

```
Input: [state_features] + [memory_vector: 256]
  ├── Policy head → action
  ├── Value head → win probability
  └── Memory head → what to write to memory (256 floats)
```

Memory vector is carried between decision points within a game. Updated by the memory head output. Training requires backprop through time (BPTT) within each game.

## 4. Training Phases

### Phase 1: Supervised Baseline (~2 days compute)

**Goal:** Agent that beats random >80% of the time.

1. Generate 50K games per legend with RandomAgent vs RandomAgent
2. 40 legends × 50K = 2M games total
3. At ~200 games/sec (Release, 8 threads): **~3 hours** to generate
4. Each game ~400 decisions → 800M decision points total
5. Train MLP per legend via supervised learning (imitation of "better" random moves weighted by game outcome)
6. Evaluation: each model plays 1000 games vs RandomAgent, measure win rate

### Phase 2: Self-Play RL (~1 week compute)

**Goal:** Agent that beats Phase 1 agent >60%.

1. For each legend, run self-play: current_model vs current_model
2. Generate 10K games per iteration
3. Train on game outcomes (policy gradient: reinforce winning actions)
4. ~20-50 iterations to convergence
5. 40 legends × 50 iterations × 10K games = 20M games
6. At ~200 games/sec: **~28 hours** to generate (parallelizable across legends)
7. Checkpoint every 5 iterations. Version: `MissFortuneAgent_v5`, `v10`, etc.

### Phase 3: Cross-Archetype League (~2 weeks)

**Goal:** Agents that are robust across matchups, not just mirrors.

1. Round-robin tournament: each legend's agent plays all other legends
2. 40 × 39 / 2 = 780 matchups, 100 games each = 78K games per round
3. Elo ratings computed from tournament results
4. Focus training on worst matchups (lowest Elo delta)
5. Re-train, re-tournament, iterate
6. ~10 rounds of league play

### Phase 4: Memory-Augmented Agents (~1 month)

**Goal:** Agents that track opponent information and adapt mid-game.

#### The Problem

The current model is stateless — each decision is evaluated from a 4407-dim snapshot with no memory of prior turns. The snapshot already includes full zone composition (self deck/trash/banishment + opp public zones), but it does NOT include short-term reveals: if P1 sees Charm in P2's hand through a reveal effect and P2 doesn't immediately play it, the model has no mechanism to register "I saw Charm in their hand." See `docs/additional-gamestate-dims.md` "Deferred — Observation Tracking" for the proposed fix.

#### Recommended Approach: Hybrid (engine-tracked observations + learned interpretation)

**Layer 1 — Engine observation log (factual, rule-based):**

The engine already emits events for every zone change (play, discard, reveal, bounce, kill, etc.). An `ObservationTracker` component subscribes to the EventBus and maintains a per-player observation state:

- **Cards revealed**: 787-dim binary vector. Set to 1 when a card is revealed to a player (via play, reveal effect, Vision keyword, etc.). Cleared to 0 when the card leaves the known zone (played from hand, discarded, recycled into deck).
- **Cards removed from game**: Separate 787-dim vector for cards in trash/banishment (public zones). These never "un-reveal" — once in a public zone, they're permanently known.
- **Opponent play history**: Ordered list of card_def_ids the opponent has played this game (capped at last N plays). Captures opponent's strategy pattern.

This is deterministic bookkeeping, not learned — the engine knows exactly when cards move zones. No model capacity wasted on learning to count.

**Layer 2 — Model interpretation (learned, attention-based):**

The observation log is passed as additional input features. The attention model (Phase 2 architecture) learns what's strategically relevant:

- "Opponent revealed 2 removal spells earlier but has played 1 — they likely still have one"
- "Opponent has channeled 4 Fury runes and played aggressive units — expect combat tricks"
- "Opponent's trash contains their only counter spell — safe to commit to a big play"

The model interprets the facts; it doesn't need to learn the facts.

**Layer 3 — Optional learned memory head:**

For information that can't be tracked deterministically (opponent bluffing patterns, tempo assessment, strategic reads), a 256-dim learned memory vector carried between decisions:

```
Input: [state_features] + [observation_log] + [memory_vector: 256]
  ├── Policy head → action
  ├── Value head → win probability
  └── Memory head → updated memory vector (256 floats)
```

Training requires BPTT within each game (decisions are sequential, memory flows forward).

#### Implementation Steps

1. Build `ObservationTracker` subscribing to EventBus (CardPlayedEvent, CardRevealedEvent, ObjectKilledEvent, etc.)
2. Serialize observation state in JSONL (per-decision snapshot of what each player has seen)
3. Add observation features to Python/C++ extractors (787-dim revealed + recent play history)
4. Train attention model on observation-augmented features
5. Measure: does observation-aware agent beat observation-blind agent?
6. Optional: add learned memory head, train with BPTT, measure incremental gain

#### Key Design Decision

The engine handles **what was seen** (deterministic). The model handles **what it means** (learned). This separation avoids asking the model to learn bookkeeping it can't reliably do, while still allowing strategic reasoning over imperfect information.

## 5. Per-Legend Agent Strategy

### Why Per-Legend?

- Each legend defines the deck's domain identity (1-2 domains)
- Different legends have fundamentally different win conditions (aggro vs control vs combo)
- A Miss Fortune deck (Fury/Chaos, aggressive) plays completely differently from an Ornn deck (Calm/Mind, equipment-focused)
- Per-legend models converge 3-5x faster than a universal model

### Model Count

- 40 legends × ~50 versions = ~2000 model files over full training
- Each model: ~2-10 MB (MLP) or ~50-100 MB (attention)
- Storage: ~20-100 GB total over full training history

### File Layout

```
models/
├── miss_fortune/
│   ├── v001_random_baseline.pt
│   ├── v005_self_play.pt
│   ├── v010_self_play.pt
│   ├── v015_league.pt
│   └── latest.pt → v015_league.pt
├── ornn/
│   ├── v001_random_baseline.pt
│   └── ...
├── elo_history.csv
└── league_results/
    ├── round_001.json
    └── round_002.json
```

## 6. Elo Rating System

### Implementation

- Start all agents at Elo 1000
- After each match: update both players' Elo using standard formula
- K-factor: 32 for new agents, 16 for established (>100 games)
- Track per-matchup win rates (40×40 matrix)
- Regression detection: if agent v(N+1) has lower Elo than v(N), investigate

### Elo History Format (CSV)

```csv
timestamp,legend,version,elo,games_played,win_rate_vs_random
2026-05-15,miss_fortune,v001,1050,1000,0.82
2026-05-16,miss_fortune,v005,1180,5000,0.91
```

### Matchup Matrix

```csv
,miss_fortune_v15,ornn_v10,vex_v12,...
miss_fortune_v15,-,0.55,0.48,...
ornn_v10,0.45,-,0.52,...
```

## 7. Evolutionary Framework

### Population Management

Each legend maintains a population of agent versions:
- **Current best**: highest Elo, used for cross-archetype evaluation
- **Training candidate**: currently being trained via self-play
- **Archive**: all past versions, for regression testing

### Selection Pressure

1. Training candidate plays 500 games vs current best
2. If win rate > 55%: candidate becomes new current best, old best archived
3. If win rate < 45%: candidate discarded, restart training from current best with different hyperparams
4. If 45-55%: continue training for more iterations

### Checkpoint Policy

- Save model every 5 self-play iterations
- Save model when it becomes new current best
- Keep all "current best" versions permanently
- Prune intermediate checkpoints after 30 days

## 8. Compute Budget Estimates

| Phase | Games | Time (8 threads, Release) | Models |
|-------|-------|---------------------------|--------|
| Data generation (Phase 1) | 2M | ~3 hours | - |
| Phase 1 training | - | ~4 hours (GPU) | 40 |
| Phase 2 self-play generation | 20M | ~28 hours | 40 × ~10 versions |
| Phase 2 training | - | ~20 hours (GPU) | 40 |
| Phase 3 league (10 rounds) | 780K | ~1 hour per round | 40 |
| Phase 4 memory | 20M+ | ~28 hours+ | 40 |
| **Total Phase 1-3** | **~23M** | **~60 hours compute** | **~500 models** |

Assumes Release build at ~200 games/sec on 8 threads. GPU training on a single RTX 3080/4080 class card.

## 9. OpenSpiel Integration

### Option A: pybind11 wrapper (recommended)

Wrap the C++ engine as a Python module. The game exposes:
- `new_initial_state()` → GameState
- `legal_actions(state)` → list of action indices
- `apply_action(state, action)` → new GameState
- `is_terminal(state)` → bool
- `returns(state)` → [p1_reward, p2_reward]

OpenSpiel algorithms (AlphaZero, MCTS, CFR) plug directly into this interface.

### Option B: Subprocess (simpler, slower)

Python training script calls `./riftbound` binary with specific seeds, collects JSONL output, trains offline. No live interaction — batch training only.

**Recommendation:** Start with Option B (already works with current batch runner). Move to Option A when training speed becomes the bottleneck.

## 10. Agent Loading in Engine

The engine loads the appropriate agent based on the deck's legend:

```cpp
// In main.cpp or batch_runner:
auto legend_name = toLower(card_db.get(deck.legend).name);
auto model_path = "models/" + legend_name + "/latest.pt";

std::unique_ptr<AgentInterface> agent;
if (std::filesystem::exists(model_path)) {
    agent = std::make_unique<ModelAgent>(model_path, card_db);
} else {
    agent = std::make_unique<RandomAgent>(seed);
}
```

The `ModelAgent` implements `AgentInterface::selectAction()` by:
1. Converting GameState → feature tensor
2. Running model inference
3. Scoring legal actions
4. Returning highest-scoring action

## 11. Ban List Integration

Ban list (`cards/ban-list.csv`) is loaded by DeckValidator at startup. Format:

```csv
SET,ID,'DISPLAY NAME'
```

Note: display names are single-quoted because some contain commas (e.g., `'Draven, Vanquisher'`). The DeckValidator looks up cards by display name in the CardDB.

Banned cards are excluded from:
- Deck validation (engine rejects decks containing banned cards)
- Deck agent card pool (deck builder skips banned cards during generation)
- Training data (games with banned cards are invalid)
