#!/usr/bin/env python3
"""
Deck Import & Validation — converts deck lists into engine-ready format.

Accepts deck lists in a simple text format and validates them against
tournament rules, outputting a JSON deck file the engine can consume.

Input format (Piltover Archive export style):
  Legend:
  1 Loose Cannon

  Champion:
  1 Jinx, Demolitionist

  MainDeck:
  3 Pouty Poro
  3 Legion Rearguard
  2 Hextech Ray
  ...

  Battlefields:
  1 Reaver's Row
  1 Void Gate
  1 Scrapheap

  Runes:
  6 Fury Rune
  6 Calm Rune

  Sideboard:
  2 Thermo Beam
  1 Blind Fury
  ...

Output: JSON deck file with card_ids from the registry, ready for the engine.
"""

import json
import re
import sys
from pathlib import Path

CARDS_DIR = Path(__file__).resolve().parent.parent / "cards"
REGISTRY_PATH = CARDS_DIR / "registry.json"
DECKS_DIR = Path(__file__).resolve().parent.parent / "decks"


class DeckValidationError(Exception):
    pass


class CardRegistry:
    """Lookup cards by name from the registry."""

    def __init__(self, registry_path: str | Path = REGISTRY_PATH):
        with open(registry_path) as f:
            self.registry = json.load(f)

        # Build lookups
        self._by_id: dict[int, dict] = {}
        self._by_name: dict[str, list[dict]] = {}

        for card in self.registry["cards"]:
            self._by_id[card["card_id"]] = card
            name = card["name"].lower()
            self._by_name.setdefault(name, []).append(card)

        self.encoding = self.registry["encoding_tables"]

    def find_by_name(self, name: str, card_type: str | None = None, set_code: str | None = None) -> dict:
        """Find a card by name, optionally filtering by type and set.

        Handles legend name normalization: Piltover Archive exports legends as
        "Champion, Title" (e.g., "Vex, Gloomist") but they're stored as just
        the title ("Gloomist") in our database. When looking up a legend and
        the full name fails, we try the title portion after the comma.
        """
        candidates = self._by_name.get(name.lower(), [])

        if not candidates:
            # For legends: try just the title part after the comma
            # e.g., "Vex, Gloomist" -> "Gloomist", "LeBlanc, Deceiver" -> "Deceiver"
            if "," in name:
                title = name.split(",", 1)[1].strip()
                candidates = self._by_name.get(title.lower(), [])

        if not candidates:
            # Try substring match as fallback
            for key, cards in self._by_name.items():
                if name.lower() in key or key in name.lower():
                    candidates.extend(cards)

        if not candidates:
            raise DeckValidationError(f"Card not found: '{name}'")

        if card_type:
            filtered = [c for c in candidates if c["card_type"] == card_type]
            if filtered:
                candidates = filtered

        if set_code:
            filtered = [c for c in candidates if c["set"] == set_code]
            if filtered:
                candidates = filtered

        # If multiple matches remain, prefer by set priority (newer sets first)
        set_priority = {"UNL": 0, "SFD": 1, "OGN": 2, "OGS": 3}
        candidates.sort(key=lambda c: set_priority.get(c["set"], 99))

        return candidates[0]

    def get_by_id(self, card_id: int) -> dict:
        return self._by_id[card_id]

    @property
    def feature_vector_size(self) -> int:
        return self.registry["feature_vector_size"]


