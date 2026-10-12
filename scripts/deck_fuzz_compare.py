#!/usr/bin/env python3
"""Compares two `deck_fuzz --json` results (an earlier pass and a later one).

    python3 scripts/deck_fuzz_compare.py old.json new.json [--min-resolved K]

Says which failure signatures disappeared, which are new and which stayed
(with their rate per 100,000 games in each pass, since passes differ in size),
and which cards gained or lost coverage. A signature is the group's normalised
text plus the card it names, so "Alpha Strike returned from its finalize pass"
and "Kennen ... returned from its finalize pass" are two signatures; every
group of one known cause (`--known`) is one signature.

With --min-resolved K it also lists the cards under the stopping rule's
threshold in the new pass: every card that can be in a deck, in play in fewer
than K games, or (if it has a triggered / activated ability or is a spell)
triggered / activated / resolved in fewer than K.

Exit status: 1 if the new pass has a signature the old one does not, else 0.
"""
import json
import sys

KINDS = ("in_play", "triggered", "activated", "resolved")


def signatures(j):
    """signature -> (count, known id or None, replay command)"""
    out = {}
    for g in j["failure_groups"]:
        if g.get("known"):
            # A known cause is one signature however its text varies (a chain
            # runaway names bystanders too).
            key = "known " + g["known"]
            ex = g["fewest_decisions_first"]
            n, _, cmd = out.get(key, (0, None, ""))
            out[key] = (n + g["count"], g["known"], cmd or (ex[0]["command"] if ex else ""))
            continue
        per_card = g.get("fewest_decisions_per_card") or {}
        if per_card:
            for card, v in per_card.items():
                out[g["key"].replace("<card>", card, 1)] = (v["count"], g.get("known"), v["command"])
        else:
            ex = g["fewest_decisions_first"]
            out[g["key"]] = (g["count"], g.get("known"), ex[0]["command"] if ex else "")
    return out


def exercised(c):
    """The smallest of the counts that apply to the card (None: nothing applies)."""
    if c["banned"] or c["token_card"]:
        return None
    s = c["games_in_play"]
    if c["has_triggered_ability"]:
        s = min(s, c["games_triggered"])
    if c["has_activated_ability"]:
        s = min(s, c["games_activated"])
    if c["has_triggered_ability"] or c["has_activated_ability"] or c["type"] == "Spell":
        s = min(s, c["games_resolved"])
    return s


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    k = None
    if "--min-resolved" in sys.argv:
        k = int(sys.argv[sys.argv.index("--min-resolved") + 1])
        args = [a for a in args if a != str(k)]
    if len(args) != 2:
        print(__doc__)
        return 2
    old, new = (json.load(open(p)) for p in args)
    go, gn = old["totals"]["games"], new["totals"]["games"]

    def describe(j):
        c = j["config"]
        return (f'{j["totals"]["games"]} games, seed {c["seed"]}, {c.get("agent1", "random")} vs '
                f'{c.get("agent2", "random")}, {"checked" if c["checked_build"] else "optimised"} build'
                + (f', guided by {c["guide"]}' if c.get("guide") else ""))

    print(f"old: {args[0]} ({describe(old)})")
    print(f"new: {args[1]} ({describe(new)})\n")

    so, sn = signatures(old), signatures(new)

    def rate(n, games):
        return f"{n} ({100000.0 * n / games:.1f} per 100k)"

    gone = sorted(set(so) - set(sn))
    fresh = sorted(set(sn) - set(so))
    both = sorted(set(so) & set(sn))
    print(f"## Failure signatures: {len(gone)} disappeared, {len(fresh)} new, {len(both)} in both\n")
    for s in gone:
        print(f"- DISAPPEARED [{so[s][1] or 'not known'}] {rate(so[s][0], go)}: {s}")
    for s in fresh:
        print(f"- NEW [{sn[s][1] or 'not known'}] {rate(sn[s][0], gn)}: {s}\n    `{sn[s][2]}`")
    for s in both:
        print(f"- both [{sn[s][1] or 'not known'}] {rate(so[s][0], go)} -> {rate(sn[s][0], gn)}: {s}")

    co = {c["id"]: c for c in old["coverage"]["cards"]}
    cn = {c["id"]: c for c in new["coverage"]["cards"]}
    print("\n## Coverage\n")
    for kind in KINDS:
        gained = [c for i, c in cn.items() if i in co and co[i]["games_" + kind] == 0 and c["games_" + kind] > 0]
        lost = [c for i, c in cn.items() if i in co and co[i]["games_" + kind] > 0 and c["games_" + kind] == 0]
        print(f"- {kind}: {len(gained)} cards gained, {len(lost)} lost")
        for c in gained:
            print(f'    + {c["id"]} {c["name"]} (0 -> {c["games_" + kind]})')
        for c in lost:
            print(f'    - {c["id"]} {c["name"]} ({co[c["id"]]["games_" + kind]} -> 0)')

    def under(cards, limit):
        return [c for c in cards.values() if exercised(c) is not None and exercised(c) < limit]

    def per_100k(cards, games):
        vals = sorted(exercised(c) * 100000.0 / games for c in cards.values() if exercised(c) is not None)
        return vals[0], vals[len(vals) // 20], vals[len(vals) // 2]

    lo, mo = per_100k(co, go), per_100k(cn, gn)
    print(f"\n- least-exercised count per 100k games (min / 5th percentile / median): "
          f"old {lo[0]:.0f} / {lo[1]:.0f} / {lo[2]:.0f}, new {mo[0]:.0f} / {mo[1]:.0f} / {mo[2]:.0f}")
    print(f"- cards never exercised in some way that applies to them: old {len(under(co, 1))}, new {len(under(cn, 1))}")
    if k is not None:
        low = sorted(under(cn, k), key=exercised)
        print(f"- cards under {k} in the new pass: {len(low)} (old pass: {len(under(co, k))})")
        for c in low:
            print(f'    {c["id"]} {c["name"]} ({c["type"]}): in play {c["games_in_play"]}, triggered '
                  f'{c["games_triggered"]}, activated {c["games_activated"]}, resolved {c["games_resolved"]}')
    return 1 if fresh else 0


if __name__ == "__main__":
    sys.exit(main())
