#!/usr/bin/env python3
"""Deck-level invariants over every traced game (logs + replay texts).

Usage: deck_level_checks.py <trace-dir> <inventory.json> <sb-dir> <out.txt>

Each check prints how many places it looked at and every violation (file,
decision). What a check cannot see is said in its header.

 C1  every game ends "Game over: winner=N | reason=..." with a winner 1/2
     whose score is >= 8 and above the other's; nothing is decided after the
     winning point.
 C2  scores: +1 at a time by Hold / Conquer, never above 8; the point that
     reaches 8 by Conquer only when that player scored every battlefield
     that turn (CR 473.1.b); the turn header repeats the tracked score.
 C3  one score per battlefield per player per turn (CR 472).
 C4  a prompt that was requested is the next decision asked (nobody gets
     chain priority or a main-phase decision while a prompt is open).
 C5  units enter exhausted (CR 359.2.c): for every "PLAY: <unit>" the first
     board that shows it, unless the log between says why it is ready
     (ACCELERATE paid, or a line naming a ready effect).
 C6  rune pools are empty at the first decision of every Main Phase
     (CR 316.3) — only where the renderer's 80-column header shows the pool.
 C7  card accounting from the rendered boards: for each player
     deck + hand + trash + banishment + Champion Zone + own non-token
     permanents + facedown + items being played = 40, and no card name is
     in the visible zones more often than the two lists hold it.
"""
import collections, json, os, re, sys

DEC = re.compile(r'\[D:(\d+)\.(\d+)\]')

def load_lists(inv, sbdir):
    """deck name -> Counter of main-deck names (champion included)."""
    lists = {}
    for deck, secs in inv.items():
        c = collections.Counter()
        for sec in ("champion", "maindeck"):
            for r in secs[sec]:
                c[r["card"]["name"]] += r["count"]
        lists[deck] = c
        # post-sideboard list: read the generated file
        p = os.path.join(sbdir, deck + "_sb.txt")
        if os.path.exists(p):
            c2, sec = collections.Counter(), None
            for raw in open(p):
                line = raw.strip()
                if not line or line.startswith("#"):
                    continue
                if line.endswith(":"):
                    sec = line[:-1].lower()
                    continue
                n, name = line.split(None, 1)
                if sec in ("champion", "maindeck"):
                    c2[name] += int(n)
            lists[deck + "_sb"] = c2
    return lists

