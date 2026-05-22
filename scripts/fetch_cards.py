#!/usr/bin/env python3
"""
Fetch all Riftbound cards from the official card gallery and save them
as individual JSON files in a format consumable by the C++ engine.

Source: https://riftbound.leagueoflegends.com/en-us/card-gallery/
Data is embedded as __NEXT_DATA__ JSON in the gallery page.

Output format per card (engine-ready):
{
    "id": "ogn-001-298",
    "name": "Blazing Scorcher",
    "set": "OGN",
    "set_name": "Origins",
    "collector_number": 1,
    "public_code": "OGN-001/298",
    "card_type": "unit",
    "super_type": null,           // "champion", "signature", "token", or null
    "domains": ["fury"],
    "tags": ["Dragon", "Noxus"],
    "energy_cost": 5,
    "power_cost": 0,
    "might": 5,
    "might_bonus": null,
    "rarity": "common",
    "ability_text": "[Accelerate] (You may pay [1][C] as an additional cost...)",
    "effect_text": null,          // gear effect text (when attached)
    "image_url": "https://...",
    "artist": "Envar Studio"
}
"""

import json
import os
import re
import sys
import urllib.request
from html.parser import HTMLParser
from pathlib import Path


GALLERY_URL = "https://riftbound.leagueoflegends.com/en-us/card-gallery/"
OUTPUT_DIR = Path(__file__).resolve().parent.parent / "cards" / "json"

# Symbols found in ability HTML that map to engine shorthands
SYMBOL_MAP = {
    ":rb_energy_1:": "[1]",
    ":rb_energy_2:": "[2]",
    ":rb_energy_3:": "[3]",
    ":rb_energy_4:": "[4]",
    ":rb_energy_5:": "[5]",
    ":rb_energy_6:": "[6]",
    ":rb_energy_7:": "[7]",
    ":rb_energy_8:": "[8]",
    ":rb_energy_9:": "[9]",
    ":rb_energy_10:": "[10]",
    ":rb_rune_fury:": "[R]",
    ":rb_rune_calm:": "[G]",
    ":rb_rune_mind:": "[B]",
    ":rb_rune_body:": "[O]",
    ":rb_rune_chaos:": "[P]",
    ":rb_rune_order:": "[Y]",
    ":rb_rune_rainbow:": "[A]",
    ":rb_might:": "[M]",
    ":rb_exhaust:": "[E]",
    ":rb_arrow:": "[>]",
}


class HTMLTextExtractor(HTMLParser):
    """Strip HTML tags, converting <br> and <p> to newlines."""

    def __init__(self):
        super().__init__()
        self.result = []
        self._in_italic = False

    def handle_starttag(self, tag, attrs):
        if tag == "br":
            self.result.append("\n")
        elif tag in ("em", "i"):
            self._in_italic = True

    def handle_endtag(self, tag):
        if tag == "p":
            self.result.append("\n")
        elif tag in ("em", "i"):
            self._in_italic = False

    def handle_data(self, data):
        self.result.append(data)

    def get_text(self):
        return "".join(self.result).strip()


def html_to_text(html_body: str) -> str:
    """Convert HTML ability text to clean plaintext with engine symbols."""
    if not html_body:
        return ""

    # Replace CMS symbols with engine shorthands
    text = html_body
    for symbol, shorthand in SYMBOL_MAP.items():
        text = text.replace(symbol, shorthand)

    # Strip any remaining :rb_...: symbols we don't know about
    text = re.sub(r":rb_[a-z0-9_]+:", "", text)

    # Parse HTML to plaintext
    extractor = HTMLTextExtractor()
    extractor.feed(text)
    result = extractor.get_text()

    # Normalize whitespace
    result = re.sub(r"\n{3,}", "\n\n", result)
    result = re.sub(r" +", " ", result)

    return result.strip()


