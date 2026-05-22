#!/usr/bin/env python3
"""
Card Registry — assigns stable integer IDs and builds feature vectors for all cards.

This is the single source of truth for card-to-integer mappings used by:
  - The C++ engine (loads the registry to map card IDs)
  - The deck importer (references cards by integer ID)
  - The ML pipeline (card embeddings indexed by these IDs)

Output:
  cards/registry.json — complete registry with IDs, features, and encoding tables

Encoding philosophy:
  - Every card gets a unique integer ID (0-based, stable across runs via deterministic sort)
  - Categorical values (domains, tags, keywords, card_type) get their own integer mappings
  - Card features are a fixed-size numeric vector suitable for direct tensor conversion
  - ID 0 is reserved as PAD/MASK token for sequence padding
"""

import json
import re
from pathlib import Path

CARDS_DIR = Path(__file__).resolve().parent.parent / "cards"
INDEX_PATH = CARDS_DIR / "card_index.json"
REGISTRY_PATH = CARDS_DIR / "registry.json"

# ─── Categorical encoding tables ───
# Deterministic ordering. New values append to the end to preserve stability.

CARD_TYPES = ["unit", "spell", "gear", "rune", "battlefield", "legend"]

SUPER_TYPES = ["none", "champion", "signature"]

DOMAINS = ["fury", "calm", "mind", "body", "chaos", "order"]

# Keywords the engine cares about (from core rules 800+ section)
KEYWORDS = [
    "Accelerate", "Action", "Ambush", "Assault", "Backline",
    "Deathknell", "Deflect", "Equip", "Ganking", "Hidden",
    "Hunt", "Legion", "Level", "Predict", "Quick-Draw",
    "Reaction", "Repeat", "Shield", "Tank", "Temporary",
    "Unique", "Vision", "Weaponmaster",
]

# Tags — sorted for determinism. Champion tags and region tags mixed.
# Built dynamically from the card data.

# ─── Feature extraction ───

def extract_keywords_from_text(ability_text: str) -> list[str]:
    """Extract keyword names from ability text brackets like [Accelerate], [Assault 2]."""
    if not ability_text:
        return []
    # Match [Keyword] or [Keyword N] patterns
    raw = re.findall(r'\[([A-Z][a-zA-Z\-]+(?:\s+\d+)?)\]', ability_text)
    keywords = []
    for kw in raw:
        # Strip numeric parameter: "Assault 2" -> "Assault"
        base = re.sub(r'\s+\d+$', '', kw)
        if base in KEYWORDS:
            keywords.append(base)
    return list(set(keywords))


def extract_keyword_value(ability_text: str, keyword: str) -> int:
    """Extract the numeric value of a parameterized keyword (e.g., Assault 2 -> 2)."""
    if not ability_text:
        return 0
    match = re.search(rf'\[{keyword}\s+(\d+)\]', ability_text)
    if match:
        return int(match.group(1))
    # Check for keyword without number (default value is 1)
    if f'[{keyword}]' in ability_text:
        return 1
    return 0


def build_card_features(card: dict, tag_to_idx: dict[str, int], num_tags: int) -> dict:
    """Build a fixed-size feature dictionary for a single card.

    Feature vector layout:
      [0]     card_type        (int, 0-5)
      [1]     super_type       (int, 0-2)
      [2]     energy_cost      (int, 0-12, -1 if N/A)
      [3]     power_cost       (int, 0-4, -1 if N/A)
      [4]     might            (int, 0-12, -1 if N/A)
      [5]     might_bonus      (int, 0-5, -1 if N/A)
      [6-11]  domains          (6 binary flags: fury, calm, mind, body, chaos, order)
      [12-34] keywords         (23 binary flags, one per keyword)
      [35-37] keyword_values   (assault_val, shield_val, deflect_val — parameterized keywords)
      [38+]   tags             (N binary flags, one per unique tag)
    """
    text = card.get("ability_text") or ""

    features = {
        "card_type": CARD_TYPES.index(card["card_type"]),
        "super_type": SUPER_TYPES.index(card.get("super_type") or "none"),
        "energy_cost": card.get("energy_cost") if card.get("energy_cost") is not None else -1,
        "power_cost": card.get("power_cost") if card.get("power_cost") is not None else -1,
        "might": card.get("might") if card.get("might") is not None else -1,
        "might_bonus": card.get("might_bonus") if card.get("might_bonus") is not None else -1,

        # Domain flags
        "domains": [1 if d in card.get("domains", []) else 0 for d in DOMAINS],

        # Keyword flags
        "keywords": [1 if kw in extract_keywords_from_text(text) else 0 for kw in KEYWORDS],

        # Parameterized keyword values
        "assault_value": extract_keyword_value(text, "Assault"),
        "shield_value": extract_keyword_value(text, "Shield"),
        "deflect_value": extract_keyword_value(text, "Deflect"),
        "level_value": extract_keyword_value(text, "Level"),

        # Tag flags
        "tags": [1 if tag in card.get("tags", []) else 0 for tag in sorted(tag_to_idx.keys())],
    }

    return features


