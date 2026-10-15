#!/usr/bin/env python3
"""List card files whose choice-making calls sit in a callback that does not
opt into a finalize pass. Heuristic parser: per file, every function body
(brace-matched), the asking calls in it, and which callbacks reach it."""
import os, re, sys, json

ROOT = sys.argv[1]
ASK = ['pickTargetPair', 'pickTarget', 'confirmOptional', 'pickMode', 'selectObjectAs',
       'selectObject', 'selectSeveralTargets', 'selectSeveral', 'pickXAmount', 'pickChainItem',
       'selectTargetOrNone', 'splitDamageAmong', 'pickEffectPlayLocation',
       'playTokensChoosingLocations', 'pickTokenDest', 'pickMoveDest', 'discardOneThenAct',
       'discardThenAct', 'requestChoice', 'chooseObjectNow', 'confirmNow', 'chooseValueNow',
       'chooseAmongNow', 'chooseLocationNow']
ASK_RE = re.compile(r'\b(' + '|'.join(ASK) + r')\s*\(')
CB = {'onResolve': 2, 'onTrigger': 1, 'onEquippedTrigger': 1, 'onActivate': 4}
OTHER_CB = ['onPlay', 'applyReplacement', 'payEquipExtraCost', 'onEquipped', 'payNonResourceCost',
            'onUnattached', 'replaceHostileEffect', 'asYouPlayChoices', 'seenOnTopOfDeck',
            'payAdditionalPlayCost', 'onReflexiveTrigger', 'payEquipAndAttach']

def strip_comments(s):
    s = re.sub(r'//[^\n]*', '', s)
    s = re.sub(r'/\*.*?\*/', '', s, flags=re.S)
    s = re.sub(r'R"RB\(.*?\)RB"', '""', s, flags=re.S)
    s = re.sub(r'"(?:\\.|[^"\\])*"', '""', s)
    return s

FUNC_HDR = re.compile(r'\b([A-Za-z_]\w*)\s*\(([^;{}()]|\([^;{}()]*\))*\)\s*(const\s*)?(override\s*)?(final\s*)?(noexcept\s*)?(->\s*[\w:<>]+\s*)?\{')

def bodies(s):
    out = []  # (name, start, end)
    for m in FUNC_HDR.finditer(s):
        name = m.group(1)
        if name in ('if', 'for', 'while', 'switch', 'catch', 'return', 'sizeof'):
            continue
        i = m.end() - 1
        depth = 0
        j = i
        while j < len(s):
            if s[j] == '{': depth += 1
            elif s[j] == '}':
                depth -= 1
                if depth == 0: break
            j += 1
        out.append((name, i, j))
    return out

def analyse(path):
    s = strip_comments(open(path).read())
    bs = bodies(s)
    # keep outermost function per name occurrence (a lambda body is not matched by FUNC_HDR)
    # attribute each ask call to the innermost enclosing known function
    funcs = {}
    for (n, a, b) in bs:
        funcs.setdefault(n, []).append((a, b))
    def innermost(pos):
        best = None
        for (n, a, b) in bs:
            if a <= pos <= b and (best is None or a > best[1]):
                best = (n, a, b)
        return best
    asks = {}  # func -> list of asks
    calls = {}  # func -> set of called names
    for m in ASK_RE.finditer(s):
        f = innermost(m.start())
        fn = f[0] if f else '?'
        asks.setdefault(fn, []).append(m.group(1))
    names = set(funcs)
    for (n, a, b) in bs:
        body = s[a:b]
        for c in re.finditer(r'\b([A-Za-z_]\w*)\s*\(', body):
            if c.group(1) in names and c.group(1) != n:
                calls.setdefault(n, set()).add(c.group(1))
    # reach: callbacks -> asks
    def reach(fn, seen):
        if fn in seen: return []
        seen.add(fn)
        r = [(fn, x) for x in asks.get(fn, [])]
        for c in calls.get(fn, ()):
            r += reach(c, seen)
        return r
    bits = 0
    m = re.search(r'choicesAtFinalization\s*\(\s*\)\s*const\s*override\s*\{([^}]*)\}', s)
    if m:
        for k, v in (('kFinalizeTrigger', 1), ('kFinalizeResolve', 2), ('kFinalizeActivate', 4)):
            if k in m.group(1): bits |= v
    ptt = bool(re.search(r'needsPlayTimeTarget(Pair)?\s*\(\s*\)\s*const\s*override\s*\{\s*return\s+true', s))
    att = 'needs_activation_time_target' in s and re.search(r'needs_activation_time_target\s*=\s*true', s)
    res = []
    for cb, bit in CB.items():
        if cb not in names: continue
        r = reach(cb, set())
        if r and not (bits & bit):
            res.append((cb, sorted(set(x for _, x in r)), sorted(set(f for f, _ in r))))
    other = []
    for cb in OTHER_CB:
        if cb in names:
            r = reach(cb, set())
            if r: other.append((cb, sorted(set(x for _, x in r))))
    unattributed = asks.get('?', [])
    return dict(bits=bits, ptt=ptt, att=bool(att), missing=res, other=other,
                unattributed=unattributed, allasks=sorted(set(a for v in asks.values() for a in v)))

results = {}
for d, _, fs in os.walk(os.path.join(ROOT, 'src/cards')):
    for f in fs:
        if not (f.endswith('.cpp') or f.endswith('.h')): continue
        if f in ('card.cpp', 'card.h', 'card_helpers.h', 'card_registry.cpp', 'card_registry.h', 'cards_init.cpp'):
            continue
        p = os.path.join(d, f)
        r = analyse(p)
        if r['allasks']:
            results[os.path.relpath(p, ROOT)] = r
json.dump(results, open(sys.argv[2], 'w'), indent=1)
missing = {k: v for k, v in results.items() if v['missing'] or v['unattributed']}
print('files with asks:', len(results))
print('files with an ask in a non-opted-in callback:', len(missing))
for k in sorted(missing):
    v = missing[k]
    print(k, 'bits=%d' % v['bits'], 'ptt' if v['ptt'] else '', 'att' if v['att'] else '',
          ';'.join('%s:%s' % (cb, ','.join(a)) for cb, a, _ in v['missing']),
          ('UNATTR:' + ','.join(v['unattributed'])) if v['unattributed'] else '')