def build_champion_tag_set(raw_cards: list[dict]) -> set[str]:
    """Collect all champion tags by inspecting legend cards.

    Legends have tags that correspond to champion names (Jinx, Darius, etc.).
    These tags are used to identify champion units and signature cards.
    """
    champion_tags = set()
    for card in raw_cards:
        type_ids = [t["id"] for t in card.get("cardType", {}).get("type", [])]
        if "legend" in type_ids:
            tags = card.get("tags", {})
            if isinstance(tags, dict):
                tags = tags.get("tags", [])
            for tag in tags:
                champion_tags.add(tag)
    return champion_tags


def detect_super_type(raw_card: dict, champion_tags: set[str]) -> str | None:
    """Determine super_type using naming convention and champion tag cross-reference.

    The gallery data doesn't distinguish champion_unit from unit, or
    signature_spell from spell. We detect them via:
    - Champion unit: a unit with 'Name, Title' format (comma in name).
      This is a universal convention — regular units never use this format.
      Not all champion characters have legends in the current card pool,
      so we can't rely solely on legend tag matching.
    - Legend: always gets "champion" super_type.
    - Signature card: a non-legend, non-champion card that has a tag
      matching a legend's champion tag (e.g., "Jinx" tag on a spell).
    """
    type_ids = [t["id"] for t in raw_card.get("cardType", {}).get("type", [])]
    card_tags = raw_card.get("tags", {})
    if isinstance(card_tags, dict):
        card_tags = card_tags.get("tags", [])
    elif not isinstance(card_tags, list):
        card_tags = []

    has_champion_tag = any(t in champion_tags for t in card_tags)

    if "legend" in type_ids:
        # Legends are their own card type, not champions.
        # They share champion tags with champion units but are functionally distinct.
        return None

    if "unit" in type_ids and "," in raw_card.get("name", ""):
        # All "Name, Title" units are champion units
        return "champion"

    if has_champion_tag and "unit" not in type_ids:
        # Non-unit with a champion tag = signature spell/gear
        return "signature"

    if has_champion_tag and "unit" in type_ids and "," not in raw_card.get("name", ""):
        # Unit with a champion tag but no comma = signature unit (e.g., Daisy!)
        return "signature"

    return None


def normalize_card_type(raw_card: dict) -> str:
    """Extract the base card type from the raw data."""
    type_ids = [t["id"] for t in raw_card.get("cardType", {}).get("type", [])]

    # Map gallery type IDs to engine types
    type_map = {
        "unit": "unit",
        "spell": "spell",
        "gear": "gear",
        "rune": "rune",
        "battlefield": "battlefield",
        "legend": "legend",
    }

    for tid in type_ids:
        # Handle compound IDs like "champion_unit" -> "unit"
        base = tid.replace("champion_", "").replace("signature_", "")
        if base in type_map:
            return type_map[base]

    return type_ids[0] if type_ids else "unknown"


