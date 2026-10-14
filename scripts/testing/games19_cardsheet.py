#!/usr/bin/env python3
"""Printed card text for the reading of the 19 paired games.

Usage:
  games19_cardsheet.py <registry.json> --all <cards.txt>
  games19_cardsheet.py <registry.json> --game <deck1.txt> <deck2.txt> <sheet.txt>

<registry.json> is the output of `riftbound --dump-registry`. --all writes one
block per registered card (id, name, type, cost, Might, Domains, tags, text),
sorted by name, for grep. --game writes the blocks of the cards the two lists
hold (sideboards left out: they are not played), grouped by list and section,
followed by every token card of the pool (a game can create any of them).
A name that several ids share is printed once per id.
"""
import json, sys, collections

def load(path):
    data = json.load(open(path))
    cards = data["cards"] if isinstance(data, dict) and "cards" in data else data
    if isinstance(cards, dict):
        cards = list(cards.values())
    return cards

def block(c):
    cost = f"[{c.get('energy_cost', 0)}]" + (f" + {c['power_cost']} Power" if c.get("power_cost") else "")
    head = f"{c['name']}  (id {c.get('card_id', c.get('id'))}, {c.get('public_code', '')})"
    kind = c.get("card_type", "")
    if c.get("super_type"):
        kind = f"{c['super_type']} {kind}"
    line = f"  {kind} | cost {cost} | Might {c.get('might', 0)}"
    if c.get("might_bonus"):
        line += f" | Might bonus +{c['might_bonus']}"
    line += f" | Domains {','.join(c.get('domains', [])) or '-'} | tags {','.join(c.get('tags', [])) or '-'}"
    if c.get("banned"):
        line += " | BANNED"
    text = (c.get("ability_text") or "").strip() or "(no rules text)"
    out = [head, line, "  text: " + text]
    if (c.get("effect_text") or "").strip():
        out.append("  effect: " + c["effect_text"].strip())
    return "\n".join(out)

def deck_sections(path):
    secs, cur = collections.OrderedDict(), None
    for raw in open(path):
        s = raw.strip()
        if not s or s.startswith("#"):
            continue
        if s.endswith(":"):
            cur = s[:-1]
            secs[cur] = []
            continue
        n, _, name = s.partition(" ")
        if n.isdigit() and cur:
            secs[cur].append((int(n), name.strip()))
    return secs

def main():
    cards = load(sys.argv[1])
    by_name = collections.defaultdict(list)
    for c in cards:
        by_name[c["name"].lower()].append(c)
        if c.get("card_type") == "legend":
            # the loader's aliases: "<Tag>, <Name>", and a "<Name> - Starter"
            # legend also answers to "<Name>" and "<Tag>, <Name>"
            titles = [c["name"]]
            if c["name"].endswith(" - Starter"):
                titles.append(c["name"][:-len(" - Starter")])
                by_name[titles[1].lower()].append(c)
            for t in c.get("tags", []):
                for title in titles:
                    by_name[(t + ", " + title).lower()].append(c)
    if sys.argv[2] == "--all":
        with open(sys.argv[3], "w") as f:
            for c in sorted(cards, key=lambda c: (c["name"].lower(), c.get("card_id", 0))):
                f.write(block(c) + "\n\n")
        return
    d1, d2, out = sys.argv[3:6]
    with open(out, "w") as f:
        for label, path in (("DECK1 (P1)", d1), ("DECK2 (P2)", d2)):
            f.write(f"################ {label}: {path}\n\n")
            for sec, rows in deck_sections(path).items():
                if sec.lower().startswith("side"):
                    continue
                f.write(f"==== {sec}\n")
                for n, name in rows:
                    hits = by_name.get(name.lower())
                    if not hits:
                        f.write(f"{n}x {name}\n  (NOT FOUND IN THE CARD TABLE)\n\n")
                        continue
                    for c in hits:
                        f.write(f"{n}x " + block(c) + "\n\n")
        f.write("################ TOKEN CARDS OF THE POOL\n\n")
        for c in sorted(cards, key=lambda c: c["name"]):
            if "token" in (c.get("card_type") or "").lower() or "token" in (c.get("super_type") or "").lower():
                f.write(block(c) + "\n\n")

if __name__ == "__main__":
    main()