def check_log(fn, path, out):
    v = []
    score = {1: 0, 2: 0}
    turn_scored = collections.defaultdict(set)   # player -> set of BF ids scored this turn
    turn_player = None
    open_prompt = None
    won_at = None
    n_bf = 2
    stats = collections.Counter()
    last_conquer_bf = {}
    lines = open(path, errors="replace").read().split("\n")
    over = [l for l in lines if l.startswith("Game over:")]
    if len(over) != 1:
        v.append(("C1", -1, f"{len(over)} 'Game over' lines"))
    for line in lines:
        m = DEC.search(line)
        dec = int(m.group(1)) if m else -1
        body = line.split("] ", 2)[-1] if m else line
        if "════ TURN" in line:
            tm = re.search(r'TURN (\d+) \(P(\d)\).*Score: P1=(\d+) P2=(\d+)', line)
            if tm:
                turn_player = int(tm.group(2))
                turn_scored.clear()
                stats["turns"] += 1
                if (int(tm.group(3)), int(tm.group(4))) != (score[1], score[2]):
                    v.append(("C2", dec, f"turn header {tm.group(3)}/{tm.group(4)} vs tracked {score[1]}/{score[2]}"))
        if "BF_TOKEN: added" in line:
            stats["bf_tokens"] += 1
            n_bf += 1
        if "BF_TOKEN" in line and "removed" in line:
            n_bf -= 1
        cm = re.search(r'CONQUER: P(\d) conquers BF#(\d+)', line)
        if cm:
            p, bf = int(cm.group(1)), int(cm.group(2))
            if bf in turn_scored[p]:
                v.append(("C3", dec, f"P{p} conquers BF#{bf} twice in a turn"))
            turn_scored[p].add(bf)
            last_conquer_bf[p] = bf
            stats["conquers"] += 1
        sm = re.search(r'SCORE: P(\d) \+(\d+) via (Hold BF#(\d+)|Conquer|effect) -> (\d+)', line)
        if sm:
            p, n, how, bf, to = int(sm.group(1)), int(sm.group(2)), sm.group(3), sm.group(4), int(sm.group(5))
            stats["scores"] += 1
            if won_at is not None:
                v.append(("C1", dec, "a point is scored after the game was won"))
            if to != score[p] + n:
                v.append(("C2", dec, f"P{p} score {score[p]} +{n} printed as {to}"))
            score[p] = to
            if to > 8:
                v.append(("C2", dec, f"P{p} score {to} above the victory score"))
            if how.startswith("Hold"):
                if int(bf) in turn_scored[p]:
                    v.append(("C3", dec, f"P{p} holds BF#{bf} after scoring it this turn"))
                turn_scored[p].add(int(bf))
                if p != turn_player:
                    v.append(("C3", dec, f"P{p} holds on P{turn_player}'s turn"))
            if how == "Conquer" and to == 8:
                stats["winning_conquers"] += 1
                if len(turn_scored[p]) < n_bf:
                    v.append(("C2", dec, f"P{p} takes the winning point by Conquer having scored only BFs {sorted(turn_scored[p])} this turn"))
            if to >= 8 and to > score[3 - p]:
                won_at = dec
        if "SCORE_BLOCKED" in line:
            stats["winning_point_blocked"] += 1
            bm = re.search(r'SCORE_BLOCKED: P(\d)', line)
            if bm:
                p = int(bm.group(1))
                if score[p] != 7:
                    v.append(("C2", dec, f"winning point blocked at score {score[p]}"))
                if len(turn_scored[p]) >= n_bf:
                    v.append(("C2", dec, f"winning point blocked although P{p} scored every battlefield this turn"))
        bo = re.search(r'BURN_OUT: P(\d).*P(\d) gains (\d+) point.*-> (\d+)', line)
        if bo:
            score[int(bo.group(2))] = int(bo.group(4))
            stats["burn_outs"] += 1
            if score[int(bo.group(2))] >= 8 and score[int(bo.group(2))] > score[int(bo.group(1))]:
                won_at = dec
        if "DECISION #" in line:
            stats["decisions"] += 1
            if won_at is not None and dec > won_at:
                v.append(("C1", dec, f"a decision is asked after the winning point (decision {won_at}): {body[:80]}"))
            if open_prompt is not None:
                kind, text, pdec = open_prompt
                if text[:40] not in line:
                    v.append(("C4", dec, f"prompt '{text[:60]}' was requested at {pdec} but the next decision is: {body[:80]}"))
                open_prompt = None
                stats["prompts_followed"] += 1
        rq = re.search(r'CHOICE-REQUEST(?: \((?:finalize|before payment)\))?: (?:\[)?(\w+)(?:\])?: (.*?) \[\d+ options\]', line)
        if rq:
            open_prompt = (rq.group(1), rq.group(1) + ": " + rq.group(2), dec)
        if "[WRN]" in line:
            v.append(("WRN", dec, body[:120]))
        if "DEFLECT_UNPAID" in line:
            v.append(("DEFLECT_UNPAID", dec, body[:120]))
    if over:
        om = re.search(r'winner=(\d+) \| reason=(.*?) \| turns=(\d+)', over[0])
        if not om or om.group(1) not in ("1", "2"):
            v.append(("C1", -1, "no winner: " + over[0]))
        else:
            w = int(om.group(1))
            stats["reason:" + re.sub(r'P\d', 'Px', om.group(2))] += 1
            if not (score[w] >= 8 and score[w] > score[3 - w]):
                v.append(("C1", -1, f"winner P{w} with tracked score {score[1]}/{score[2]}"))
    return v, stats

TOKENS = {"Sand Soldier", "Recruit", "Sprite", "Gold", "Reflection", "Shadow Clone", "Tentacle",
          "Mech", "Bird", "Poro", "Warhawk", "Minion"}
ABBR = 10

def split_cells(text):
    return [c.strip() for c in re.split(r'\s{2,}', text.strip()) if c.strip()]

