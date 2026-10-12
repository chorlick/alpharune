#!/usr/bin/env python3
"""Markdown inventory of the deck lists for the findings document.

Usage: write_inventory_md.py <inventory.json> <coverage.json> <out.md>

Part 1: per list — legend, champion, battlefields, runes, Main Deck and
sideboard with counts and card ids. Part 2: every distinct card once, with
its printed text from def() and the lists that hold it.
"""
import collections, json, sys

def cost(c):
    if c["card_type"] in ("Legend", "Battlefield", "Rune"):
        return ""
    s = f"[{c.get('energy_cost', 0)}]"
    if c.get("power_cost"):
        s += "".join("[" + (c.get("domains") or ["A"])[0][0] + "]" for _ in range(c["power_cost"]))
    return s

def main():
    inv = json.load(open(sys.argv[1]))
    cov = json.load(open(sys.argv[2]))
    out = open(sys.argv[3], "w")
    use = collections.OrderedDict()
    out.write("### Per list\n\n")
    for deck, secs in inv.items():
        leg, ch = secs["legend"][0]["card"], secs["champion"][0]["card"]
        out.write(f"**{deck}** — legend {leg['name']} (#{leg['id']}, {'/'.join(leg['domains'])}), "
                  f"champion {ch['name']} (#{ch['id']})\n\n")
        out.write("- battlefields: " + ", ".join(f"{r['card']['name']} (#{r['card']['id']})" for r in secs["battlefields"]) + "\n")
        out.write("- runes: " + ", ".join(f"{r['count']} {r['card']['name']}" for r in secs["runes"]) + "\n")
        out.write("- Main Deck (39): " + ", ".join(f"{r['count']} {r['card']['name']} (#{r['card']['id']})" for r in secs["maindeck"]) + "\n")
        out.write("- sideboard (10): " + ", ".join(f"{r['count']} {r['card']['name']} (#{r['card']['id']})" for r in secs.get("sideboard", [])) + "\n\n")
        for sec, rows in secs.items():
            for r in rows:
                use.setdefault(r["card"]["name"], (r["card"], []))[1].append(deck if sec != "sideboard" else deck + "(sb)")
    out.write("### Every distinct card\n\n")
    out.write(f"{len(use)} distinct cards. `lists` counts the lists that hold the card in any section; `(sb)` = sideboard only in that list.\n\n")
    out.write("| card | id | type | cost | Might | lists | printed text (def()) |\n|---|---|---|---|---|---|---|\n")
    order = {"Legend": 0, "Unit": 1, "Spell": 2, "Gear": 3, "Battlefield": 4, "Rune": 5}
    for name, (c, decks) in sorted(use.items(), key=lambda kv: (order[kv[1][0]["card_type"]], kv[0])):
        typ = c["card_type"] + ("/" + c["super_type"] if c.get("super_type") else "")
        text = (c.get("ability_text", "") + (" // effect text: " + c["effect_text"] if c.get("effect_text") else "")).replace("\n", " / ").replace("|", "\\|")
        uniq = []
        for d in decks:
            base = d.replace("(sb)", "")
            if not any(u.replace("(sb)", "") == base for u in uniq):
                uniq.append(d)
            elif d == base:   # main-deck entry wins over (sb)
                uniq = [u if u.replace("(sb)", "") != base else base for u in uniq]
        out.write(f"| {name} | {c['id']} | {typ} | {cost(c)} | {c.get('might', '')} | {len(uniq)}: {', '.join(uniq)} | {text} |\n")
    out.close()

if __name__ == "__main__":
    main()
