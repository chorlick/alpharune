# ML Agent Training Pipeline — Design Document

## 1. Overview

Train per-legend game-playing agents for Riftbound via self-play reinforcement learning. Each legend gets its own model that learns optimal play for decks built around that legend. Agents are versioned, Elo-rated, and evolved through tournament selection.

## 2. Training Data Format

### 2.1 State Features (~825 floats per decision point)

**Your state (full visibility):**

| Feature | Size | Notes |
|---------|------|-------|
| Hand card IDs | 40 × 153 = max 6120 | 153-dim feature vector per card from registry.json. Padded to max hand size. |
| Hand card count | 1 | |
| Main deck size | 1 | Secret — only size, not contents |
| Rune deck size | 1 | |
| Trash card IDs | ~40 × 1 | card_def_id per card (or one-hot) |
| Banishment card IDs | ~10 × 1 | |
| Base units | per-unit: might, damage, exhausted, keywords, buffs | ~10 floats per unit |
| Base gear | per-gear: attached?, might_bonus, exhausted | ~5 floats per gear |
| Base runes | per-rune: domain, exhausted | ~2 floats per rune |
| Champion zone | card_def_id, on_board? | 2 |
| Legend zone | card_def_id, exhausted | 2 |
| Score, XP | 2 | |
| Rune pool (energy, power per domain) | 8 | |
| Cards played this turn | 1 | |
| Has discarded this turn | 1 | |

**Opponent state (partial visibility):**

| Feature | Size | Notes |
|---------|------|-------|
| Hand SIZE only | 1 | Contents are secret |
| Main deck size | 1 | |
| Rune deck size | 1 | |
| Trash card IDs | ~40 × 1 | Public zone |
| Banishment card IDs | ~10 × 1 | Public zone |
| Base units/gear/runes | same as above | Public zone |
| Champion zone | card_def_id, on_board? | 2 |
| Legend zone | card_def_id, exhausted | 2 |
| Score, XP | 2 | |

**Shared state:**

| Feature | Size | Notes |
|---------|------|-------|
| Battlefields | per-BF: name_id, controller, contested, units per side | ~30 per BF × 2-4 BFs |
| Chain contents | per-item: card_def_id, controller, spell/ability | ~10 per item |
| Turn number, phase, active player | 5 | |
| Victory score target | 1 | |

**Observation history (what you've seen the opponent do):**

| Feature | Size | Notes |
|---------|------|-------|
| Cards opponent has played | 787 | One-hot / count per card_def_id |
| Cards revealed from opponent | 787 | One-hot from reveal/predict events |
| Opponent cards played this turn | 1 | |
| Opponent total spells played | 1 | |
| Opponent total units played | 1 | |

**Compact representation:** Use card_def_id indices (0-787) rather than one-hot for zones with few cards. One-hot for observation history (which cards have been seen). Total input: ~825 floats for compact, ~2000 for full one-hot.

### 2.2 Action Representation

Legal actions are presented as a list of `Intent` objects. Each intent encodes:
- Action type (PlayCard, Move, Spell, Activate, EndTurn, Pass, etc.)
- Card involved (card_def_id)
- Targets (GameObjectIds)
- Location (battlefield ID)

For the ML model, each legal action is featurized and scored. The model outputs a score per legal action, and the highest-scoring legal action is chosen.

### 2.3 JSONL Output Format (already implemented)

```json
{
  "type": "decision",
  "decision_index": 42,
  "turn": 7,
  "phase": "MainPhase",
  "state": { "player1": {...}, "player2": {...}, "battlefields": [...] },
  "legal_actions": [ {"type": "PlayCard", "card": 123, ...}, ... ],
  "chosen_action": {"type": "PlayCard", "card": 123, ...}
}
```

Each game produces one JSONL file with all decision points. Per-game files enable parallel training data loading.

## 3. Model Architecture

### 3.1 Baseline (Phase 1): MLP

```
Input: [state_features: ~825] → Dense(512) → ReLU → Dense(256) → ReLU
  ├── Policy head: Dense(max_actions) → softmax → action probabilities
  └── Value head: Dense(1) → tanh → win probability [-1, 1]
```

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

1. Add memory vector to model input
2. Add memory head to model output
3. Train via self-play with memory enabled
4. Compare memory agents vs non-memory agents
5. Measure: do agents that learn to record observations win more?

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
