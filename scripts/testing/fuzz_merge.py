#!/usr/bin/env python3
"""Merge `deck_fuzz --json` results of one pass that was run in chunks, and
check a result against the stopping rule's coverage minimum.

  fuzz_merge.py merge <out.json> <chunk.json> [<chunk.json> ...]
      One result with the schema of a single run, so deck_fuzz_report.py and
      deck_fuzz_compare.py read it: totals and termination reasons summed,
      failure groups merged by key (counts and cards named summed, examples
      and per-card fewest-decision games kept by fewest decisions), flagged
      games concatenated, per-card coverage summed. Length: mean and turn mean
      weighted by completed games, max the max; median / p99 / p99.9 are the
      maximum over the chunks (an upper bound, marked "merged_bound"), and the
      longest games are the union of the chunks' lists sorted by decisions
      (every game of the whole run's top 0.1% is in some chunk's top 0.1%).
      `config` is the first chunk's, with `seeds` listing every chunk's seed.

  fuzz_merge.py check <result.json> <K> [--per-million]
      Every card that is neither banned nor a token card, and every way that
      applies to it (in play: all; triggered: has a triggered ability;
      activated: has an activated ability; resolved: a spell or a card with
      an ability), with fewer than K games. --per-million scales K by
      games_counted / 1,000,000. Prints the list (TSV) and exits 1 if any.

  fuzz_merge.py table <result.json>
      Per-card coverage as TSV: id, name, type, games in deck / in play /
      triggered / activated / resolved, and which ways apply.
"""
import json, sys

KINDS = ("in_deck", "in_play", "triggered", "activated", "resolved")


def merge(out_path, paths):
    rs = [json.load(open(p)) for p in paths]
    m = json.loads(json.dumps(rs[0]))
    m["config"]["seeds"] = [r["config"]["seed"] for r in rs]
    m["config"]["chunks"] = paths
    # totals
    t = {}
    for r in rs:
        for k, v in r["totals"].items():
            if isinstance(v, (int, float)):
                t[k] = t.get(k, 0) + v
    t["games_per_second"] = t["games"] / t["seconds"] if t.get("seconds") else 0
    t["games_per_second_per_thread"] = t["games_per_second"] / max(1, m["config"]["threads"])
    # termination reasons
    tr = {}
    for r in rs:
        for k, v in r["termination_reasons"].items():
            tr[k] = tr.get(k, 0) + v
    m["termination_reasons"] = tr
    # failure groups
    groups = {}
    for r in rs:
        for g in r["failure_groups"]:
            k = g["key"]
            if k not in groups:
                groups[k] = json.loads(json.dumps(g))
                continue
            h = groups[k]
            h["count"] += g["count"]
            for c, n in g.get("cards_named", {}).items():
                h["cards_named"][c] = h["cards_named"].get(c, 0) + n
            h["fewest_decisions_first"] = sorted(
                h["fewest_decisions_first"] + g["fewest_decisions_first"],
                key=lambda e: e["decisions"])[:max(len(h["fewest_decisions_first"]), len(g["fewest_decisions_first"]))]
            for c, v in g.get("fewest_decisions_per_card", {}).items():
                old = h.setdefault("fewest_decisions_per_card", {}).get(c)
                if old is None:
                    h["fewest_decisions_per_card"][c] = v
                else:
                    keep = v if v["decisions"] < old["decisions"] else old
                    keep = dict(keep)
                    keep["count"] = old["count"] + v["count"]
                    h["fewest_decisions_per_card"][c] = keep
    m["failure_groups"] = sorted(groups.values(), key=lambda g: -g["count"])
    t["failure_groups"] = len(groups)
    if any("new_failure_groups" in r["totals"] for r in rs):
        t["new_failure_groups"] = sum(1 for g in groups.values() if not g.get("known"))
    m["totals"] = t
    # flagged, hung
    m["flagged"] = {k: [] for k in rs[0]["flagged"]}
    for r in rs:
        for k, v in r["flagged"].items():
            m["flagged"].setdefault(k, []).extend(v)
    m["hung"] = [h for r in rs for h in r.get("hung", [])]
    # length
    done = [r["totals"]["completed"] for r in rs]
    L = {}
    tot = sum(done)
    for key in ("decisions_mean", "turns_mean"):
        if all(key in r["length"] for r in rs):
            L[key] = sum(r["length"][key] * n for r, n in zip(rs, done)) / tot
    L["decisions_max"] = max(r["length"]["decisions_max"] for r in rs)
    for key in ("decisions_median", "decisions_p99", "decisions_p999"):
        L[key] = max(r["length"][key] for r in rs)
    L["merged_bound"] = "median, p99 and p99.9 are the maximum over the chunks"
    longest = sorted((g for r in rs for g in r["length"].get("longest", [])),
                     key=lambda g: -g["decisions"])
    L["games_at_or_over_p999"] = sum(r["length"]["games_at_or_over_p999"] for r in rs)
    L["longest"] = longest
    m["length"] = L
    # coverage
    cards = {}
    for r in rs:
        for c in r["coverage"]["cards"]:
            if c["id"] not in cards:
                cards[c["id"]] = dict(c)
                continue
            h = cards[c["id"]]
            for k in KINDS:
                h["games_" + k] += c["games_" + k]
                h["events_" + k] += c["events_" + k]
    cl = [cards[k] for k in sorted(cards)]
    cov = m["coverage"]
    cov["cards"] = cl
    cov["games_counted"] = sum(r["coverage"]["games_counted"] for r in rs)
    cov["cards_in_a_deck"] = sum(1 for c in cl if c["games_in_deck"] > 0)
    cov["cards_never_in_play"] = sum(1 for c in cl if c["games_in_play"] == 0)
    cov["cards_never_triggered"] = sum(1 for c in cl if c["games_triggered"] == 0)
    cov["cards_never_activated"] = sum(1 for c in cl if c["games_activated"] == 0)
    cov["cards_never_resolved"] = sum(1 for c in cl if c["games_resolved"] == 0)
    json.dump(m, open(out_path, "w"), indent=1, sort_keys=True)