def features_to_vector(features: dict) -> list[float]:
    """Flatten a feature dict into a single numeric vector for tensor conversion."""
    vec = [
        float(features["card_type"]),
        float(features["super_type"]),
        float(features["energy_cost"]),
        float(features["power_cost"]),
        float(features["might"]),
        float(features["might_bonus"]),
    ]
    vec.extend(float(x) for x in features["domains"])
    vec.extend(float(x) for x in features["keywords"])
    vec.extend([
        float(features["assault_value"]),
        float(features["shield_value"]),
        float(features["deflect_value"]),
        float(features["level_value"]),
    ])
    vec.extend(float(x) for x in features["tags"])
    return vec


def main():
    with open(INDEX_PATH) as f:
        cards = json.load(f)

    # Sort deterministically: by set, then collector_number, then name
    cards.sort(key=lambda c: (c["set"], c.get("collector_number", 0), c["name"]))

    # Build tag encoding table from all cards
    all_tags = set()
    for c in cards:
        all_tags.update(c.get("tags", []))
    tag_list = sorted(all_tags)
    tag_to_idx = {tag: i for i, tag in enumerate(tag_list)}

    # Assign IDs (0 = PAD/MASK, cards start at 1)
    registry_entries = []
    for i, card in enumerate(cards):
        card_id = i + 1  # 0 reserved for PAD

        features = build_card_features(card, tag_to_idx, len(tag_list))
        feature_vector = features_to_vector(features)

        entry = {
            "card_id": card_id,
            "card_def_id": card["id"],       # original gallery ID like "ogn-001-298"
            "name": card["name"],
            "set": card["set"],
            "public_code": card["public_code"],
            "card_type": card["card_type"],
            "super_type": card.get("super_type"),
            "domains": card.get("domains", []),
            "tags": card.get("tags", []),
            "energy_cost": card.get("energy_cost"),
            "power_cost": card.get("power_cost"),
            "might": card.get("might"),
            "might_bonus": card.get("might_bonus"),
            "ability_text": card.get("ability_text"),
            "effect_text": card.get("effect_text"),
            "features": features,
            "feature_vector": feature_vector,
        }
        registry_entries.append(entry)

    # Build encoding tables
    encoding_tables = {
        "card_types": {ct: i for i, ct in enumerate(CARD_TYPES)},
        "super_types": {st: i for i, st in enumerate(SUPER_TYPES)},
        "domains": {d: i for i, d in enumerate(DOMAINS)},
        "keywords": {kw: i for i, kw in enumerate(KEYWORDS)},
        "tags": tag_to_idx,
        "feature_vector_layout": {
            "card_type": 0,
            "super_type": 1,
            "energy_cost": 2,
            "power_cost": 3,
            "might": 4,
            "might_bonus": 5,
            "domains_start": 6,
            "domains_end": 12,
            "keywords_start": 12,
            "keywords_end": 12 + len(KEYWORDS),
            "assault_value": 12 + len(KEYWORDS),
            "shield_value": 12 + len(KEYWORDS) + 1,
            "deflect_value": 12 + len(KEYWORDS) + 2,
            "level_value": 12 + len(KEYWORDS) + 3,
            "tags_start": 12 + len(KEYWORDS) + 4,
            "tags_end": 12 + len(KEYWORDS) + 4 + len(tag_list),
            "total_size": 12 + len(KEYWORDS) + 4 + len(tag_list),
        },
    }

    # Name-to-ID lookup (for deck imports)
    name_to_id = {}
    for entry in registry_entries:
        name = entry["name"]
        if name not in name_to_id:
            name_to_id[name] = []
        name_to_id[name].append({
            "card_id": entry["card_id"],
            "set": entry["set"],
            "card_type": entry["card_type"],
        })

        # Legends: add "<ChampionTag>, <PrintedName>" alias for human lookup.
        # e.g., "Gloomist" with tag "Vex" gets alias "Vex, Gloomist".
        # This is how players and deck tools refer to legends, even though
        # the printed card name is just the title.
        if entry["card_type"] == "legend" and entry.get("tags"):
            for tag in entry["tags"]:
                alias = f"{tag}, {name}"
                if alias not in name_to_id:
                    name_to_id[alias] = []
                name_to_id[alias].append({
                    "card_id": entry["card_id"],
                    "set": entry["set"],
                    "card_type": entry["card_type"],
                })

    registry = {
        "version": 1,
        "pad_id": 0,
        "num_cards": len(registry_entries),
        "feature_vector_size": encoding_tables["feature_vector_layout"]["total_size"],
        "encoding_tables": encoding_tables,
        "name_to_id": name_to_id,
        "cards": registry_entries,
    }

    with open(REGISTRY_PATH, "w") as f:
        json.dump(registry, f, indent=2)

    print(f"Registry built: {len(registry_entries)} cards")
    print(f"Feature vector size: {registry['feature_vector_size']}")
    print(f"PAD token ID: 0")
    print(f"Card IDs: 1-{len(registry_entries)}")
    print(f"Encoding tables: {list(encoding_tables.keys())}")
    print(f"Output: {REGISTRY_PATH}")


if __name__ == "__main__":
    main()