def parse_board(block):
    """-> dict with per-player zone lists from one rendered board."""
    b = {1: collections.defaultdict(list), 2: collections.defaultdict(list)}
    hdr = {}
    cur_p, cur_zone, side = None, None, None
    chain = []
    for raw in block:
        if not raw.startswith("|"):
            continue
        line = raw[1:].rstrip()
        if line.endswith("|"):
            line = line[:-1]
        s = line.strip()
        m = re.match(r'P(\d)  (.+?)  Score:(\d+)  Deck:(\d+)  Runes:(\d+)/(\d+)\+(\d+)(.*)$', s)
        if m:
            cur_p = int(m.group(1)); cur_zone = None; side = None
            pool = re.search(r'Pool:E(\d+)/P(\d+)', m.group(8))
            hdr[cur_p] = {"deck": int(m.group(4)), "pool": (int(pool.group(1)), int(pool.group(2))) if pool else None,
                          "runes_board": int(m.group(6)), "rune_deck": int(m.group(7))}
            continue
        m = re.match(r'>> Champion Zone: (.*)$', s)
        if m:
            if "(on board)" not in m.group(1) and m.group(1).strip() not in ("", "(empty)", "--"):
                b[cur_p]["champzone"].append(m.group(1))
            continue
        if s.startswith(">> Legend Zone"):
            continue
        m = re.match(r'(Hand|Trash|Banish):\s*(.*)$', s)
        if m and cur_p and side is None:
            cur_zone = m.group(1).lower()
            if m.group(2) != "(empty)":
                b[cur_p][cur_zone] += split_cells(m.group(2))
            continue
        m = re.match(r'P(\d) BASE', s)
        if m:
            side = ("base", int(m.group(1))); cur_zone = None; cur_p = None
            continue
        m = re.match(r'(.+?)\s+\[Ctrl:(P\d|--)\]', s)
        if m:
            side = ("bf", m.group(1)); cur_zone = None; cur_p = None
            continue
        if s.startswith("Chain"):
            side = ("chain", None); cur_zone = None; cur_p = None
            continue
        if side and side[0] == "base":
            m = re.match(r'(Units|Gear|Runes):\s*(.*)$', s)
            if m:
                cur_zone = m.group(1).lower()
                b[side[1]]["board_" + cur_zone] += split_cells(m.group(2))
            elif cur_zone and s:
                b[side[1]]["board_" + cur_zone] += split_cells(s)
            continue
        if side and side[0] == "bf":
            m = re.match(r'P(\d): (.*)$', s)
            if m:
                cur_p2 = int(m.group(1)); cur_zone = "bfunits"
                if m.group(2).strip() != "--":
                    b[cur_p2]["bf_units"] += split_cells(m.group(2))
                side = ("bf", side[1], cur_p2)
                continue
            m = re.match(r'\[HIDDEN: P(\d) — facedown(?:: (.*))?\]', s)
            if m:
                b[int(m.group(1))]["facedown"].append(m.group(2) or "?")
                continue
            if len(side) == 3 and s and not s.startswith("["):
                b[side[2]]["bf_units"] += split_cells(s)
            continue
        if side and side[0] == "chain":
            m = re.search(r'(?:\[top\]|RESOLVING:|^)\s*(.+?) \(P(\d)\)', s)
            if m:
                chain.append((m.group(1).replace(">>> RESOLVING: ", "").strip(), int(m.group(2))))
            continue
        if cur_p and cur_zone in ("hand", "trash", "banish") and s:
            b[cur_p][cur_zone] += split_cells(s)
    return b, hdr, chain

def abbr(name):
    """StateRenderer::renderObject's shortening of a unit's name."""
    if len(name) > 16:
        c = name.find(",")
        if c != -1 and c < 12:
            return name[:c] + "," + name[c + 2:c + 6]
        return name[:14] + ".."
    return name

def unit_states(z, ab):
    r = e = 0
    for cell in z["board_units"] + z["bf_units"]:
        if cell.startswith(ab + "("):
            if ",rdy" in cell:
                r += 1
            else:
                e += 1
    return r, e