def parse_deck_list(text: str, registry: CardRegistry) -> dict:
    """Parse a Piltover Archive export-style deck list.

    Format uses section headers (Legend:, Champion:, MainDeck:, Battlefields:,
    Runes:, Sideboard:) followed by lines of "N CardName".

    Returns:
        {
            "legend": card_id,
            "chosen_champion": card_id,
            "main_deck": [card_id, ...],   # 39 cards (40 minus champion)
            "rune_deck": [card_id, ...],    # 12 runes
            "battlefields": [card_id, ...], # 3 battlefields
            "sideboard": [card_id, ...],    # 0-8 cards
        }
    """
    legend = None
    champion = None
    battlefields = []
    runes = []
    main_deck = []
    sideboard = []

    # Normalize section headers
    section_map = {
        "legend": "legend",
        "champion": "champion",
        "maindeck": "maindeck",
        "main deck": "maindeck",
        "mainboard": "maindeck",
        "battlefields": "battlefields",
        "battlefield": "battlefields",
        "runes": "runes",
        "rune": "runes",
        "sideboard": "sideboard",
        "side": "sideboard",
    }

    current_section = None

    for raw_line in text.strip().split("\n"):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue

        # Check for section header (word(s) followed by colon, nothing else meaningful after)
        header_match = re.match(r"^([A-Za-z ]+)\s*:\s*$", line)
        if header_match:
            header = header_match.group(1).strip().lower()
            if header in section_map:
                current_section = section_map[header]
                continue

        # Parse card line: "N CardName" or "Nx CardName"
        card_match = re.match(r"^(\d+)x?\s+(.+)$", line)
        if not card_match:
            # Try bare card name (count=1)
            card_match = re.match(r"^(.+)$", line)
            if card_match:
                count = 1
                name = card_match.group(1).strip()
            else:
                continue
        else:
            count = int(card_match.group(1))
            name = card_match.group(2).strip()

        # Route to appropriate section
        if current_section == "legend":
            card = registry.find_by_name(name, card_type="legend")
            legend = card["card_id"]

        elif current_section == "champion":
            card = registry.find_by_name(name, card_type="unit")
            champion = card["card_id"]

        elif current_section == "battlefields":
            card = registry.find_by_name(name, card_type="battlefield")
            for _ in range(count):
                battlefields.append(card["card_id"])

        elif current_section == "runes":
            card = registry.find_by_name(name, card_type="rune")
            runes.extend([card["card_id"]] * count)

        elif current_section == "sideboard":
            card = registry.find_by_name(name)
            sideboard.extend([card["card_id"]] * count)

        elif current_section == "maindeck":
            card = registry.find_by_name(name)
            main_deck.extend([card["card_id"]] * count)

        else:
            # No section header yet — try to infer from card type
            card = registry.find_by_name(name)
            if card["card_type"] == "legend":
                legend = card["card_id"]
            elif card["card_type"] == "rune":
                runes.extend([card["card_id"]] * count)
            elif card["card_type"] == "battlefield":
                battlefields.append(card["card_id"])
            else:
                main_deck.extend([card["card_id"]] * count)

    return {
        "legend": legend,
        "chosen_champion": champion,
        "main_deck": main_deck,
        "rune_deck": runes,
        "battlefields": battlefields,
        "sideboard": sideboard,
    }


