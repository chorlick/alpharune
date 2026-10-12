#!/usr/bin/env python3
"""Inventory of the constructed deck lists in decks/.

Usage: deck_inventory.py <registry.json> <decks-dir> <out.json> [--md <out.md>]

Joins every line of every list to the compiled card table (the output of
`riftbound --dump-registry`) the way the engine's loader does (case-insensitive
name; a legend also answers to "<Tag>, <Name>"; the FIRST registered id of a
name wins), and writes the per-deck inventory and the cross-deck counts.
"""
import json, sys, os, collections

def load_registry(path):
    with open(path) as f:
        data = json.load(f)
    cards = data["cards"] if isinstance(data, dict) and "cards" in data else data
    if isinstance(cards, dict):
        cards = list(cards.values())
    return cards

def build_index(cards):
    idx = collections.OrderedDict()
    for c in sorted(cards, key=lambda c: c["id"]):
        idx.setdefault(c["name"].lower(), []).append(c)
    for c in sorted(cards, key=lambda c: c["id"]):
        if c.get("card_type", "").lower() == "legend":
            for t in c.get("tags", []):
                a = (t + ", " + c["name"]).lower()
                idx.setdefault(a, [c])
            suf = " - Starter"
            if c["name"].endswith(suf):
                title = c["name"][:-len(suf)]
                for a in [title.lower()] + [(t + ", " + title).lower() for t in c.get("tags", [])]:
                    idx.setdefault(a, [c])
    return idx

def parse_deck(path):
    sections = collections.OrderedDict()
    sec = None
    with open(path) as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if line.endswith(":"):
                sec = line[:-1].strip().lower()
                sections.setdefault(sec, [])
                continue
            n, name = line.split(None, 1)
            sections[sec].append((int(n), name.strip()))
    return sections

def main():
    reg, deckdir, out = sys.argv[1:4]
    cards = load_registry(reg)
    idx = build_index(cards)
    inv = collections.OrderedDict()
    for fn in sorted(os.listdir(deckdir)):
        if not fn.endswith(".txt"):
            continue
        deck = fn[:-4]
        secs = parse_deck(os.path.join(deckdir, fn))
        d = collections.OrderedDict()
        for sec, lines in secs.items():
            rows = []
            for n, name in lines:
                hit = idx.get(name.lower())
                if not hit:
                    rows.append({"count": n, "name": name, "id": None})
                    continue
                c = hit[0]
                rows.append({"count": n, "name": name, "id": c["id"],
                             "printed_name": c["name"],
                             "reprints": [x["id"] for x in hit[1:]],
                             "card": c})
            d[sec] = rows
        inv[deck] = d
    with open(out, "w") as f:
        json.dump(inv, f, indent=1)

if __name__ == "__main__":
    main()
