#!/usr/bin/env python3
"""Phase 6o audit — per-deck card implementation status.

For each card in each deck under decks/, classify as:
  ✅ FULL    — has a manual override in src/cards/manual/
  ⚠️ PARTIAL — only generated; has a body BUT carries a "partial" note
              OR uses only trivial primitives (single moveToBase, single buff)
  ❌ STUB    — only generated; onResolve/onTrigger body is empty
  ?  MISSING — no class found at all (parser miss)

Run from repo root:  python3 scripts/audit_deck_cards.py [deck1 ...]
"""

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MANUAL_DIR = REPO / "src" / "cards" / "manual"
GENERATED_DIR = REPO / "src" / "cards" / "generated"
DECKS_DIR = REPO / "decks"


def manual_card_ids():
    pat = re.compile(r"registry\.registerCard\(\s*(\d+)\s*,")
    ids = set()
    for cpp in MANUAL_DIR.glob("*.cpp"):
        for m in pat.finditer(cpp.read_text()):
            ids.add(int(m.group(1)))
    return ids


def generated_card_bodies():
    """For each generated card_id, find the next class and read its body.

    Returns dict cid → {
        has_partial_note: bool,
        class_body: str,
        bodies: {Resolve/Trigger/Activate: source string},
    }
    """
    out = {}
    body_re = re.compile(
        r"void on(Resolve|Trigger|Activate|Play|Equip)\s*\([^)]*\)\s*override\s*\{",
    )
    for cpp in GENERATED_DIR.glob("*.cpp"):
        text = cpp.read_text()
        # Find all card_id markers + their positions
        markers = list(re.finditer(r"^/// card_id=(\d+),([^\n]*)$", text, re.MULTILINE))
        for i, m in enumerate(markers):
            cid = int(m.group(1))
            block_start = m.start()
            block_end = markers[i + 1].start() if i + 1 < len(markers) else len(text)
            block = text[block_start:block_end]
            # Look for partial note
            has_partial = "partial implementation" in block.lower()
            # Find the class { ... }; — match {..} with brace-counting
            class_match = re.search(r"class \w+ : public \w+Card \{", block)
            if not class_match:
                continue
            i_class = class_match.end()
            depth = 1
            j = i_class
            while j < len(block) and depth > 0:
                if block[j] == "{":
                    depth += 1
                elif block[j] == "}":
                    depth -= 1
                j += 1
            class_body = block[i_class:j - 1]
            # Find each on* body
            bodies = {}
            for fn in body_re.finditer(class_body):
                # body starts at fn.end() - 1 (the opening brace position)
                start = class_body.find("{", fn.start())
                depth = 1
                k = start + 1
                while k < len(class_body) and depth > 0:
                    if class_body[k] == "{":
                        depth += 1
                    elif class_body[k] == "}":
                        depth -= 1
                    k += 1
                bodies[fn.group(1)] = class_body[start + 1:k - 1].strip()
            out[cid] = {
                "has_partial_note": has_partial,
                "class_body": class_body,
                "bodies": bodies,
            }
    return out


def classify_body(body: str) -> str:
    """Classify a generated function body."""
    if not body.strip():
        return "EMPTY"
    # Strip comments
    stripped = []
    for line in body.splitlines():
        s = line.strip()
        if s and not s.startswith("//"):
            stripped.append(s)
    if not stripped:
        return "EMPTY"
    # Single-statement trivials
    joined = " ".join(stripped)
    trivial_markers = [
        "ctx.executor.moveToBase(targets[0]);",
        "// Choose target",
        "// targeting handled",
    ]
    if len(stripped) <= 2 and all(any(t in joined for t in trivial_markers)
                                    for _ in [0]):
        # heuristic — single moveToBase or comment-only
        if "moveToBase" in joined and len(stripped) == 1:
            return "TRIVIAL"
        if all(s.startswith("//") or s.startswith("/*") for s in stripped):
            return "EMPTY"
    return "REAL"


