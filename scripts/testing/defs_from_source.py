#!/usr/bin/env python3
"""Card table read straight from the card classes' def() blocks in src/cards.

Usage: defs_from_source.py <src/cards> <out.json>

Same content as `riftbound --dump-registry` for the fields the testing
documents need (id, name, type, costs, Might, domains, tags, keywords, text,
banned), plus the source file of each card. Used to join deck lists to card
text without a built binary and to point a reader at the card's class.
"""
import json, os, re, sys

FIELD = re.compile(r'^\s+d\.([a-z_]+)\s*=\s*(.*?);\s*$', re.S)

def parse(path):
    txt = open(path, encoding="utf-8").read()
    m = re.search(r'const CardDef def_ = \[\] \{(.*?)return d;', txt, re.S)
    if not m:
        return None
    body = m.group(1)
    d = {"file": path}
    for fm in re.finditer(r'd\.([a-z_]+)\s*=\s*(R"RB\((.*?)\)RB"|[^;]*);', body, re.S):
        key, raw, rawstr = fm.group(1), fm.group(2), fm.group(3)
        if rawstr is not None:
            d[key] = rawstr
        else:
            raw = raw.strip()
            if re.fullmatch(r'-?\d+', raw):
                d[key] = int(raw)
            elif raw in ("true", "false"):
                d[key] = raw == "true"
            elif raw.startswith("{"):
                d[key] = re.findall(r'R"RB\((.*?)\)RB"|(?:\w+::)(\w+)', raw)
                d[key] = [a or b for a, b in d[key]]
            else:
                d[key] = raw.split("::")[-1]
    d["keywords"] = re.findall(r'd\.keywords\.set\(Keyword::(\w+)\)', body)
    return d

def main():
    root, out = sys.argv[1:3]
    cards = []
    for dp, _, fns in os.walk(root):
        for fn in sorted(fns):
            if re.match(r'\d{4}_.*\.cpp$', fn):
                c = parse(os.path.join(dp, fn))
                if c and "id" in c:
                    cards.append(c)
    cards.sort(key=lambda c: c["id"])
    json.dump({"cards": cards}, open(out, "w"), indent=1)
    print(len(cards), "cards")

if __name__ == "__main__":
    main()
