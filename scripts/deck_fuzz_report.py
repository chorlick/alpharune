#!/usr/bin/env python3
"""Turns one or more `deck_fuzz --json` results into Markdown.

    python3 scripts/deck_fuzz_report.py result.json [more.json ...] > report.md

Prints, per result: totals, how the games ended, the failure groups (count,
cards named with a count and the shortest game of each, the command that
replays it), the flagged games, and the coverage lists: every card never in
play, never triggered, never activated, never resolved. Several results are
reported one after another; coverage is also summed over all of them (a card
counts as covered if any run covered it).
"""
import json
import sys


def table(rows, header):
    out = ["| " + " | ".join(header) + " |", "|" + "---|" * len(header)]
    for r in rows:
        out.append("| " + " | ".join(str(c) for c in r) + " |")
    return "\n".join(out)


def totals(j):
    t, c = j["totals"], j["config"]
    what = c["deck_dir"] or "random decks by seed"
    rows = [
        ("Decks", f'{c["decks"]} ({what}), {j["decks"]["distinct_decks"]} distinct, '
                  f'{len(j["decks"]["rejected_by_the_validator"])} rejected by the validator'),
        ("Pairs x games", f'{c["pairs"]} x {c["games_per_pair"]} = {t["games"]}, seed {c["seed"]}'),
        ("Build", "checked (RIFTBOUND_CHECK_BOARD_INDEX)" if c["checked_build"] else "optimised"),
        ("Completed", t["completed"]),
        ("P1 wins / P2 wins", f'{t["p1_wins"]} / {t["p2_wins"]}'),
        ("Agents", f'{c.get("agent1", "random")} vs {c.get("agent2", "random")}'
                   + (f', decks guided by {c["guide"]}' if c.get("guide") else "")),
        ("Failures", f'{t["failures"]} in {t["failure_groups"]} groups'
                     + (f' ({t["new_failures"]} new in {t["new_failure_groups"]} groups, '
                        f'{t["known_failures"]} known)' if "new_failures" in t else "")),
        ("Turn cap / no winner / no reason", f'{t["max_turns"]} / {t["no_winner"]} / {t["no_reason"]}'),
        ("Games with a chain of 200+ items", t["long_chain"]),
        ("Hung", t["hung"]),
        ("std::bad_alloc only beside another game", t["bad_alloc_only_beside_another_game"]),
        ("Seconds, threads", f'{t["seconds"]:.1f}, {c["threads"]}'),
        ("Games per second", f'{t["games_per_second"]:.0f} ({t["games_per_second_per_thread"]:.0f} per thread)'),
    ]
    ln = j.get("length") or {}
    if ln:
        rows.append(("Decisions per game that ended",
                     f'mean {ln["decisions_mean"]:.0f}, median {ln["decisions_median"]}, '
                     f'p99 {ln["decisions_p99"]}, p99.9 {ln["decisions_p999"]}, max {ln["decisions_max"]}'))
    return table(rows, ("", ""))


def endings(j):
    rows = sorted(j["termination_reasons"].items(), key=lambda kv: -kv[1])
    return table(rows, ("How the game ended", "Games"))


def failures(j):
    out = []
    for i, g in enumerate(j["failure_groups"], 1):
        tag = f'known {g["known"]}' if g.get("known") else "NEW"
        out.append(f'**Group {i} ({tag}): {g["count"]} games** — `{g["key"]}`\n')
        per_card = g.get("fewest_decisions_per_card") or {}
        if per_card:
            rows = [(card, v["count"], v["decisions"], v["legends"], "`" + v["command"] + "`")
                    for card, v in sorted(per_card.items(), key=lambda kv: -kv[1]["count"])]
            out.append(table(rows, ("Card named", "Games", "Fewest decisions", "Decks", "Replay")))
        else:
            ex = g["fewest_decisions_first"][:3]
            rows = [(e["decisions"], e["legends"], e["exception"][:160], "`" + e["command"] + "`") for e in ex]
            out.append(table(rows, ("Decisions", "Decks", "Text", "Replay")))
        out.append("")
    return "\n".join(out) if out else "None."


def flagged(j):
    out = []
    for kind, games in j["flagged"].items():
        if not games:
            continue
        out.append(f"**{kind}: {len(games)} listed**\n")
        rows = [(g["decisions"], g["turns"], g["longest_chain"], g["legends"], "`" + g["command"] + "`")
                for g in games[:10]]
        out.append(table(rows, ("Decisions", "Turns", "Longest chain", "Decks", "Replay")))
        out.append("")
    ln = j.get("length") or {}
    if ln.get("longest"):
        out.append(f'**Longest games that ended** ({ln["games_at_or_over_p999"]} at or over p99.9; first 10)\n')
        rows = [(g["decisions"], g["turns"], g["longest_chain"], g["legends"], "`" + g["command"] + "`")
                for g in ln["longest"][:10]]
        out.append(table(rows, ("Decisions", "Turns", "Longest chain", "Decks", "Replay")))
    return "\n".join(out) if out else "None."