def transform_card(raw: dict, champion_tags: set[str]) -> dict:
    """Transform a raw gallery card object into engine-ready format."""

    # Card type
    card_type = normalize_card_type(raw)
    super_type = detect_super_type(raw, champion_tags)

    # Domains
    domains = []
    for d in raw.get("domain", {}).get("values", []):
        did = d.get("id", "")
        if did and did != "colorless":
            domains.append(did)

    # Tags
    tags = raw.get("tags", {})
    if isinstance(tags, dict):
        tags = tags.get("tags", [])
    elif not isinstance(tags, list):
        tags = []

    # Costs
    energy_cost = None
    energy_val = raw.get("energy", {})
    if energy_val and isinstance(energy_val, dict):
        ev = energy_val.get("value", {})
        if ev:
            energy_cost = ev.get("id")
            if isinstance(energy_cost, str) and energy_cost.isdigit():
                energy_cost = int(energy_cost)

    power_cost = None
    power_val = raw.get("power", {})
    if power_val and isinstance(power_val, dict):
        pv = power_val.get("value", {})
        if pv:
            power_cost = pv.get("id")
            if isinstance(power_cost, str) and power_cost.isdigit():
                power_cost = int(power_cost)

    # Might
    might = None
    might_val = raw.get("might", {})
    if might_val and isinstance(might_val, dict):
        mv = might_val.get("value", {})
        if mv:
            might = mv.get("id")
            if isinstance(might, str) and might.isdigit():
                might = int(might)

    # Might Bonus (for gear)
    might_bonus = None
    mb_val = raw.get("mightBonus", {})
    if mb_val and isinstance(mb_val, dict):
        mbv = mb_val.get("value", {})
        if mbv:
            might_bonus = mbv.get("id")
            if isinstance(might_bonus, str):
                # Strip + sign if present
                might_bonus = int(might_bonus.lstrip("+"))
            elif isinstance(might_bonus, int):
                pass

    # Ability text
    ability_text = None
    text_val = raw.get("text", {})
    if text_val and isinstance(text_val, dict):
        rt = text_val.get("richText", {})
        if isinstance(rt, dict):
            body = rt.get("body", "")
            ability_text = html_to_text(body)

    # Effect text (for gear with attached effects)
    effect_text = None
    effect_val = raw.get("effect", {})
    if effect_val and isinstance(effect_val, dict):
        rt = effect_val.get("richText", {})
        if isinstance(rt, dict):
            body = rt.get("body", "")
            effect_text = html_to_text(body)

    # Rarity
    rarity = raw.get("rarity", {}).get("value", {}).get("id", "unknown")

    # Image URL
    image_url = None
    ci = raw.get("cardImage", {})
    if ci and isinstance(ci, dict):
        image_url = ci.get("url", "")
        # Strip query params for cleaner URL
        if image_url and "?" in image_url:
            image_url = image_url.split("?")[0]

    # Artist
    artist = None
    ill = raw.get("illustrator", {})
    if ill and isinstance(ill, dict):
        values = ill.get("values", [])
        if values:
            artist = values[0].get("label", "")

    # Set
    set_val = raw.get("set", {}).get("value", {})
    set_id = set_val.get("id", "")
    set_name = set_val.get("label", "")

    return {
        "id": raw.get("id", ""),
        "name": raw.get("name", ""),
        "set": set_id,
        "set_name": set_name,
        "collector_number": raw.get("collectorNumber"),
        "public_code": raw.get("publicCode", ""),
        "card_type": card_type,
        "super_type": super_type,
        "domains": domains,
        "tags": tags,
        "energy_cost": energy_cost,
        "power_cost": power_cost,
        "might": might,
        "might_bonus": might_bonus,
        "rarity": rarity,
        "ability_text": ability_text,
        "effect_text": effect_text,
        "image_url": image_url,
        "artist": artist,
    }


