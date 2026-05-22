# Deck Construction Agent — Design Document

## 1. Problem Definition

Build a system that generates tournament-legal Riftbound decks optimized for win rate. The deck agent selects ~40 cards from a pool of ~200 legal cards (filtered by legend's domain identity), constrained by tournament rules. Evaluation is done by running games against a field of opponents using the engine.

This is a **combinatorial optimization** problem, not a sequential game. It is a separate application from the play agent.

## 2. Tournament Constraints

All constraints are enforced by `DeckValidator` (`src/rules/deck_validator.h`):

| Constraint | Rule | Notes |
|-----------|------|-------|
| Main deck size | Exactly 40 cards (including champion) | CR 101 |
| Rune deck size | Exactly 12 runes | CR 102 |
| Battlefields | Exactly 3, unique names | CR 103 |
| Sideboard | 0-8 cards | CR 104 |
| Domain identity | All cards must match legend's 1-2 domains | CR 105 |
| Copy limit | Max 3 copies of any named card | CR 106 |
| Signature limit | Max 3 signature cards total, matching champion tag | CR 107 |
| Champion | Must be a champion unit with matching tag | CR 108 |
| Ban list | No banned cards (from `cards/ban-list.csv`) | |

### Ban List Format

File: `cards/ban-list.csv`

```csv
SET,ID,'DISPLAY NAME'
SFD,122,'Called Shot'
SFD,020,'Draven, Vanquisher'
```

**Note:** Display names are wrapped in single quotes because some contain commas (e.g., `'Draven, Vanquisher'`). The parser must handle this correctly — split on commas but respect single-quote delimiters.

## 3. Deck Space Size

For a typical legend with 2 domains:
- ~200 legal cards in the card pool (matching domain identity)
- Choose 39 cards + 1 champion (from ~5-10 champion options)
- Choose 12 runes from ~6-12 legal runes
- Choose 3 battlefields from ~20-30 legal battlefields

The combinatorial space: C(200, 39) ≈ 10^40. Exhaustive search is impossible. Must use heuristic optimization.

## 4. Architecture

```
riftbound-deckbuilder (Python)
│
├── Card Pool Filter
│   ├── Load registry.json (787 cards)
│   ├── Load ban-list.csv
│   ├── Filter by legend's domain identity
│   └── Output: legal_cards[] for this legend
│
├── Deck Generator
│   ├── Random legal deck generation
│   ├── Crossover (swap card groups between two parent decks)
│   ├── Mutation (swap 1-3 cards, respecting constraints)
│   └── Output: DeckSubmission JSON files
│
├── Evaluator
│   ├── Calls: `riftbound --games N --threads T deck.json opponent.json -o results/`
│   ├── Parses win rate from stdout
│   ├── Batch evaluation: deck vs field of opponents
│   └── Output: fitness score (win rate)
│
├── Optimizer
│   ├── Phase 1: Random search (baseline)
│   ├── Phase 2: Evolutionary algorithm
│   ├── Phase 3: Bayesian optimization
│   └── Output: ranked deck lists
│
└── Output
    ├── Ranked decks as JSON (compatible with engine CLI)
    ├── Win rate matrix (deck × opponent)
    └── Card frequency analysis (which cards appear in top decks)
```

## 5. Approach Phases

### Phase 1: Random Baseline

1. For a target legend, generate 100 random legal decks
2. Evaluate each deck: 50 games vs each of 5 opponent decks = 250 games per deck
3. Total: 25,000 games (~2 minutes at 200 games/sec)
4. Rank by overall win rate
5. Save top 10 decks as baseline
6. Output: "best random deck for Legend X has Y% win rate"

### Phase 2: Evolutionary Algorithm

1. **Initial population**: top 20 decks from Phase 1
2. **Crossover**: take two parent decks, randomly assign each card slot to parent A or B. Fix constraint violations by swapping illegal cards.
3. **Mutation**: with probability 0.1, swap 1-3 cards for random legal alternatives
4. **Evaluation**: each child plays 100 games vs the field
5. **Selection**: keep top 20 by win rate, discard bottom
6. **Iterate**: 50-100 generations
7. Total: 20 children × 100 games × 100 generations = 200K games (~17 minutes)

### Phase 3: Trained Agent Evaluation

1. Replace RandomAgent opponents with trained ModelAgents from Phase 2 of ML training
2. Re-run evolutionary optimization — now finding decks that beat skilled play
3. This produces tournament-quality decks, not just random-beater decks

### Phase 4: Meta-Game Optimization

1. Given a field of known opponent decks (from tournament data), optimize for that field
2. Counter-deck generation: find decks that specifically beat the top meta decks
3. Rock-paper-scissors analysis: identify archetype triangles

## 6. Separate Application

`scripts/deckbuilder.py` — standalone Python script.

**Dependencies:** Only needs Python 3, json, subprocess, numpy (optional for Bayesian opt).

**Interface with engine:** Shells out to `./build/riftbound` binary:

```python
import subprocess, json

def evaluate_deck(deck_path, opponent_path, num_games=50, threads=4):
    result = subprocess.run(
        ['./build/riftbound', deck_path, opponent_path,
         '-r', 'cards/registry.json',
         '--games', str(num_games),
         '--threads', str(threads)],
        capture_output=True, text=True,
        env={'RIFTBOUND_ROOT': '.'}
    )
    # Parse "P1 wins: N" from stdout
    for line in result.stdout.split('\n'):
        if 'P1 wins:' in line:
            wins = int(line.strip().split(':')[1].strip())
            return wins / num_games
    return 0.0
```

**Deck generation:**

```python
def generate_random_deck(legend_id, card_pool, card_db, ban_set):
    """Generate a random tournament-legal deck for a given legend."""
    legend = card_db[legend_id]
    domains = legend['domains']

    # Filter legal cards
    legal = [c for c in card_pool
             if c['card_id'] not in ban_set
             and any(d in domains for d in c.get('domains', []))
             and c['card_type'] in ('unit', 'spell', 'gear')]

    # Select champion (must match legend's tags)
    champions = [c for c in legal if c.get('super_type') == 'champion']
    chosen_champ = random.choice(champions)

    # Select 39 main deck cards (respecting copy limits)
    # ... constraint-aware random selection ...

    # Select 12 runes matching domains
    # Select 3 unique battlefields

    return DeckSubmission(legend_id, chosen_champ['card_id'], main_deck, ...)
```

## 7. Output Format

Decks are saved as JSON files compatible with the engine CLI:

```json
{
    "player_name": "DeckBuilder_MissFortune_Gen42",
    "legend": 756,
    "chosen_champion": 734,
    "main_deck": [
        {"card_id": 484, "name": "Deathgrip", "feature_vector": [...]},
        ...
    ],
    "rune_deck": [...],
    "battlefields": [...],
    "sideboard": [],
    "encoding_metadata": {...}
}
```

## 8. Card Frequency Analysis

After optimization, analyze which cards appear most often in top-performing decks:

```
Legend: Miss Fortune (Fury/Chaos)
Top 10 decks average 72% win rate vs field

Card frequency in top 10:
  Deathgrip (spell):     10/10 decks (100%)
  Hextech Ray (spell):    9/10 decks (90%)
  Ruined Rex (unit):      8/10 decks (80%)
  Shadow's Call (spell):  7/10 decks (70%)
  ...

Cards never in top decks:
  Noxus Hopeful (unit):   0/10 decks
  ...
```

This reveals the meta: which cards are staples, which are flex slots, which are traps.

## 9. Integration with Ban List

The deck builder loads `cards/ban-list.csv` at startup and excludes banned cards from the legal card pool. If a card is banned mid-season, existing decks containing it are automatically flagged as illegal when re-validated.

```python
def load_ban_list(csv_path):
    banned = set()
    with open(csv_path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            # Format: SET,ID,'DISPLAY NAME'
            # Single quotes around name because some have commas
            parts = line.split(',', 2)  # split on first 2 commas only
            if len(parts) >= 3:
                display_name = parts[2].strip("'")
                banned.add(display_name)
    return banned
```

## 10. Future: Legend-Specific Deck Templates

Once the play agent is trained per-legend, the deck builder can use archetype templates:

- **Aggro template**: maximize low-cost units, include direct damage spells
- **Control template**: maximize removal, include high-cost finishers
- **Equipment template**: include Weaponmaster units + Equipment gear package
- **Token template**: include token generators + buff cards

Templates reduce the search space and can be mixed/matched per legend's strengths.