KINDS = ("in_deck", "in_play", "triggered", "activated", "resolved")


def merge_coverage(results):
    cards = {}
    games = 0
    for j in results:
        games += j["coverage"]["games_counted"]
        for c in j["coverage"]["cards"]:
            if c["id"] not in cards:
                cards[c["id"]] = dict(c)
                continue
            m = cards[c["id"]]
            for k in KINDS:
                m["games_" + k] += c["games_" + k]
                m["events_" + k] += c["events_" + k]
    return games, [cards[k] for k in sorted(cards)]


def why_not_in_deck(c):
    if c["banned"]:
        return "banned"
    if c["token_card"]:
        return "token card"
    return "?"


def coverage(games, cards):
    out = []
    n = len(cards)

    def count(pred):
        return sum(1 for c in cards if pred(c))

    has_trig = [c for c in cards if c["has_triggered_ability"]]
    has_act = [c for c in cards if c["has_activated_ability"]]
    spells = [c for c in cards if c["type"] == "Spell"]
    with_ability = [c for c in cards if c["has_triggered_ability"] or c["has_activated_ability"]
                    or c["type"] == "Spell"]
    rows = [
        ("Registered cards", n, ""),
        ("In a deck of at least one game", count(lambda c: c["games_in_deck"] > 0), n),
        ("Played or put into play", count(lambda c: c["games_in_play"] > 0), n),
        ("Has a triggered ability: it triggered", count(lambda c: c["has_triggered_ability"] and c["games_triggered"] > 0), len(has_trig)),
        ("Has an activated ability: it was activated", count(lambda c: c["has_activated_ability"] and c["games_activated"] > 0), len(has_act)),
        ("Spell: it resolved", count(lambda c: c["type"] == "Spell" and c["games_resolved"] > 0), len(spells)),
        ("Has an ability or is a spell: something of it resolved", count(lambda c: c in with_ability and c["games_resolved"] > 0), len(with_ability)),
    ]
    out.append(f"Games counted: {games}\n")
    out.append(table(rows, ("", "Cards", "Of")))

    def listing(title, sel, extra=lambda c: ""):
        out.append(f"\n**{title}: {len(sel)}**\n")
        if not sel:
            return
        rows = [(c["id"], c["name"], c["type"], c["games_in_deck"], c["games_in_play"],
                 c["games_triggered"], c["games_activated"], c["games_resolved"], extra(c)) for c in sel]
        out.append(table(rows, ("Id", "Card", "Type", "In deck", "In play", "Triggered", "Activated",
                                "Resolved", "Note")))

    listing("Never in a deck", [c for c in cards if c["games_in_deck"] == 0], why_not_in_deck)
    listing("In a deck, never in play",
            [c for c in cards if c["games_in_deck"] > 0 and c["games_in_play"] == 0])
    listing("Has a triggered ability, in play, never triggered",
            [c for c in has_trig if c["games_in_play"] > 0 and c["games_triggered"] == 0])
    listing("Has an activated ability, in play, never activated",
            [c for c in has_act if c["games_in_play"] > 0 and c["games_activated"] == 0])
    listing("Spell, in a deck, never resolved",
            [c for c in spells if c["games_in_deck"] > 0 and c["games_resolved"] == 0])
    listing("Triggered or activated, never resolved",
            [c for c in cards if c["type"] != "Spell" and
             (c["games_triggered"] > 0 or c["games_activated"] > 0) and c["games_resolved"] == 0])
    return "\n".join(out)


def main():
    results = [json.load(open(p)) for p in sys.argv[1:]]
    if not results:
        print(__doc__)
        return 2
    for path, j in zip(sys.argv[1:], results):
        print(f"## {path}\n")
        print(totals(j) + "\n")
        print(endings(j) + "\n")
        print("### Failures\n")
        print(failures(j) + "\n")
        print("### Flagged and longest games\n")
        print(flagged(j) + "\n")
    games, cards = merge_coverage(results)
    if cards:
        print("## Coverage" + (" (all results together)" if len(results) > 1 else "") + "\n")
        print(coverage(games, cards))
    return 0


if __name__ == "__main__":
    sys.exit(main())