def fetch_gallery_html(url: str) -> str:
    """Download the card gallery page."""
    print(f"Fetching {url} ...")
    req = urllib.request.Request(url, headers={"User-Agent": "RiftboundEngine/1.0"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return resp.read().decode("utf-8")


def extract_cards_from_html(html: str) -> list[dict]:
    """Extract raw card objects from the __NEXT_DATA__ JSON in the page."""
    match = re.search(
        r'<script id="__NEXT_DATA__" type="application/json">(.*?)</script>',
        html,
        re.DOTALL,
    )
    if not match:
        raise RuntimeError("Could not find __NEXT_DATA__ in page source")

    data = json.loads(match.group(1))
    blades = data["props"]["pageProps"]["page"]["blades"]

    # The card data is in a blade with a 'cards' key containing 'items'
    for blade in blades:
        if isinstance(blade, dict) and "cards" in blade:
            items = blade["cards"].get("items", [])
            if items:
                return items

    raise RuntimeError("Could not find card items in page data")


def is_duplicate_art(card: dict, seen_names: set[tuple[str, str]]) -> bool:
    """Check if this is a showcase/alt-art variant (same card, different art).

    Detection methods:
    1. ID contains a letter suffix like 'ogn-030a-298' (showcase)
    2. ID contains 'star' like 'unl-230-star-219' (foil/star variant)
    3. Same name+set combo already seen (catches remaining dupes)
    """
    card_id = card.get("id", "")
    name = card.get("name", "")
    set_id = card.get("set", {}).get("value", {}).get("id", "")

    # Check ID patterns for known alt-art formats
    if re.match(r"^[a-z]+-\d+[a-z]-\d+$", card_id):
        return True
    if "star" in card_id:
        return True

    # Deduplicate by name+set
    key = (name, set_id)
    if key in seen_names:
        return True
    seen_names.add(key)

    return False


def main():
    # Allow using cached HTML for development
    cache_path = Path("/tmp/riftbound-gallery.html")
    if cache_path.exists() and "--no-cache" not in sys.argv:
        print(f"Using cached page from {cache_path}")
        html = cache_path.read_text()
    else:
        html = fetch_gallery_html(GALLERY_URL)
        cache_path.write_text(html)
        print(f"Cached page to {cache_path}")

    # Extract raw cards
    raw_cards = extract_cards_from_html(html)
    print(f"Found {len(raw_cards)} total card entries")

    # Build champion tag set from legends for super_type detection
    champion_tags = build_champion_tag_set(raw_cards)
    print(f"Found {len(champion_tags)} champion tags from legends: {sorted(champion_tags)}")

    # Save raw gallery data for reference/debugging
    raw_dir = OUTPUT_DIR.parent / "raw"
    raw_dir.mkdir(parents=True, exist_ok=True)
    with open(raw_dir / "gallery_raw.json", "w") as f:
        json.dump(raw_cards, f, indent=2)
    print(f"Saved raw gallery data to {raw_dir / 'gallery_raw.json'}")

    # Transform and save
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    saved = 0
    skipped_dupes = 0
    by_type = {}
    by_set = {}
    by_super = {}
    seen_names: set[tuple[str, str]] = set()

    for raw in raw_cards:
        card = transform_card(raw, champion_tags)

        # Skip alt-art duplicates — same gameplay card, different art
        if is_duplicate_art(raw, seen_names):
            skipped_dupes += 1
            continue

        # Build filename: {set}_{collector_number:03d}_{sanitized_name}.json
        safe_name = re.sub(r"[^a-zA-Z0-9]", "_", card["name"]).lower().strip("_")
        filename = f"{card['set'].lower()}_{card['collector_number']:03d}_{safe_name}.json"

        filepath = OUTPUT_DIR / filename
        with open(filepath, "w") as f:
            json.dump(card, f, indent=2)

        saved += 1

        # Track stats
        ct = card["card_type"]
        by_type[ct] = by_type.get(ct, 0) + 1
        st = card["set"]
        by_set[st] = by_set.get(st, 0) + 1
        sp = card["super_type"] or "none"
        by_super[sp] = by_super.get(sp, 0) + 1

    print(f"\nSaved {saved} unique cards to {OUTPUT_DIR}")
    print(f"Skipped {skipped_dupes} alt-art duplicates")
    print(f"\nBy type: {json.dumps(by_type, indent=2)}")
    print(f"\nBy set: {json.dumps(by_set, indent=2)}")
    print(f"\nBy super_type: {json.dumps(by_super, indent=2)}")

    # Also save a combined index for quick loading
    index_path = OUTPUT_DIR.parent / "card_index.json"
    all_cards = []
    seen_for_index: set[tuple[str, str]] = set()
    for raw in raw_cards:
        if not is_duplicate_art(raw, seen_for_index):
            all_cards.append(transform_card(raw, champion_tags))

    with open(index_path, "w") as f:
        json.dump(all_cards, f, indent=2)
    print(f"\nSaved combined index ({len(all_cards)} cards) to {index_path}")


if __name__ == "__main__":
    main()
