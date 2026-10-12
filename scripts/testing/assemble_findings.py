#!/usr/bin/env python3
"""Assemble docs/testing-findings-decks-2026-10.md from its parts.

Usage: assemble_findings.py <parts-dir> <out.md> [--readers 1,2,3]

Parts (all in <parts-dir>):
  head.md             title, summary, method, deck-level checks (hand-written)
  inventory.md        write_inventory_md.py
  lead_findings.md    the lead's findings, "### D<n>  ..." blocks
  reader_<k>.md       one per reader: ## CARDS / ## FINDINGS (### R<k>-<n>) /
                      ## CHECKED AND CORRECT / ## QUESTIONS / ## NOT OBSERVED /
                      ## DECK INTERACTIONS
  merge.tsv           "R<k>-<n> <tab> D<m> | R<j>-<i>": a reader finding that is
                      the same defect as another finding; its block is put under
                      that finding as further evidence instead of getting a number
  tail.md             closing notes (optional)

Reader findings that are not merged are numbered D<next> in reader order, the
old id kept in the heading. Every "R<k>-<n>" mentioned anywhere in the output
is followed by its D number.
"""
import os, re, sys

def sections(text):
    out, cur = {}, None
    for line in text.split("\n"):
        m = re.match(r'^## (.+?)\s*$', line)
        if m:
            cur = m.group(1).strip().upper()
            out[cur] = []
            continue
        if cur:
            out[cur].append(line)
    return {k: "\n".join(v).strip("\n") for k, v in out.items()}

def blocks(text, prefix):
    parts = re.split(r'\n(?=### ' + prefix + r')', "\n" + text)
    res = []
    for p in parts:
        m = re.match(r'\n?### (' + prefix + r'[\w\-]*)\s+(.*)', p)
        if m:
            body = p.split("\n", 2)
            res.append((m.group(1), m.group(2).strip(), p.split("\n", 2)[2] if len(body) > 2 else ""))
    return res

def main():
    pdir, outp = sys.argv[1:3]
    readers = [1, 2, 3, 4, 5, 6, 7, 8, 9]
    if "--readers" in sys.argv:
        readers = [int(x) for x in sys.argv[sys.argv.index("--readers") + 1].split(",")]
    rd = lambda n: open(os.path.join(pdir, n), encoding="utf-8").read() if os.path.exists(os.path.join(pdir, n)) else ""
    merge = {}
    for line in rd("merge.tsv").split("\n"):
        if line.strip() and not line.startswith("#"):
            a, b = line.split("\t")[:2]
            merge[a.strip()] = b.strip()
    lead = blocks(rd("lead_findings.md"), "D")
    findings = [[i, t, b.rstrip(), []] for i, t, b in lead]          # id, title, body, extra
    byid = {f[0]: f for f in findings}
    nextd = max(int(f[0][1:]) for f in findings) + 1
    rmap = {}
    rsec = {}
    pending_merge = []
    for k in readers:
        txt = rd(f"reader_{k}.md")
        if not txt:
            continue
        s = sections(txt)
        rsec[k] = s
        for rid, title, body in blocks(s.get("FINDINGS", ""), "R"):
            if rid in merge:
                pending_merge.append((rid, title, body, k))
                continue
            did = f"D{nextd}"
            nextd += 1
            rmap[rid] = did
            f = [did, f"{title}  _(reader {k}: {rid})_", body.rstrip(), []]
            findings.append(f)
            byid[did] = f
            byid[rid] = f
    for rid, title, body, k in pending_merge:
        tgt = merge[rid]
        tgt = rmap.get(tgt, tgt)
        rmap[rid] = tgt
        if tgt in byid:
            byid[tgt][3].append(f"**Further evidence — reader {k}, {rid}: {title}**\n\n{body.rstrip()}")
        else:
            print("merge target missing:", rid, "->", tgt)
    def relabel(text):
        return re.sub(r'\bR(\d)-(\d+)\b', lambda m: (f"{rmap[m.group(0)]} ({m.group(0)})" if m.group(0) in rmap else m.group(0)), text)
    out = [rd("head.md").rstrip(), ""]
    out += ["## 3. Inventory", "", rd("inventory.md").rstrip(), ""]
    out += ["## 4. Coverage: every card of the lists against a trace", "",
            "One table per reader (the lists whose cards that reader owned: a card is read once, by the reader of the first list that holds it).",
            "`read`: FULL = every clause of the printed text was compared against a traced instance that exercised it; PART = the named clause was not seen at work; NONE = not read.", ""]
    for k in readers:
        if k in rsec:
            head = re.search(r'^# (.*)$', rd(f"reader_{k}.md"), re.M)
            out += [f"### {head.group(1) if head else 'reader ' + str(k)}", "", relabel(rsec[k].get("CARDS", "")), ""]
    out += ["## 5. Findings", ""]
    for did, title, body, extra in findings:
        out += [f"### {did}  {relabel(title) if did not in ('',) else title}", "", relabel(body), ""]
        for e in extra:
            out += [relabel(e), ""]
    for name, key, num in (("Checked and correct", "CHECKED AND CORRECT", 6), ("Judge questions", "QUESTIONS", 7),
                           ("Not observed", "NOT OBSERVED", 8), ("Deck interactions", "DECK INTERACTIONS", 9)):
        out += [f"## {num}. {name}", ""]
        if key == "CHECKED AND CORRECT":
            out += [rd("lead_checked.md").rstrip(), ""]
        if key == "QUESTIONS":
            out += [rd("lead_questions.md").rstrip(), ""]
        for k in readers:
            if k in rsec and rsec[k].get(key, "").strip():
                out += [f"**Reader {k}**", "", relabel(rsec[k][key]), ""]
    out += [rd("tail.md").rstrip(), ""]
    open(outp, "w", encoding="utf-8").write("\n".join(out))
    print(len(findings), "findings;", len(pending_merge), "merged blocks;", sum(1 for _ in open(outp)), "lines")
    with open(os.path.join(pdir, "idmap.tsv"), "w") as f:
        for r, d in sorted(rmap.items()):
            f.write(f"{r}\t{d}\n")

if __name__ == "__main__":
    main()
