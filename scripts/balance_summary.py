#!/usr/bin/env python3
"""Summarise balance CSVs: first-player and seat-1 rates, per-list tables."""
import csv, math, sys
from collections import defaultdict


def wilson(w, n):
    if n == 0:
        return (0.0, 1.0)
    z = 1.96
    p = w / n
    d = 1 + z * z / n
    c = (p + z * z / (2 * n)) / d
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / d
    return (max(0.0, c - h), min(1.0, c + h))


def fmt(w, n):
    if n == 0:
        return "-"
    lo, hi = wilson(w, n)
    return f"{w / n:.3f} [{lo:.3f}-{hi:.3f}]"


def load(paths):
    rows = []
    for p in paths:
        with open(p) as f:
            rows.extend(csv.DictReader(f))
    return rows


def headline(label, rows):
    ok = [r for r in rows if not r["error"] and r["first"] in ("1", "2")]
    err = len(rows) - len(ok)
    draws = sum(1 for r in ok if r["winner"] == "0")
    # A draw counts half.
    fw = sum(1.0 if r["winner"] == r["first"] else 0.5 if r["winner"] == "0" else 0.0 for r in ok)
    s1 = sum(1.0 if r["winner"] == "1" else 0.5 if r["winner"] == "0" else 0.0 for r in ok)
    f1 = sum(1 for r in ok if r["first"] == "1")
    n = len(ok)
    print(f"| {label} | {n} | {err} | {draws} | {fmt(fw, n)} | {fmt(s1, n)} | {f1 / n:.3f} |")


def per_list(label, rows):
    stat = defaultdict(lambda: [0.0, 0, 0.0, 0, 0.0, 0])  # all w,n ; first w,n ; second w,n
    for r in rows:
        if r["error"] or r["first"] not in ("1", "2"):
            continue
        for seat, deck in (("1", r["deck1"]), ("2", r["deck2"])):
            s = 1.0 if r["winner"] == seat else 0.5 if r["winner"] == "0" else 0.0
            st = stat[deck]
            st[0] += s; st[1] += 1
            if r["first"] == seat:
                st[2] += s; st[3] += 1
            else:
                st[4] += s; st[5] += 1
    print(f"\n**Per deck list, {label}** (win rate [95% CI])\n")
    print("| list | games | overall | going first | going second | |")
    print("|---|---|---|---|---|---|")
    lop = []
    for deck, st in sorted(stat.items(), key=lambda kv: -kv[1][0] / kv[1][1]):
        lo, hi = wilson(st[0], st[1])
        mark = "strong" if lo > 0.5 else "weak" if hi < 0.5 else ""
        if mark:
            lop.append((deck, st[0] / st[1], mark))
        print(f"| {deck} | {st[1]} | {fmt(st[0], st[1])} | {fmt(st[2], st[3])} | {fmt(st[4], st[5])} | {mark} |")
    return lop


def main():
    groups = {}
    for arg in sys.argv[1:]:
        label, paths = arg.split("=", 1)
        groups[label] = load(paths.split("+"))
    print("| agent / decks | games | errors | draws | player going first wins | seat 1 wins | seat 1 went first |")
    print("|---|---|---|---|---|---|---|")
    for label, rows in groups.items():
        headline(label, rows)
    for label, rows in groups.items():
        if "lists" in label and "ismcts" not in label:
            lop = per_list(label, rows)
            print(f"\nLopsided ({label}): " + ", ".join(f"{d} {r:.3f} ({m})" for d, r, m in lop))
    for label, rows in groups.items():
        errs = [r for r in rows if r["error"]]
        for r in errs[:5]:
            print(f"error {label}: game {r['game']} {r['deck1']} v {r['deck2']} seed {r['engine_seed']}: {r['error']}")


main()
