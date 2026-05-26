#!/usr/bin/env python3
"""Card implementation coverage gate / dashboard.

Classifies every src/cards/<type>/<id>_<slug>.cpp by what its code contains:
  - implemented   : has effect code (onResolve/onTrigger/onActivate/onEquip/applyPassiveAura/...)
  - metadata-only : declares targeting/trigger/cost but NO effect body  -> incomplete
  - stub          : only def(), yet the card has real (non-keyword) ability text -> no-op
  - vanilla       : keywords/stats only; correctly needs no code

Cross-cuts each with in-code gap comments:
  - engine-gap    : strong markers (cannot / engine gap / not implemented / needs a new ...)
  - partial       : soft markers (TODO / simplified / approximate / for now)

Exit code is non-zero while any card is a `stub` or `metadata-only` (the
unambiguous "does nothing" buckets) so this can act as a CI completeness gate.
Writes the work lists to docs/card-coverage/ for batch pickup.

Usage:  python3 scripts/card_coverage.py [--write-lists]
"""
import re, sys, json
from pathlib import Path
from collections import Counter

ROOT = Path(__file__).resolve().parent.parent
CARDS = ROOT / "src/cards"

# Overrides that, by themselves, implement a card's ability.
EFFECT = ["onResolve(", "onTrigger(", "onActivate(", "onEquip(", "applyPassiveAura(",
          "onDeath(", "onCounter(", "onPlay(",
          "entersReadyOnPlay(", "selfCostReduction(", "activationCostReduction(",
          "minTurnToScore(", "canBeChosenByEnemy(", "playableAsReactionToAttack(",
          "requiresLegion("]
# Declaration-only overrides: they describe an ability but need a paired effect
# body (onResolve/onActivate/onTrigger) to actually DO something.
META = ["getTargetRequirements(", "needsPlayTimeTarget(", "triggerType(", "triggerTypes(",
        "getActivationCost(", "hasActivatedAbility(", "isReactionAbility(", "isActionAbility(",
        "canActivateAbility(", "needsPlayTimeTargetPair(", "activatedAbilities(",
        "enumerateLegalTargets("]
STRONG = re.compile(r"(?i)(cannot|can't implement|can not be implemented|engine gap|"
                    r"not implemented|unimplemented|engine limitation|needs a new|no engine|"
                    r"requires? .*primitive|not supported)")
WEAK = re.compile(r"(?i)(todo|fixme|simplified|approximat|workaround|not yet|placeholder|"
                  r"best-effort|for now)")
ATEXT = re.compile(r'd\.ability_text = R"([A-Za-z0-9]*)\((.*?)\)\1";', re.S)
KW = re.compile(r'\[[^\]]*\]')
PAREN = re.compile(r'\([^()]*\)')


def residual(a):
    r, prev = a, None
    while prev != r:
        prev = r
        r = PAREN.sub('', r)
    return KW.sub('', r).strip()


def classify():
    rows = []
    for p in sorted(CARDS.rglob("[0-9]*.cpp")):
        t = p.read_text()
        eff = any(s in t for s in EFFECT)
        meta = any(s in t for s in META)
        m = ATEXT.search(t)
        resid = residual(m.group(2) if m else "")
        real = len(resid) >= 4
        bucket = ("implemented" if eff else "metadata-only" if meta
                  else "stub" if real else "vanilla")
        flag = ("engine-gap" if STRONG.search(t) else "partial" if WEAK.search(t) else "clean")
        rows.append({"file": str(p.relative_to(ROOT)), "name": p.name,
                     "bucket": bucket, "flag": flag, "ability": resid[:90]})
    return rows


def main():
    rows = classify()
    B = Counter(r["bucket"] for r in rows)
    ct = Counter((r["bucket"], r["flag"]) for r in rows)
    incomplete = [r for r in rows if r["bucket"] in ("stub", "metadata-only")]
    gap = [r for r in rows if r["flag"] == "engine-gap"]

    print(f"{'bucket':16}{'engine-gap':>11}{'partial':>9}{'clean':>7}{'total':>7}")
    for b in ["implemented", "metadata-only", "stub", "vanilla"]:
        print(f"{b:16}{ct[(b,'engine-gap')]:>11}{ct[(b,'partial')]:>9}"
              f"{ct[(b,'clean')]:>7}{B[b]:>7}")
    print(f"{'TOTAL':16}{'':>11}{'':>9}{'':>7}{sum(B.values()):>7}")
    print(f"\nINCOMPLETE (stub+metadata-only): {len(incomplete)}   "
          f"engine-gap flagged: {len(gap)}")

    if "--write-lists" in sys.argv:
        out = ROOT / "docs/card-coverage"
        out.mkdir(parents=True, exist_ok=True)
        (out / "incomplete.json").write_text(json.dumps(incomplete, indent=1))
        (out / "engine-gap.json").write_text(json.dumps(gap, indent=1))
        print(f"wrote {out}/incomplete.json ({len(incomplete)}), engine-gap.json ({len(gap)})")

    return 1 if incomplete else 0


if __name__ == "__main__":
    sys.exit(main())