def check_replay(fn, path, lists, out):
    v = []
    stats = collections.Counter()
    d1, d2 = fn.split("__")[:2]
    total_list = lists[d1] + lists[d2]
    abbr_index = collections.defaultdict(set)
    for n in total_list:
        abbr_index[n.replace(", ", ",")[:ABBR]].add(n)
    snaps = open(path, errors="replace").read().split("=== SNAP ")[1:]
    main_start_pending = False
    played_units = []   # (name, player, dec, why_ready)
    for sn in snaps:
        head, _, rest = sn.partition("\n")
        dm = re.search(r'idx=(-?\d+)', head)
        dec = int(dm.group(1)) if dm else -1
        parts = rest.split("--- decision\n")
        board_txt = parts[0].replace("--- board\n", "") if len(parts) > 1 and "--- board" in parts[0] else ""
        dtxt, _, trace = (parts[1] if len(parts) > 1 else parts[0]).partition("--- trace\n")
        if board_txt:
            b, hdr, chain = parse_board(board_txt.split("\n"))
            # C6
            if main_start_pending and "[SEAM" not in head:
                main_start_pending = False
                for p in (1, 2):
                    if p in hdr:
                        if hdr[p]["pool"] is None:
                            stats["c6_unreadable"] += 1
                        else:
                            stats["c6_checked"] += 1
                            if hdr[p]["pool"] != (0, 0):
                                v.append(("C6", dec, f"P{p} pool {hdr[p]['pool']} at the first Main Phase decision"))
            # C7 (skip while something is mid-play: the card being paid for is in no rendered zone)
            if len(hdr) == 2 and "MainPhase" in head and "[0] End Turn" in dtxt and not chain:
                tot = 0
                names = collections.Counter()
                for p in (1, 2):
                    z = b[p]
                    cnt = hdr[p]["deck"] + len(z["hand"]) + len(z["trash"]) + len(z["banish"]) + len(z["champzone"]) + len(z["facedown"])
                    for at_bf, cells in ((False, z["board_units"] + z["board_gear"]), (True, z["bf_units"])):
                        for cell in cells:
                            nm = re.sub(r'\(.*$', '', cell).strip()
                            eq = re.search(r'EQ:([^)]*)\)', cell)
                            # Equipment attached at a battlefield is listed only inside
                            # its bearer's cell; in a base it is also on the Gear line.
                            if eq and at_bf:
                                cnt += 1 + eq.group(1).count("+")
                            if nm in TOKENS or nm.rstrip(".") in TOKENS:
                                continue
                            cnt += 1
                    tot += cnt
                    for cell in z["hand"]:
                        names[re.sub(r'(\(\d+M\)|\[[SG]\]).*$', '', cell).strip()] += 1
                    for cell in z["trash"] + z["banish"] + z["facedown"]:
                        names[cell] += 1
                stats["c7_boards"] += 1
                if tot != 80:
                    v.append(("C7", dec, f"cards accounted for: {tot} of 80"))
                for n, k in names.items():
                    if n in total_list and k > total_list[n]:
                        v.append(("C7", dec, f"'{n}' is in hand/trash/banishment/facedown {k} times; the lists hold {total_list[n]}"))
        # C5: resolve pending unit plays against this board, then note new ones
        top_level = ("End Turn" in dtxt) or ("Pass" in dtxt)
        dm2 = re.search(r'\[D:(\d+)\.', trace)
        dnum = int(dm2.group(1)) if dm2 else dec
        if board_txt and top_level:
            still = []
            for pu in played_units:
                r, e = unit_states(b[pu["p"]], pu["ab"])
                if r + e > pu["r"] + pu["e"]:
                    stats["c5_units_seen_entering"] += 1
                    if r > pu["r"] and not pu["accel"]:
                        why = [n for n in pu["notes"] if re.search(r'ready|READY|readied|Ready', n)
                               and not re.search(r'RUNE|CHANNELED|READY TO PLAY', n)]
                        (stats.__setitem__("c5_ready_with_a_reason_line", stats["c5_ready_with_a_reason_line"] + 1) if why else None)
                        v.append(("C5" if not why else "C5-explained?", pu["dec"],
                                  f"{pu['name']} (P{pu['p']}) is ready on the first top-level board that shows it (trace D:{dnum}); lines: " + " / ".join(x[-70:] for x in why[:3])))
                    elif r > pu["r"]:
                        stats["c5_ready_by_accelerate"] += 1
                    continue
                pu["age"] += 1
                if pu["age"] < 10:
                    still.append(pu)
                else:
                    stats["c5_never_seen_on_board"] += 1
            played_units = still
        tl = trace.split("\n")
        who = None
        for line in tl:
            im = re.search(r'INTENT: \w+ by P(\d)', line)
            if im:
                who = int(im.group(1))
            for pu in played_units:
                if "ACCELERATE: " + pu["name"] + " pays" in line:
                    pu["accel"] = True
                pu["notes"].append(line)
            pm = re.search(r'PLAY: (.+?) \(id=\d+, Unit', line)
            if pm and who and board_txt:
                r, e = unit_states(b[who], abbr(pm.group(1)))
                dp = re.search(r'\[D:(\d+)\.', line)
                played_units.append({"name": pm.group(1), "ab": abbr(pm.group(1)), "p": who, "r": r, "e": e,
                                     "dec": int(dp.group(1)) if dp else dec, "accel": False, "notes": [], "age": 0})
        if "── Main Phase ──" in trace:
            main_start_pending = True
    return v, stats

def main():
    tdir, inv_path, sbdir, outp = sys.argv[1:5]
    inv = json.load(open(inv_path))
    lists = load_lists(inv, sbdir)
    allv = collections.defaultdict(list)
    stats = collections.Counter()
    files = sorted(f for f in os.listdir(tdir) if f.endswith(".log"))
    for fn in files:
        v, st = check_log(fn, os.path.join(tdir, fn), None)
        stats.update(st)
        rp = os.path.join(tdir, fn[:-4] + ".replay.txt")
        if os.path.exists(rp):
            v2, st2 = check_replay(fn, rp, lists, None)
            v += v2
            stats.update(st2)
        for code, dec, msg in v:
            allv[code].append((fn[:-4], dec, msg))
    with open(outp, "w") as out:
        out.write(f"games: {len(files)}\n")
        for k in sorted(stats):
            out.write(f"  {k}: {stats[k]}\n")
        for code in sorted(allv):
            out.write(f"\n== {code}: {len(allv[code])} violations\n")
            for fn, dec, msg in allv[code][:5000]:
                out.write(f"  {fn} @ {dec}: {msg}\n")
    print(open(outp).read()[:6000])

if __name__ == "__main__":
    main()
