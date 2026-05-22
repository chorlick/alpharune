#!/usr/bin/env python3
"""
Apply official errata to card JSON files.

This script patches card ability_text in cards/json/ and cards/card_index.json
with the corrected text from official errata documents.

Errata sources:
  - errata/Riftbound Card Errata 10-21-2025.pdf (Origins errata)
  - errata/UPDATED Riftbound Errata File 1-12-26.pdf (Origins + Spiritforged errata)
  - errata/unleased.txt (Spiritforged + Unleashed errata)

Run after fetch_cards.py to ensure cards are up to date.
"""

import json
import glob
import re
from pathlib import Path

CARDS_DIR = Path(__file__).resolve().parent.parent / "cards"
JSON_DIR = CARDS_DIR / "json"
INDEX_PATH = CARDS_DIR / "card_index.json"

# Each entry: (card_name, new_ability_text, new_effect_text_or_None)
# Text uses engine symbol shorthands: [1], [R], [G], [B], [O], [P], [Y], [A], [M], [E], [>], [C]
ERRATA: list[tuple[str, str, str | None]] = [
    # ===== Origins Errata (10-21-2025) =====
    (
        "Ava Achiever",
        "When I attack, you may pay [C] to play a card with [Hidden] from your hand, ignoring its cost. If it's a unit, play it here.",
        None,
    ),
    (
        "Baited Hook",
        "[1][C], [E]: Kill a friendly unit. Look at the top 5 cards of your Main Deck. You may banish a unit from among them that has Might up to 1 more than the killed unit and play it, ignoring its cost. Then recycle the rest.",
        None,
    ),
    (
        "Blind Fury",
        "[Action] (Play on your turn or in showdowns.)\nEach opponent reveals the top card of their Main Deck. Choose one and banish it, then play it, ignoring its cost. Then recycle the rest.",
        None,
    ),
    (
        "Clockwork Keeper",
        "You may pay [C] as an additional cost to play me.\nWhen you play me, if you paid the additional cost, draw 1.",
        None,
    ),
    (
        "Convergent Mutation",
        "[Reaction] (Play any time, even before spells and abilities resolve.)\nChoose a friendly unit. This turn, increase its Might to the Might of another friendly unit.",
        None,
    ),
    (
        "Dark Child",  # This is the starter legend "Dark Child, Starter" but stored as "Dark Child"
        "At the end of your turn, ready up to 2 runes.",
        None,
    ),
    (
        "Dazzling Aurora",
        "At the end of your turn, reveal cards from the top of your Main Deck until you reveal a unit and banish it. Play it, ignoring its cost, and recycle the rest.",
        None,
    ),
    (
        "Disintegrate",
        "[Action] (Play on your turn or in showdowns.)\nDeal 3 to a unit at a battlefield. If this kills it, do this: draw 1.",
        None,
    ),
    (
        "Dragon's Rage",
        "Move an enemy unit. Then do this: Choose another enemy unit at its destination. They deal damage equal to their Mights to each other.",
        None,
    ),
    (
        "Dune Drake",
        "When I attack, give me +2 [M] this turn if there is a ready enemy unit here.",
        None,
    ),
    (
        "Highlander",
        "[Reaction] (Play any time, even before spells and abilities resolve.)\nChoose a friendly unit. The next time it would die this turn, heal it, exhaust it, and recall it instead. (Send it to base. This isn't a move.)",
        None,
    ),
    (
        "Karma, Channeler",
        "[Vision] (When you play me, look at the top card of your Main Deck. You may recycle it.)\nWhen you recycle one or more cards to your Main Deck, buff a friendly unit. (If it doesn't have a buff, it gets a +1 [M] buff. Runes aren't cards.)",
        None,
    ),
    (
        "Kinkou Monk",
        "When you play me, buff up to two other friendly units. (Each one that doesn't have a buff gets a +1 [M] buff.)",
        None,
    ),
    (
        "Nocturne, Horrifying",
        "[Ganking] (I can move from battlefield to battlefield.)\nAs you look at or reveal me from the top of your deck, you may banish me. If you do, you may play me for [A].",
        None,
    ),
    (
        "Pack of Wonders",
        "[E]: Return another friendly gear, unit, or facedown card to its owner's hand.",
        None,
    ),
    (
        "Portal Rescue",
        "[Action] (Play on your turn or in showdowns.)\nBanish a friendly unit, then its owner plays it to their base, ignoring its cost.",
        None,
    ),
    (
        "Promising Future",
        "Each player looks at the top 5 cards of their Main Deck, banishes one of them, then recycles the rest. Starting with the next player, each player plays those cards, ignoring Energy costs. (They must still pay Power costs.)",
        None,
    ),
    (
        "Ravenborn Tome",
        "[E]: The next spell you play this turn deals 1 Bonus Damage. (Each instance of damage the spell deals is increased by 1.)",
        None,
    ),
    (
        "Salvage",
        "[Action] (Play on your turn or in showdowns.)\nYou may kill up to one gear. Draw 1.",
        None,
    ),
    (
        "Sigil of the Storm",
        "When you conquer here, you must recycle one of your runes. (This doesn't choose anything.)",
        None,
    ),
    (
        "Sona, Harmonious",
        "At the end of your turn, if I'm at a battlefield, ready up to 4 friendly runes.",
        None,
    ),
    (
        "Targon's Peak",
        "When you conquer here, ready up to 2 runes at the end of this turn.",
        None,
    ),
    (
        "Teemo, Strategist",
        "[Hidden] (Hide now for [A] to react with later for [0].)\nWhen I defend, choose an enemy unit here and reveal the top 5 cards of your Main Deck. Deal 1 to that unit for each card with [Hidden] revealed this way, then recycle the revealed cards.",
        None,
    ),
    (
        "The Boss",
        "If a buffed unit you control would die, you may pay [C], exhaust me, and spend its buff to heal it, exhaust it, and recall it instead. (Send it to base. This isn't a move.)\nWhen you conquer, ready me.",
        None,
    ),
    (
        "The Dreaming Tree",
        "When a player chooses a friendly unit here with a spell for the first time each turn, they draw 1.",
        None,
    ),
    (
        "The Syren",
        "[1], [E]: Move a friendly unit at a battlefield to its base.",
        None,
    ),
    (
        "Tideturner",
        "[Hidden] (Hide now for [A] to react with later for [0].)\nWhen you play me, you may choose a unit you control at another location. Move me to its location and it to my original location.",
        None,
    ),
    (
        "Unforgiven",
        "[2], [E]: Move a friendly unit to or from its base.",
        None,
    ),
    (
        "Unlicensed Armory",
        "Discard 1, [E]: Choose a friendly unit. The next time it would die this turn, you may pay [C] to heal it, exhaust it, and recall it instead. (Send it to base. This isn't a move.)",
        None,
    ),
    (
        "Void Gate",
        "Spells and abilities deal 1 Bonus Damage to units here. (Each instance of damage the spell deals to a unit here is increased by 1.)",
        None,
    ),
    (
        "Zhonya's Hourglass",
        "[Hidden] (Hide now for [A] to react with later for [0].)\nIf a friendly unit would die, kill this instead. Heal that unit, exhaust it, and recall it. (Send it to base. This isn't a move.)",
        None,
    ),
    # ===== Origins + Spiritforged Errata (1-12-26) =====
    (
        "Falling Star",
        "Deal 3 to a unit.\nDeal 3 to a unit.",
        None,
    ),
    (
        "Icathian Rain",
        "Deal 2 to a unit.\nDeal 2 to a unit.\nDeal 2 to a unit.\nDeal 2 to a unit.\nDeal 2 to a unit.\nDeal 2 to a unit.",
        None,
    ),
    (
        "Reinforce",
        "Look at the top 5 cards of your Main Deck. You may banish a unit from among them, then play it, reducing its cost by [5]. Recycle the remaining cards.",
        None,
    ),
    (
        "Arise!",
        "Play a 2 [M] Sand Soldier unit token for each Equipment you control. Then do this: Ready up to two of them.",
        None,
    ),
    (
        "Blood Rush",
        "[Action] (Play on your turn or in showdowns.)\n[Repeat] [1] (You may pay the additional cost to repeat this spell's effect.)\nGive a unit [Assault 2] this turn. (+2 [M] while it's an attacker.)",
        None,
    ),
    (
        "Deathgrip",
        "[Reaction] (Play any time, even before spells and abilities resolve.)\nKill a friendly unit. If you do, give +[M] equal to its Might to another friendly unit this turn.\nDraw 1.",
        None,
    ),
    (
        "Edge of Night",
        "[Hidden] (Hide now for [A] to react with later for [0].)\nWhen you play this from face down, attach it to a unit you control (here).\n[Equip] [C] ([C]: Attach this to a unit you control.)",
        None,
    ),
    (
        "Janna, Savior",
        "[Reaction] (Play any time, even before spells and abilities resolve, including to a battlefield you control.)\nWhen you play me, heal your units here, then move up to one enemy unit from here to its base.",
        None,
    ),
    (
        "Jax, Unmatched",
        "[Deflect] (Opponents must pay [A] to choose me with a spell or ability.)\nYour Equipment everywhere have [Quick-Draw]. (Each gains [Reaction]. When you play it, attach it to a unit you control.)",
        None,
    ),
    (
        "Kato the Arm",
        "[Deflect] (Opponents must pay [A] to choose me with a spell or ability.)\nWhen I move to a battlefield, give another friendly unit my keywords and +[M] equal to my Might this turn.",
        None,
    ),
    (
        "Rek'Sai, Swarm Queen",
        "When I attack, you may reveal the top 2 cards of your Main Deck. You may banish one, then play it. If it is a unit, you may play it here. Recycle the rest.",
        None,
    ),
    (
        "Rell, Magnetic",
        "[Tank] (I must be assigned combat damage first.)\nWhen I attack, you may play an Equipment with Energy cost no more than [2], ignoring its cost. If you do, then do this: Attach it to me.",
        None,
    ),
    (
        "Tianna Crownguard",
        "[Deflect] (Opponents must pay [A] to choose me with a spell or ability.)\nWhile I'm at a battlefield, opponents can't gain points.",
        None,
    ),
    (
        "Void Burrower",
        "When you conquer, you may exhaust me to reveal the top 2 cards of your Main Deck. You may banish one, then play it. Recycle the rest.",
        None,
    ),
    (
        "Void Rush",
        "Reveal the top 2 cards of your Main Deck. You may banish one, then play it, reducing its cost by [2]. Draw any you didn't banish.",
        None,
    ),
    (
        "Yone, Blademaster",
        "[Weaponmaster] (When you play me, you may [Equip] one of your Equipment to me for [A] less, even if it's already attached.)\nWhen I conquer a battlefield that was uncontrolled, deal damage equal to my Might to an enemy unit in a base.",
        None,
    ),
    # ===== Spiritforged + Unleashed Errata (unleased.txt) =====
    (
        "Guards!",
        "[Hidden] (Hide now for [A] to react with later for [0].)\nPlay a 2 [M] Sand Soldier unit token. Then do this: You may pay [C] to ready it.",
        None,
    ),
    (
        "Relentless Pursuit",
        "[Action] (Play on your turn or in showdowns.)\nMove a friendly unit. You may attach up to one Equipment with the same controller to it. This turn, that unit has \"When I conquer, you may move me to my base.\"",
        None,
    ),
    (
        "Death from Below",
        "Kill a unit at a battlefield. Then, if it had 3 [M] or less, do this: You may play this from your trash for [A].",
        None,
    ),
    (
        "Bone Skewer",
        "[Hidden] (Hide now for [A] to react with later for [0].)\nChoose a battlefield. An opponent reveals their hand. You may choose a unit from it. They play that unit to that battlefield, ignoring any and all costs. If they do, then do this: [Stun] it. (It doesn't deal combat damage this turn.)",
        None,
    ),
    (
        "Deceiver",  # Legend card for LeBlanc — stored as just "Deceiver" in gallery
        "When you conquer or hold, you may discard 1 and exhaust me to play a ready Reflection unit token there. Then do this: It becomes a copy of another unit there. Give it [Temporary].",
        None,
    ),
    (
        "Mirror Image",
        "Choose a unit. Play a ready Reflection unit token to your base. Then do this: It becomes a copy of that unit. Give it [Temporary]. (Kill it at the start of its controller's Beginning Phase, before scoring.)",
        None,
    ),
    (
        "Keeper of Masks",
        "[Hidden] (Hide now for [A] to react with later for [0].)\n[Temporary] (Kill me at the start of my controller's Beginning Phase, before scoring.)\nWhen you play me, play two Reflection unit tokens here. Then do this: They become copies of me.",
        None,
    ),
    (
        "Rengar, Trophy Hunter",
        "[Ambush] (You may play me as a [Reaction] to a battlefield where you have units.)\nI can [Ambush] to a battlefield where there are enemy units, even if you don't have units there.",
        None,
    ),
]