def validate_deck(deck: dict, registry: CardRegistry) -> list[str]:
    """Validate a deck submission against tournament rules.

    Returns a list of error strings. Empty list = valid.
    """
    errors = []

    # --- Legend ---
    if deck["legend"] is None:
        errors.append("Missing legend")
        return errors  # Can't validate domain identity without legend

    legend = registry.get_by_id(deck["legend"])
    if legend["card_type"] != "legend":
        errors.append(f"Legend slot contains non-legend card: {legend['name']}")
        return errors

    legend_domains = set(legend.get("domains", []))
    legend_tags = set(legend.get("tags", []))

    # --- Chosen Champion ---
    if deck["chosen_champion"] is None:
        errors.append("Missing chosen champion")
    else:
        champ = registry.get_by_id(deck["chosen_champion"])
        if champ["card_type"] != "unit":
            errors.append(f"Champion slot contains non-unit: {champ['name']}")
        elif champ.get("super_type") != "champion":
            errors.append(f"Champion slot contains non-champion unit: {champ['name']}")
        else:
            # Champion must share a tag with the legend
            champ_tags = set(champ.get("tags", []))
            if not champ_tags & legend_tags:
                errors.append(
                    f"Chosen champion '{champ['name']}' (tags: {champ_tags}) "
                    f"doesn't share a champion tag with legend '{legend['name']}' (tags: {legend_tags})"
                )

    # --- Main Deck Size ---
    # Main deck should be 39 cards + champion = 40 total
    total_main = len(deck["main_deck"]) + (1 if deck["chosen_champion"] else 0)
    if total_main != 40:
        errors.append(f"Main deck must be exactly 40 cards (including champion), got {total_main}")

    # --- Rune Deck ---
    if len(deck["rune_deck"]) != 12:
        errors.append(f"Rune deck must be exactly 12 runes, got {len(deck['rune_deck'])}")

    # --- Battlefields ---
    if len(deck["battlefields"]) != 3:
        errors.append(f"Must have exactly 3 battlefields, got {len(deck['battlefields'])}")

    # Battlefield names must be unique
    bf_names = [registry.get_by_id(bf)["name"] for bf in deck["battlefields"]]
    if len(bf_names) != len(set(bf_names)):
        errors.append(f"Battlefield names must be unique, got: {bf_names}")

    # --- Sideboard ---
    if len(deck["sideboard"]) > 8:
        errors.append(f"Sideboard can have at most 8 cards, got {len(deck['sideboard'])}")

    # --- Domain Identity ---
    all_main_ids = deck["main_deck"] + ([deck["chosen_champion"]] if deck["chosen_champion"] else [])
    for card_id in all_main_ids:
        card = registry.get_by_id(card_id)
        card_domains = set(card.get("domains", []))
        if card_domains and not card_domains.issubset(legend_domains):
            errors.append(
                f"Card '{card['name']}' domains {card_domains} "
                f"not within legend domain identity {legend_domains}"
            )

    for card_id in deck["sideboard"]:
        card = registry.get_by_id(card_id)
        card_domains = set(card.get("domains", []))
        if card_domains and not card_domains.issubset(legend_domains):
            errors.append(
                f"Sideboard card '{card['name']}' domains {card_domains} "
                f"not within legend domain identity {legend_domains}"
            )

    # Rune domain check
    for card_id in deck["rune_deck"]:
        rune = registry.get_by_id(card_id)
        rune_domains = set(rune.get("domains", []))
        if rune_domains and not rune_domains.issubset(legend_domains):
            errors.append(
                f"Rune '{rune['name']}' domain {rune_domains} "
                f"not within legend domain identity {legend_domains}"
            )

    # --- Copy Limits ---
    # Max 3 copies of any named card across main deck + sideboard
    name_counts: dict[str, int] = {}
    for card_id in all_main_ids + deck["sideboard"]:
        card = registry.get_by_id(card_id)
        name = card["name"]
        name_counts[name] = name_counts.get(name, 0) + 1

    for name, count in name_counts.items():
        if count > 3:
            errors.append(f"Too many copies of '{name}': {count} (max 3 across main + sideboard)")

    # --- Signature Limits ---
    # Max 3 total signature cards, all must share legend's champion tag
    sig_count = 0
    for card_id in all_main_ids + deck["sideboard"]:
        card = registry.get_by_id(card_id)
        if card.get("super_type") == "signature":
            sig_count += 1
            card_tags = set(card.get("tags", []))
            if not card_tags & legend_tags:
                errors.append(
                    f"Signature card '{card['name']}' tag {card_tags} "
                    f"doesn't match legend champion tag {legend_tags}"
                )

    if sig_count > 3:
        errors.append(f"Too many signature cards: {sig_count} (max 3)")

    return errors


