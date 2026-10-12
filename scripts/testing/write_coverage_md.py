#!/usr/bin/env python3
"""Markdown table of the coverage index (card_coverage.py) for the findings document.

Usage: write_coverage_md.py <coverage.json> <out.md>

Per card: the number of the read games whose lists hold it, how often it was
played from hand (PLAY / SPELL lines; a battlefield: games it was the chosen
battlefield; a rune: times channelled), hidden, how often an ability of it
was queued, activated and resolved, and the first sighting of a play, a
trigger and an activation as game + D. A play from facedown prints no PLAY
line, so "played" undercounts cards with [Hidden]; an activation at chain
priority or at Focus prints no source, so "activated" counts Main Phase
activations only.
"""
import json, sys

def first(e, kind):
    f = e["first"].get(kind) or []
    return f"{f[0][0][:-4]} D:{f[0][1]:03d}" if f else "—"

def main():
    cov = json.load(open(sys.argv[1]))
    order = {"Legend": 0, "Unit": 1, "Spell": 2, "Gear": 3, "Battlefield": 4, "Rune": 5}
    with open(sys.argv[2], "w") as out:
        out.write("| card | type | lists | games | played | hidden | triggered | activated | resolved (ability / spell) | first play | first trigger | first activation |\n")
        out.write("|---|---|---|---|---|---|---|---|---|---|---|---|\n")
        for name, e in sorted(cov.items(), key=lambda kv: (order[kv[1]["type"]], kv[0])):
            c = e["count"]
            out.write(f"| {name} | {e['type']} | {len(e['decks'])} | {e['in_games']} | {c['play']} | {c['hide']} | {c['trigger']} | "
                      f"{c['activate']} | {c['res_abil']} / {c['res_spell']} | {first(e, 'play')} | {first(e, 'trigger')} | {first(e, 'activate')} |\n")

if __name__ == "__main__":
    main()
