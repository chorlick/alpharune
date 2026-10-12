#!/usr/bin/env python3
"""Per-card observation index over the traced deck games.

Usage: card_coverage.py <inventory.json> <trace-dir> <out.json>

For every distinct card of the deck lists (inventory.json from
deck_inventory.py) it scans every <trace-dir>/*.log and records where the
card was seen, as (log file, decision number, line number):

  in_games   games whose deck list (either seat) holds the card
  play       "PLAY: <name> (" (permanent) / "SPELL: <name> (" (spell);
             a battlefield: "CHOSE BF #n (<name>)"; a rune: "CHANNELED: <name>"
  hide       "HIDE: <name> at"
  trigger    "TRIGGER: <name> ("            (an ability of the card was queued)
  activate   "CHOSE: [Px] Activate(Reaction)Ability (src=<name>)"
  finalize   "FINALIZE-CHOICES: <name> ("
  res_abil   "RESOLVE: <name> (chain_item=.., ability=1"
  res_spell  "RESOLVE: <name> (chain_item=.., ability=0, spell=1"
  own_lines  trace lines the card's class writes itself ("<NAME IN CAPS>: ...")

Counts are totals; `first` keeps up to 12 sightings per kind, spread over
different games. Names are matched as the engine prints them.
"""
import json, os, re, sys, collections

DEC = re.compile(r'\[D:(\d+)\.\d+\]')

def main():
    inv_path, tdir, out = sys.argv[1:4]
    inv = json.load(open(inv_path))
    cards = {}
    deck_cards = collections.defaultdict(set)
    for deck, secs in inv.items():
        for sec, rows in secs.items():
            for r in rows:
                c = r["card"]
                cards.setdefault(c["name"], {"id": c["id"], "type": c["card_type"],
                                             "decks": {}, "name": c["name"]})
                cards[c["name"]]["decks"].setdefault(deck, []).append(sec)
                # a sideboard card is in the Main Deck of the _sb list only
                if sec == "sideboard":
                    deck_cards[deck + "_sb"].add(c["name"])
                else:
                    deck_cards[deck].add(c["name"])
                    deck_cards[deck + "_sb"].add(c["name"])
    names = sorted(cards, key=len, reverse=True)
    kinds = ["play", "hide", "trigger", "activate", "finalize", "res_abil", "res_spell", "own_lines"]
    for c in cards.values():
        c["count"] = {k: 0 for k in kinds}
        c["first"] = {k: [] for k in kinds}
        c["games"] = {k: set() for k in kinds}
        c["in_games"] = 0
    upper = {}
    for n in names:
        upper.setdefault(n.upper(), n)
        upper.setdefault(re.sub(r"[,]", "", n.upper()), n)
    pats = [
        ("play", re.compile(r'\b(?:PLAY|SPELL): (.+?) \(id=')),
        ("play", re.compile(r'CHOSE BF #\d+ \((.+?)\)\s*$')),
        ("play", re.compile(r'CHANNELED: (.+?) \(id=')),
        ("hide", re.compile(r'HIDE: (.+?) at BF#')),
        ("trigger", re.compile(r'TRIGGER: (.+?) (?:\(P\d\)|\[)')),
        ("activate", re.compile(r'CHOSE: \[P\d\] Activate(?:Reaction)?Ability \(src=(.+?)\)')),
        ("finalize", re.compile(r'FINALIZE-CHOICES: (.+?) \(chain_item=')),
        ("res_abil", re.compile(r'RESOLVE: (.+?) \(chain_item=\d+, ability=1')),
        ("res_spell", re.compile(r'RESOLVE: (.+?) \(chain_item=\d+, ability=0, spell=1')),
    ]
    own = re.compile(r'\] \[D:[\d.]+\]\s+([A-Z][A-Z0-9 ,\'!\-\.]+?)(?: \(.*?\))?: ')
    files = sorted(f for f in os.listdir(tdir) if f.endswith(".log"))
    for fn in files:
        d1, d2 = fn.split("__")[:2]
        for n in deck_cards[d1] | deck_cards[d2]:
            cards[n]["in_games"] += 1
        with open(os.path.join(tdir, fn), errors="replace") as f:
            for ln, line in enumerate(f, 1):
                if "[D:" not in line:
                    continue
                m = DEC.search(line)
                dec = int(m.group(1)) if m else -1
                hit = None
                for kind, p in pats:
                    pm = p.search(line)
                    if pm and pm.group(1) in cards:
                        hit = (kind, pm.group(1))
                        break
                if not hit:
                    om = own.search(line)
                    if om and om.group(1) in upper:
                        hit = ("own_lines", upper[om.group(1)])
                if hit:
                    kind, n = hit
                    c = cards[n]
                    c["count"][kind] += 1
                    if fn not in c["games"][kind]:
                        c["games"][kind].add(fn)
                        if len(c["first"][kind]) < 12:
                            c["first"][kind].append([fn, dec, ln])
    for c in cards.values():
        c["games"] = {k: len(v) for k, v in c["games"].items()}
    json.dump(cards, open(out, "w"), indent=1)
    print(len(cards), "cards,", len(files), "games")

if __name__ == "__main__":
    main()