def deck_to_engine_format(deck: dict, registry: CardRegistry, player_name: str = "Player") -> dict:
    """Convert a validated deck submission into the engine-consumable format.

    This is what the C++ engine loads. All values are integers suitable
    for tensor conversion.
    """
    # Build feature vectors for all cards in the deck
    def card_entry(card_id: int) -> dict:
        card = registry.get_by_id(card_id)
        return {
            "card_id": card_id,
            "name": card["name"],
            "feature_vector": card["feature_vector"],
        }

    return {
        "player_name": player_name,
        "legend": card_entry(deck["legend"]),
        "chosen_champion": card_entry(deck["chosen_champion"]),
        "main_deck": [card_entry(cid) for cid in deck["main_deck"]],
        "rune_deck": [card_entry(cid) for cid in deck["rune_deck"]],
        "battlefields": [card_entry(cid) for cid in deck["battlefields"]],
        "sideboard": [card_entry(cid) for cid in deck["sideboard"]],
        "encoding_metadata": {
            "feature_vector_size": registry.feature_vector_size,
            "pad_id": 0,
        },
    }


def main():
    if len(sys.argv) < 2:
        print("Usage: python deck_import.py <decklist.txt> [output.json]")
        print("       python deck_import.py --example  (print example deck list format)")
        sys.exit(1)

    if sys.argv[1] == "--example":
        print("""# Example Riftbound Deck List (Piltover Archive export format)
# Lines starting with # are comments

Legend:
1 Loose Cannon

Champion:
1 Jinx, Demolitionist

MainDeck:
3 Pouty Poro
3 Legion Rearguard
3 Chemtech Enforcer
3 Noxus Hopeful
3 Flame Chompers
3 Blazing Scorcher
2 Hextech Ray
2 Cleave
2 Disintegrate
2 Dangerous Duo
2 Scrapyard Champion
2 Get Excited!
2 Void Seeker
2 Captain Farron
2 Sky Splitter
2 Iron Ballista
2 Noxus Saboteur

Battlefields:
1 Reaver's Row
1 Void Gate
1 Scrapheap

Runes:
12 Fury Rune

Sideboard:
2 Thermo Beam
2 Blind Fury
2 Falling Star
2 Raging Soul""")
        sys.exit(0)

    deck_path = Path(sys.argv[1])
    if not deck_path.exists():
        print(f"Error: File not found: {deck_path}")
        sys.exit(1)

    output_path = Path(sys.argv[2]) if len(sys.argv) > 2 else None

    # Load registry
    if not REGISTRY_PATH.exists():
        print("Error: Card registry not found. Run card_registry.py first.")
        sys.exit(1)

    registry = CardRegistry()
    print(f"Loaded registry: {registry.registry['num_cards']} cards")

    # Parse deck
    deck_text = deck_path.read_text()
    try:
        deck = parse_deck_list(deck_text, registry)
    except DeckValidationError as e:
        print(f"Parse error: {e}")
        sys.exit(1)

    # Validate
    errors = validate_deck(deck, registry)
    if errors:
        print(f"Deck validation FAILED ({len(errors)} errors):")
        for e in errors:
            print(f"  - {e}")
        sys.exit(1)

    print("Deck validation PASSED")

    # Convert to engine format
    player_name = deck_path.stem
    engine_deck = deck_to_engine_format(deck, registry, player_name)

    # Summary
    legend = registry.get_by_id(deck["legend"])
    champ = registry.get_by_id(deck["chosen_champion"])
    print(f"  Legend: {legend['name']} (domains: {legend['domains']})")
    print(f"  Champion: {champ['name']}")
    print(f"  Main Deck: {len(deck['main_deck'])} cards + champion = 40")
    print(f"  Rune Deck: {len(deck['rune_deck'])} runes")
    print(f"  Battlefields: {[registry.get_by_id(bf)['name'] for bf in deck['battlefields']]}")
    print(f"  Sideboard: {len(deck['sideboard'])} cards")
    print(f"  Feature vector size: {registry.feature_vector_size}")

    # Output
    if output_path:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, "w") as f:
            json.dump(engine_deck, f, indent=2)
        print(f"  Saved to: {output_path}")
    else:
        DECKS_DIR.mkdir(parents=True, exist_ok=True)
        default_output = DECKS_DIR / f"{player_name}.json"
        with open(default_output, "w") as f:
            json.dump(engine_deck, f, indent=2)
        print(f"  Saved to: {default_output}")


if __name__ == "__main__":
    main()