def classify_card(cid, manual_ids, generated):
    if cid in manual_ids:
        return "FULL", "manual override"
    if cid not in generated:
        return "MISSING", "no generated class"
    g = generated[cid]
    bodies = g["bodies"]
    if not bodies:
        # No effect functions — vanilla card with no triggered/activated abilities
        if g["has_partial_note"]:
            return "STUB", "complex card, no body"
        return "FULL", "no effects needed"
    # Pick the primary body
    primary = (bodies.get("Resolve")
               or bodies.get("Trigger")
               or bodies.get("Activate")
               or bodies.get("Play")
               or bodies.get("Equip")
               or next(iter(bodies.values())))
    klass = classify_body(primary)
    if klass == "EMPTY":
        return "STUB", "empty body"
    if klass == "TRIVIAL":
        if g["has_partial_note"]:
            return "STUB", "trivial body + partial note"
        return "PARTIAL", "trivial body"
    if g["has_partial_note"]:
        return "PARTIAL", "generated; partial note present"
    return "FULL", "generated with real body"


def audit_deck(deck_path: Path, manual_ids, generated):
    data = json.loads(deck_path.read_text())
    seen = {}
    legend = data.get("legend") or {}
    if legend:
        seen[legend["card_id"]] = (legend["name"], 1, "legend")
    for c in data.get("champions", []):
        cid = c["card_id"]
        if cid in seen:
            n, k, z = seen[cid]; seen[cid] = (n, k + 1, z)
        else:
            seen[cid] = (c["name"], 1, "champion")
    for c in data.get("main_deck", []):
        cid = c["card_id"]
        if cid in seen:
            n, k, z = seen[cid]; seen[cid] = (n, k + 1, z)
        else:
            seen[cid] = (c["name"], 1, "main")
    counts = {"FULL": 0, "PARTIAL": 0, "STUB": 0, "MISSING": 0}
    rows = []
    for cid, (name, k, zone) in sorted(seen.items(), key=lambda x: (x[1][2], x[1][0])):
        status, reason = classify_card(cid, manual_ids, generated)
        counts[status] += k
        rows.append((cid, name, k, zone, status, reason))
    return rows, counts


def main():
    manual_ids = manual_card_ids()
    generated = generated_card_bodies()

    deck_paths = (
        [Path(p) for p in sys.argv[1:]]
        if len(sys.argv) > 1
        else sorted(DECKS_DIR.glob("*.json"))
    )

    overall = {"FULL": 0, "PARTIAL": 0, "STUB": 0, "MISSING": 0}
    bad_overall = []
    for dp in deck_paths:
        if not dp.exists():
            print(f"!! {dp} not found", file=sys.stderr); continue
        rows, counts = audit_deck(dp, manual_ids, generated)
        total = sum(counts.values())
        if total == 0:
            continue
        print(f"\n{'=' * 78}")
        print(f"{dp.name}  ({total} cards by copy count)")
        print(f"  ✅ FULL={counts['FULL']}  ⚠️  PARTIAL={counts['PARTIAL']}  "
              f"❌ STUB={counts['STUB']}  ? MISSING={counts['MISSING']}")
        print(f"{'=' * 78}")
        for cid, name, k, zone, status, reason in rows:
            if status != "FULL":
                marker = {"PARTIAL": "⚠️ ", "STUB": "❌", "MISSING": "?  "}[status]
                print(f"  {marker} [{cid:3}] {k}x {name:35} ({zone:8}) — {reason}")
                bad_overall.append((dp.name, cid, name, k, status))
        for k_, v in counts.items():
            overall[k_] += v
    print(f"\n{'=' * 78}")
    print(f"ROLL-UP across {len(deck_paths)} decks (weighted by copies):")
    print(f"  ✅ FULL={overall['FULL']}  ⚠️  PARTIAL={overall['PARTIAL']}  "
          f"❌ STUB={overall['STUB']}  ? MISSING={overall['MISSING']}")
    print(f"{'=' * 78}\n")


if __name__ == "__main__":
    main()