def ways(c):
    w = ["in_play"]
    if c["has_triggered_ability"]:
        w.append("triggered")
    if c["has_activated_ability"]:
        w.append("activated")
    if c["type"] == "Spell" or c["has_triggered_ability"] or c["has_activated_ability"]:
        w.append("resolved")
    return w


def check(path, k, per_million):
    r = json.load(open(path))
    n = r["coverage"]["games_counted"]
    kk = k * n / 1e6 if per_million else k
    low = []
    for c in r["coverage"]["cards"]:
        if c["banned"] or c["token_card"]:
            continue
        for w in ways(c):
            if c["games_" + w] < kk:
                low.append((c["id"], c["name"], c["type"], w, c["games_" + w]))
    print(f"# games counted {n}, minimum {kk:g} games per way; {len(low)} below")
    for row in low:
        print("\t".join(str(x) for x in row))
    return 1 if low else 0


def table(path):
    r = json.load(open(path))
    print("id\tname\ttype\tin_deck\tin_play\ttriggered\tactivated\tresolved\tways\tbanned\ttoken")
    for c in r["coverage"]["cards"]:
        print("\t".join(str(x) for x in (c["id"], c["name"], c["type"], c["games_in_deck"], c["games_in_play"],
                                         c["games_triggered"], c["games_activated"], c["games_resolved"],
                                         ",".join(ways(c)), int(c["banned"]), int(c["token_card"]))))


if __name__ == "__main__":
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__)
    if a[0] == "merge":
        merge(a[1], a[2:])
    elif a[0] == "check":
        sys.exit(check(a[1], float(a[2]), "--per-million" in a))
    elif a[0] == "table":
        table(a[1])
    else:
        sys.exit(__doc__)