def main():
    # Load index
    with open(INDEX_PATH) as f:
        index = json.load(f)

    # Build lookup by name (multiple cards can share a name across sets)
    by_name: dict[str, list[dict]] = {}
    for card in index:
        by_name.setdefault(card["name"], []).append(card)

    # Apply errata
    applied = 0
    not_found = []

    for card_name, new_ability, new_effect in ERRATA:
        matches = by_name.get(card_name, [])
        if not matches:
            # Try partial match for legends with variant names
            for name, cards in by_name.items():
                if name.startswith(card_name + ",") or name.startswith(card_name + " "):
                    matches.extend(cards)
            if not matches:
                not_found.append(card_name)
                continue

        for card in matches:
            changed = False
            if new_ability is not None and card["ability_text"] != new_ability:
                card["ability_text"] = new_ability
                changed = True
            if new_effect is not None and card.get("effect_text") != new_effect:
                card["effect_text"] = new_effect
                changed = True

            if changed:
                applied += 1

    # Write updated index
    with open(INDEX_PATH, "w") as f:
        json.dump(index, f, indent=2)

    # Update individual JSON files
    json_updated = 0
    for filepath in sorted(JSON_DIR.glob("*.json")):
        with open(filepath) as f:
            card = json.load(f)

        card_name = card["name"]
        matches = by_name.get(card_name, [])
        for match in matches:
            if match["id"] == card["id"]:
                if card["ability_text"] != match["ability_text"] or card.get("effect_text") != match.get("effect_text"):
                    card["ability_text"] = match["ability_text"]
                    card["effect_text"] = match.get("effect_text")
                    with open(filepath, "w") as f:
                        json.dump(card, f, indent=2)
                    json_updated += 1
                break

    print(f"Applied {applied} errata changes to card_index.json")
    print(f"Updated {json_updated} individual JSON files")
    if not_found:
        print(f"WARNING: {len(not_found)} cards not found in database:")
        for name in not_found:
            print(f"  - {name}")


if __name__ == "__main__":
    main()
