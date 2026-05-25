#include "play_index_html.h"

#include <nlohmann/json.hpp>

#include <cstring>
#include <string>

namespace riftbound {

namespace {

// Template tokens substituted at startup. Single-occurrence in the
// raw-string body so we can use a one-shot find/replace without
// regex. Pick unlikely literal sequences that will never appear
// inside the actual CSS/HTML.
constexpr const char* kTokenCardImages = "/*__RIFTBOUND_CARD_IMAGES__*/";
constexpr const char* kTokenVersion    = "__RIFTBOUND_VERSION__";
constexpr const char* kTokenBuildDate  = "__RIFTBOUND_BUILD_DATE__";
constexpr const char* kTokenSeed       = "__RIFTBOUND_SEED__";

void replaceOnce(std::string& s, const std::string& needle, const std::string& replacement) {
    auto pos = s.find(needle);
    if (pos == std::string::npos) return;
    s.replace(pos, needle.size(), replacement);
}

constexpr const char* kHtmlTemplate = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8" />
<title>Riftbound — Play</title>
<style>
  * { box-sizing: border-box; }
  html, body { margin: 0; padding: 0; height: 100%; background: #0e0e10;
               color: #e3e3e8; font: 14px/1.4 system-ui, sans-serif;
               overflow: hidden; }
  body { display: flex; flex-direction: column; height: 100vh; }

  header {
    flex: 0 0 32px; display: flex; gap: 14px; align-items: center;
    padding: 0 12px; background: #16161a; border-bottom: 1px solid #232328;
    font-size: 12px; color: #9da0aa;
  }
  header .brand { color: #ffd35a; font-weight: 700; }
  header .sep   { color: #46464c; }
  header .conn  { margin-left: auto; }
  header .conn.bad { color: #f17171; }

  main {
    flex: 1 1 auto; display: flex; flex-direction: row; min-height: 0;
  }

  /* Column panels */
  #board-pane  { flex: 1 1 auto; min-width: 100px; padding: 12px; overflow: auto; }
  #side-pane   { flex: 0 0 360px; min-width: 200px;
                 display: flex; flex-direction: column; min-height: 0; }
  #image-pane  { flex: 0 0 360px; min-width: 120px; padding: 12px;
                 overflow: auto; background: #0a0a0c;
                 display: flex; flex-direction: column; }

  /* Side-pane sub-panels. trace-panel is the elastic one — it
     absorbs slack so the column always fills the side-pane,
     whether god-panel is shown or hidden. */
  #actions-panel { flex: 0 0 240px; min-height: 60px; padding: 12px; overflow: auto; }
  #god-panel     { flex: 0 0 200px; min-height: 60px; padding: 12px; overflow: auto;
                   border-top: 1px solid #232328; }
  #recent-panel  { flex: 0 0 160px; min-height: 60px; padding: 12px; overflow: auto;
                   border-top: 1px solid #232328; }
  #trace-panel   { flex: 1 1 auto;  min-height: 60px; padding: 12px; overflow: auto;
                   border-top: 1px solid #232328; }

  /* Resize handles */
  .resize-v { flex: 0 0 4px; cursor: col-resize; background: #232328;
              transition: background 80ms; }
  .resize-h { flex: 0 0 4px; cursor: row-resize; background: #232328;
              transition: background 80ms; }
  .resize-v:hover, .resize-h:hover,
  .resize-v.dragging, .resize-h.dragging { background: #3a5e90; }
  body.dragging-x, body.dragging-x * { cursor: col-resize !important; user-select: none; }
  body.dragging-y, body.dragging-y * { cursor: row-resize !important; user-select: none; }

  /* Content styles */
  #board { font: 12px/1.25 ui-monospace, SFMono-Regular, Menlo, monospace;
           white-space: pre; background: #16161a; padding: 10px;
           border-radius: 6px; }
  h2 { margin: 0 0 8px; font-size: 13px; text-transform: uppercase;
       letter-spacing: 0.05em; color: #9da0aa; font-weight: 600; }
  #status { font-size: 12px; color: #9da0aa; margin-bottom: 8px; }
  #status .status-line { display: flex; align-items: center; flex-wrap: wrap;
                          gap: 6px; font-size: 13px; }
  #status .player   { color: #ffd35a; font-weight: 700; }
  #status .score-p1 { color: #6fcf73; font-weight: 700;  /* friendly green for P1 */
                       font-size: 14px; }
  #status .score-p2 { color: #f17171; font-weight: 700;  /* opponent red for P2 */
                       font-size: 14px; }
  #status .sep      { color: #46464c; }
  #status .muted    { color: #6b6e7a; font-size: 11px; }
  #status .badge    { display: inline-block; padding: 1px 6px; border-radius: 3px;
                      background: #232328; margin-left: 6px; font-size: 11px; }
  .badge { display: inline-block; padding: 1px 6px; border-radius: 3px;
           background: #232328; margin-left: 6px; font-size: 11px;
           color: #9da0aa; }

  .actions { display: flex; flex-direction: column; gap: 4px; }
  .actions .prompt-header {
    padding: 6px 8px; margin-bottom: 4px;
    background: #1f2433; color: #b8def7; font-weight: 600;
    border-left: 3px solid #88c4ff; border-radius: 4px; font-size: 13px;
  }
  .actions button {
    text-align: left; padding: 6px 8px; background: #1c1c22;
    color: #e3e3e8; border: 1px solid #2a2a32; border-radius: 4px;
    font: inherit; cursor: pointer;
  }
  .actions button:hover  { background: #25252d; border-color: #3a3a48; }
  .actions button:active { background: #2a2a35; }
  .actions button:disabled {
    background: #15151a; color: #5e6170; border-color: #1f1f25;
    cursor: not-allowed; opacity: 0.6;
  }
  .actions .thinking {
    padding: 6px 8px; color: #b8def7; font-style: italic;
    background: #16161a; border: 1px dashed #2a3548; border-radius: 4px;
    margin-top: 4px;
  }
  .actions .idx { color: #6b6e7a; margin-right: 6px; }
  .actions .empty { color: #6b6e7a; font-style: italic; }

  .card-link {
    color: #88c4ff; cursor: pointer; border-bottom: 1px dotted #4a6a90;
  }
  .card-link:hover  { color: #b8def7; border-bottom-style: solid; }
  .card-link.active { color: #ffd35a; border-bottom-color: #ffd35a; }

  details { margin-bottom: 8px; }
  details summary {
    cursor: pointer; padding: 4px 0; font-size: 12px;
    text-transform: uppercase; letter-spacing: 0.05em; color: #9da0aa;
  }
  .zone-row {
    display: flex; align-items: center; gap: 6px; padding: 2px 0;
    border-bottom: 1px solid #1a1a20; font-size: 12px;
  }
  .zone-row .name { flex: 1; }
  .zone-row .meta { color: #6b6e7a; font-size: 11px; }
  .zone-row .move-btn {
    background: #232328; border: 1px solid #2a2a32; color: #e3e3e8;
    border-radius: 3px; padding: 1px 4px; font-size: 11px; cursor: pointer;
  }
  .field-row { display: flex; gap: 4px; padding: 2px 0; align-items: center;
               font-size: 12px; }
  .field-row label { width: 80px; color: #9da0aa; }
  .field-row input, .field-row select {
    background: #1c1c22; border: 1px solid #2a2a32; color: #e3e3e8;
    border-radius: 3px; padding: 2px 4px; font: inherit;
  }
  .field-row button {
    background: #2c4a78; border: 1px solid #3a5e90; color: #e3e3e8;
    border-radius: 3px; padding: 2px 8px; cursor: pointer; font: inherit;
  }
  .field-row button:hover { background: #355990; }

  #log { font: 11px/1.4 ui-monospace, monospace;
         overflow: auto; color: #6b6e7a; }
  #log .ok  { color: #6fcf73; }
  #log .err { color: #f17171; }
  /* Recent-actions feed — large, high-contrast. The whole point is to
     make opponent moves obvious without scanning the firehose. */
  #recent { font: 12px/1.3 system-ui, sans-serif;
            display: flex; flex-direction: column; gap: 3px; }
  #recent .ra {
    padding: 3px 7px; border-radius: 4px; background: #16161a;
    border-left: 4px solid #2a2a32;
  }
  #recent .ra-p1 { border-left-color: #6fcf73;
                    background: linear-gradient(to right, #1a2418 0%, #16161a 60%); }
  #recent .ra-p2 { border-left-color: #f17171;
                    background: linear-gradient(to right, #241a1a 0%, #16161a 60%); }
  #recent .ra.tr-score    { color: #ffd35a; font-weight: 700; }
  #recent .ra.tr-kill     { color: #f17171; font-weight: 600; }
  #recent .ra.tr-chose    { color: #e3e3e8; }
  #recent .ra:first-child { /* most recent — emphasise */
    background-color: #1f1f25;
    box-shadow: 0 0 0 1px #2a2a32 inset;
  }
  /* Semantic trace styling — surface what the opponent just did. */
  #log .tr          { color: #5e6170; padding: 0 2px 0 6px;
                      border-left: 2px solid transparent; }
  #log .tr.tr-p1    { border-left-color: #2d5e35; }         /* dim green = P1 attribution */
  #log .tr.tr-p2    { border-left-color: #5e2d2d; }         /* dim red   = P2 attribution */
  #log .tr-chose    { color: #ffd35a; font-weight: 600;     /* WHO chose WHAT — most important */
                      background: #2a230a; padding: 1px 4px;
                      border-left: 3px solid #ffd35a; margin: 2px 0; }
  #log .tr-score    { color: #6fcf73; font-weight: 600; }   /* score / conquer / hold / win */
  #log .tr-kill     { color: #f17171; font-weight: 600; }   /* kill / burn-out / deathknell */
  #log .tr-combat   { color: #ff8e3c; }                     /* combat steps */
  #log .tr-chain    { color: #88c4ff; }                     /* chain / spell / counter */
  #log .tr-effect   { color: #b8def7; }                     /* card effect text */
  #log .tr-phase    { color: #46464c; font-style: italic; } /* phase noise — dimmest */
  #log .tr-decision { color: #6b6e7a; }                     /* DECISION # marker */

  #image-pane .name { font-weight: 700; margin-bottom: 6px; min-height: 1.2em; }
  #image-pane img {
    max-width: 100%; object-fit: contain;
    border-radius: 6px; background: #16161a; display: none;
  }
  #image-placeholder { color: #6b6e7a; font-style: italic; font-size: 12px; }
  .game-over { background: #2a1a1a; border: 1px solid #4a2a2a; padding: 8px;
               border-radius: 4px; margin-bottom: 8px; color: #ffd35a; }
</style>
</head>
<body>
<header>
  <span class="brand">Riftbound — Play</span>
  <span class="sep">•</span>
  <span>Engine <strong id="version">__RIFTBOUND_VERSION__</strong> (built __RIFTBOUND_BUILD_DATE__)</span>
  <span class="sep">•</span>
  <span>Seed <strong id="seed">__RIFTBOUND_SEED__</strong></span>
  <span class="conn" id="conn">connecting…</span>
</header>

<main>
  <div id="board-pane">
    <div id="status">Waiting…</div>
    <pre id="board"></pre>
  </div>

  <div class="resize-v" data-axis="x" data-grow-with-positive="false"></div>

  <div id="side-pane">
    <div id="actions-panel">
      <h2>Legal Actions</h2>
      <div id="actions" class="actions"></div>
    </div>
    <div id="resize-actions-god" class="resize-h" data-axis="y" data-mode="trade"></div>
    <!--__GOD_BLOCK_START__-->
    <div id="god-panel">
      <h2>God Mode <span id="god-mode-badge" class="badge">disabled</span></h2>
      <div id="god"></div>
    </div>
    <div id="resize-god-recent" class="resize-h" data-axis="y" data-mode="trade"></div>
    <!--__GOD_BLOCK_END__-->
    <div id="recent-panel">
      <h2>Recent Actions</h2>
      <div id="recent"></div>
    </div>
    <div id="resize-recent-trace" class="resize-h" data-axis="y" data-mode="trade"></div>
    <div id="trace-panel">
      <h2>Trace</h2>
      <div id="log"></div>
    </div>
  </div>

  <div class="resize-v" data-axis="x" data-grow-with-positive="false"></div>

  <div id="image-pane">
    <h2>Card Details</h2>
    <div class="name" id="card-name"></div>
    <img id="card-image" alt="" />
    <div id="image-placeholder">Click any card name to view its image.</div>
  </div>
</main>

<script>
const CARD_IMAGES = /*__RIFTBOUND_CARD_IMAGES__*/;

(() => {
  const $ = (id) => document.getElementById(id);
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  const ws = new WebSocket(proto + '//' + location.host + '/ws');

  function log(msg, cls) {
    const div = document.createElement('div');
    if (cls) div.className = cls;
    div.textContent = msg;
    $('log').prepend(div);
    while ($('log').childElementCount > 500) $('log').lastChild.remove();
  }

  // Engine trace lines are a firehose. Pattern-match them into semantic
  // buckets so opponent moves + critical events surface visually. Style
  // applied alongside the level class.
  //   chose:   "CHOSE: ..."  → highlight what an agent just did.
  //   score:   SCORE / CONQUER / HOLD / WIN — points changed hands.
  //   kill:    KILL / BURN_OUT / DEATHKNELL — board change.
  //   combat:  COMBAT / DAMAGE / SHOWDOWN / ATTACK / DEFEND.
  //   chain:   CHAIN / SPELL / counter activity.
  //   effect:  EFFECT: ... — card-resolved effect text.
  //   phase:   PHASE / TURN — phase transitions.
  //   decision: top-of-decision marker (kept dimmer than CHOSE).
  function semanticTraceClass(msg) {
    if (msg.includes('CHOSE:'))      return 'tr-chose';
    if (msg.includes('SCORE') || msg.includes('CONQUER') || msg.includes('HOLD')
        || msg.includes('GAME_OVER') || msg.includes('WIN'))
                                      return 'tr-score';
    if (msg.includes('KILL') || msg.includes('BURN_OUT')
        || msg.includes('DEATHKNELL'))
                                      return 'tr-kill';
    if (msg.includes('COMBAT') || msg.includes('DAMAGE')
        || msg.includes('SHOWDOWN') || msg.includes('ATTACK')
        || msg.includes('DEFEND'))    return 'tr-combat';
    if (msg.includes('CHAIN') || msg.includes('SPELL_PLAYED')
        || msg.includes('COUNTER'))   return 'tr-chain';
    if (msg.includes('EFFECT:'))     return 'tr-effect';
    if (msg.includes('PHASE') || msg.includes('TURN_START')
        || msg.includes('TURN_END'))  return 'tr-phase';
    if (msg.includes('DECISION #'))   return 'tr-decision';
    return null;
  }

  // Carry-forward player attribution. The engine stamps CHOSE: /
  // DECISION # / PLAY: / phase lines with the acting player either
  // inline ("[P1]") or in a parenthesised qualifier ("(P1)"); we
  // remember the most recent attribution and prefix it onto subsequent
  // trace lines that don't carry their own tag. That way every line in
  // the log scans with a visible [Px] regardless of where it came from
  // in the engine. Best-effort: an event that's truly cross-player
  // (e.g. a cleanup pass) inherits the last decider, which is usually
  // the right intuition anyway since the cleanup is a consequence of
  // their action.
  let currentActor = null;

  function detectActorTag(msg) {
    // Inline tag: "[P1] ..." or "CHOSE: [P1] ..." (most common).
    let m = msg.match(/\[(P[12])\]/);
    if (m) return m[1];
    // Parenthesised tag: "DECISION #5 (P1): ...".
    m = msg.match(/\((P[12])\b[^)]*\)/);
    if (m) return m[1];
    // "P1 conceded", "P1 reached 8 points", "P2 wins", etc.
    m = msg.match(/\b(P[12])\b/);
    if (m) return m[1];
    return null;
  }

  function logTrace(level, msg) {
    const detected = detectActorTag(msg);
    if (detected) currentActor = detected;

    // Prefix the rendered text with [Px] if the message doesn't lead
    // with one already. Leaves the underlying msg untouched for the
    // semantic-class matcher below.
    let displayed = msg;
    if (currentActor && !msg.startsWith('[P1]') && !msg.startsWith('[P2]')) {
      displayed = '[' + currentActor + '] ' + msg;
    }

    const tag = semanticTraceClass(msg);
    const div = document.createElement('div');
    div.className = ['tr', tag,
                     currentActor ? 'tr-' + currentActor.toLowerCase() : null]
                    .filter(Boolean).join(' ');
    div.textContent = '[' + level + '] ' + displayed;
    $('log').prepend(div);
    while ($('log').childElementCount > 500) $('log').lastChild.remove();

    // Side-channel: pluck high-signal events (CHOSE lines + scoring +
    // kills + game-over) into a separate "Recent actions" feed at the
    // top of the log panel. This is the readability fix — full trace
    // is overwhelming, but the recent-actions feed shows ONLY what
    // each agent actually decided to do, at a larger, calmer pace.
    if (tag === 'tr-chose' || tag === 'tr-score' || tag === 'tr-kill'
        || msg.startsWith('GAME_OVER:')) {
      addRecentAction(msg, tag);
    }
  }

  // ── Recent actions feed ────────────────────────────────────────────────
  // Keeps the most recent ~12 high-signal events visible. CHOSE: lines
  // are formatted to highlight WHO chose WHAT (the engine's "[P1] " /
  // "[P2] " tag at the start of each CHOSE message drives the colour).
  function addRecentAction(msg, tag) {
    // Pull the player tag (`[P1]` or `[P2]`) out of the message so we
    // can colour-code the row regardless of where it appears.
    let player = null;
    const m = msg.match(/\[(P[12])\]/);
    if (m) player = m[1];

    const row = document.createElement('div');
    row.className = 'ra ' + (tag || '');
    if (player) row.classList.add('ra-' + player.toLowerCase());

    // Strip the level-irrelevant prefixes for cleaner display. The
    // user already sees the tag colour; the literal "CHOSE:" word is
    // visual noise once they're trained on the pattern.
    let display = msg
      .replace(/^CHOSE:\s*/, '')
      .replace(/^SCORE:\s*/, '+1 ')
      .replace(/^KILL:\s*/, 'killed ')
      .replace(/^CONQUER:\s*/, 'conquered ');
    row.textContent = display;

    const feed = $('recent');
    feed.prepend(row);
    while (feed.childElementCount > 12) feed.lastChild.remove();
  }

  ws.onopen  = () => { $('conn').textContent = 'connected'; $('conn').classList.remove('bad');
                       ws.send(JSON.stringify({type:'request_state'})); };
  ws.onclose = () => { $('conn').textContent = 'disconnected'; $('conn').classList.add('bad'); };
  ws.onerror = () => log('WebSocket error', 'err');

  ws.onmessage = (ev) => {
    let m; try { m = JSON.parse(ev.data); } catch { log('Bad JSON from server', 'err'); return; }
    if (m.type === 'decision' || m.type === 'state') { render(m); }
    else if (m.type === 'edit_ok')  { log('edit ok', 'ok'); }
    else if (m.type === 'edit_err') { log('edit err: ' + m.msg, 'err'); }
    else if (m.type === 'err')      { log('server: ' + m.msg, 'err'); }
    else if (m.type === 'log')      { logTrace(m.level, m.msg); }
  };

  function levelClass(level) {
    if (level === 'WRN') return 'err';
    if (level === 'INF' || level === 'DBG') return 'ok';
    return null;  // TRC: plain (gray)
  }

  function send(obj) { ws.send(JSON.stringify(obj)); }

  function render(m) {
    updateLiveNames(m);
    const t = m.turn || {};
    // Score line: always shown so the user can see P1/P2 progress
    // toward the victory score at a glance, regardless of whether
    // god-mode (the per-zone score field) is enabled.
    const phaseLabel = t.phase_name || ('phase ' + t.phase);
    const turnLabel  = 'Turn ' + (t.turn_number ?? '?');
    const scoreLine =
      `<span class="score-p1">P1: ${t.p1_score ?? 0}</span>` +
      ` <span class="sep">|</span> ` +
      `<span class="score-p2">P2: ${t.p2_score ?? 0}</span>` +
      (t.victory_score ? ` <span class="muted">(to ${t.victory_score})</span>` : '');

    let status = '';
    if (m.game_over) {
      status =
        `<div class="status-line">${turnLabel} — ${phaseLabel} — ${scoreLine}</div>` +
        `<div class="game-over">Game Over — winner: ${m.winner || 'none'} (${m.game_over_reason || ''})</div>`;
    } else if (m.type === 'decision') {
      status =
        `<div class="status-line">${turnLabel} — ${phaseLabel} — ${scoreLine}` +
        ` <span class="sep">|</span> <span class="player">${m.player}</span> to act` +
        ` <span class="badge">decision #${m.decision_id}</span></div>`;
    } else {
      status =
        `<div class="status-line">${turnLabel} — ${phaseLabel} — ${scoreLine}` +
        ` <span class="sep">|</span> <span class="player">${t.turn_player ?? '?'}</span>` +
        ` <span class="badge">engine busy</span></div>`;
    }
    $('status').innerHTML = status;

    $('board').textContent = m.render_text || '';

    const aDiv = $('actions');
    aDiv.innerHTML = '';
    // Prompt header for MakeChoice / multi-choice decisions. The server
    // attaches the latest *_PROMPT trace line as `m.prompt`; if present,
    // surface it above the legal-action buttons so the user knows what
    // they're choosing (e.g. "Discard a card", "Choose a target").
    if (m.prompt) {
      const ph = document.createElement('div');
      ph.className = 'prompt-header';
      ph.textContent = m.prompt;
      aDiv.appendChild(ph);
    }
    const legal = m.legal || [];
    if (legal.length === 0) {
      const e = document.createElement('div');
      if (m.game_over) {
        e.className = 'empty';
        e.textContent = 'Game ended.';
      } else {
        // No human is at a decision → an AI agent is selecting or the
        // engine is processing internally. Surface this clearly so the
        // user doesn't sit confused wondering why nothing is clickable.
        e.className = 'thinking';
        e.textContent = 'Engine is thinking — please wait…';
      }
      aDiv.appendChild(e);
    } else {
      legal.forEach(act => {
        const b = document.createElement('button');
        const idx = document.createElement('span');
        idx.className = 'idx'; idx.textContent = act.index + '.';
        b.appendChild(idx);
        b.appendChild(document.createTextNode(' ' + act.label));
        b.onclick = () => {
          // Disable every button immediately so the user can't double-click
          // or rage-click while the engine processes. The next frame from
          // the server will rebuild the panel — either with new buttons
          // (next human decision) or a "Waiting for engine…" message
          // (AI's turn or internal engine processing).
          const buttons = aDiv.querySelectorAll('button');
          buttons.forEach(btn => { btn.disabled = true; });
          const note = document.createElement('div');
          note.className = 'thinking';
          note.textContent = 'Sent — engine is processing…';
          aDiv.appendChild(note);
          send({type:'choose', player: m.player, index: act.index});
        };
        aDiv.appendChild(b);
      });
    }

    // God-mode markup is stripped server-side when --god-mode=off,
    // so the elements may not exist. Only update them when present.
    const godPanel = $('god-panel');
    if (godPanel) {
      $('god-mode-badge').textContent = m.god_mode ? 'enabled' : 'disabled';
      if (m.god_mode) renderGod(m);
    }

    // Wrap card names AFTER DOM updates so the walker sees the new text.
    wrapCardNames($('board'));
    wrapCardNames($('actions'));
    if ($('god')) wrapCardNames($('god'));
  }

  function renderGod(m) {
    const root = $('god');
    root.innerHTML = '';
    const battlefields = m.battlefields || [];

    for (const pid of ['P1', 'P2']) {
      const zones = m.zones[pid];
      const det = document.createElement('details');
      det.open = (pid === m.player);
      const sum = document.createElement('summary');
      sum.textContent = `${pid} — score ${zones.score} • energy ${zones.energy} • power ${zones.power_total}`;
      det.appendChild(sum);

      det.appendChild(playerFieldRow(pid, 'score',  zones.score));
      det.appendChild(playerFieldRow(pid, 'energy', zones.energy));

      // Off-board zones + own base.
      for (const z of ['hand','main_deck','trash','banishment','rune_deck','base']) {
        det.appendChild(zoneBlock(pid, z, zones[z] || [], battlefields));
      }
      root.appendChild(det);
    }

    // On-board battlefields are shared and shown once below the per-player
    // sections. Each unit row carries the controller it currently belongs
    // to so move ops know which player to attribute it to.
    if (battlefields.length) {
      const bfDet = document.createElement('details');
      bfDet.open = true;
      const sum = document.createElement('summary');
      sum.textContent = `Battlefields (${battlefields.length})`;
      bfDet.appendChild(sum);
      for (const bf of battlefields) {
        bfDet.appendChild(battlefieldBlock(bf, battlefields));
      }
      root.appendChild(bfDet);
    }
  }

  function playerFieldRow(pid, field, current) {
    const r = document.createElement('div');
    r.className = 'field-row';
    r.innerHTML = `<label>${pid} ${field}</label>
      <input type="number" min="0" value="${current}" style="width:64px" />
      <button>Set</button>`;
    const [inp, btn] = [r.querySelector('input'), r.querySelector('button')];
    btn.onclick = () => send({type:'edit_player', player: pid, field, value: Number(inp.value)});
    return r;
  }

  // ── Destination dropdown options ────────────────────────────────────────
  // The dropdown lists every destination the StateEditor can route a card
  // to. Off-board zones are static; "Base" puts the card at the controller's
  // base location; "BF <id>" puts the card at that specific battlefield
  // (the dropdown value is "BF:<id>" so the click handler can parse it).
  function buildDestOptions(battlefields) {
    const out = [];
    ['Hand','MainDeck','Trash','Banishment','RuneDeck','ChampionZone','LegendZone','Base']
        .forEach(z => out.push({value: z, label: z}));
    // Battlefield options now show the disambiguated card name from
    // the server (e.g. "Vilemaw's Lair (P1's)") so mirror-match BFs
    // with identical card names don't all read as "BF 0 / BF 1 / BF 2".
    (battlefields || []).forEach(bf => {
      const name = bf.name || ('BF ' + bf.id);
      const ctrl = bf.controller && bf.controller !== 'none'
                     ? ' — ctrl: ' + bf.controller : '';
      out.push({value: 'BF:' + bf.id, label: name + ctrl});
    });
    return out;
  }

  function sendMove(objectId, toPlayer, destValue) {
    if (destValue.startsWith('BF:')) {
      const bfId = parseInt(destValue.substring(3), 10);
      send({type:'edit_move', object_id: objectId, to_player: toPlayer,
            to_zone: 'BattlefieldZone', battlefield_id: bfId, to_top: true});
    } else {
      send({type:'edit_move', object_id: objectId, to_player: toPlayer,
            to_zone: destValue, to_top: true});
    }
  }

  function zoneRowEl(c, toPlayer, battlefields) {
    const row = document.createElement('div');
    row.className = 'zone-row';
    const nameSpan = document.createElement('span');
    nameSpan.className = 'name';
    nameSpan.textContent = c.name;
    const metaSpan = document.createElement('span');
    metaSpan.className = 'meta';
    metaSpan.textContent = `#${c.id}${c.exhausted ? ' • exh' : ''}${c.damage ? ' • dmg ' + c.damage : ''}`;
    const sel = document.createElement('select');
    buildDestOptions(battlefields).forEach(opt => {
      const o = document.createElement('option');
      o.value = opt.value; o.textContent = opt.label;
      sel.appendChild(o);
    });
    const btn = document.createElement('button');
    btn.textContent = '→';
    btn.className = 'move-btn';
    btn.onclick = () => sendMove(c.id, toPlayer, sel.value);
    row.appendChild(nameSpan);
    row.appendChild(metaSpan);
    row.appendChild(sel);
    row.appendChild(btn);
    return row;
  }

  function zoneBlock(pid, zone, cards, battlefields) {
    const wrap = document.createElement('details');
    const sum = document.createElement('summary');
    sum.textContent = `${zone} (${cards.length})`;
    wrap.appendChild(sum);
    cards.forEach(c => wrap.appendChild(zoneRowEl(c, pid, battlefields)));
    return wrap;
  }

  function battlefieldBlock(bf, battlefields) {
    const wrap = document.createElement('details');
    wrap.open = bf.units.length > 0;
    const sum = document.createElement('summary');
    // Disambiguated name from server (e.g. "Vilemaw's Lair (P1's)").
    const bfName = bf.name || ('BF ' + bf.id);
    sum.textContent = `${bfName} — controller: ${bf.controller} — ${bf.units.length} unit(s)`;
    wrap.appendChild(sum);
    bf.units.forEach(c => {
      // Units at a BF: controller comes from the BF itself, which is the
      // player the move will be attributed to in case the dest is off-board.
      const toPlayer = (bf.controller === 'P1' || bf.controller === 'P2')
          ? bf.controller : 'P1';
      wrap.appendChild(zoneRowEl(c, toPlayer, battlefields));
    });
    return wrap;
  }

  // ── Card-name → image-panel wiring ──────────────────────────────────────
  // The board ASCII (StateRenderer::renderObject) truncates names that
  // are too wide for fixed-width columns:
  //   • Top-level unit/gear: if size > 16, either
  //       "<prefix-up-to-comma>,<4-chars-after-comma>"  e.g. "LeBlanc,Frag"
  //     when the comma is before position 12, or
  //       "<first-14-chars>.."  e.g. "Miss Fortune, .."
  //   • Equipped gear inline: "<first-6-chars>.."  e.g. "Cataly.."
  // The plain CARD_IMAGES lookup misses all those forms. Below we
  // pre-compute every truncated form a name can produce, mapping each
  // back to its FULL name; the wrapper then matches against the union
  // and emits a card-link whose `data-card` carries the FULL name (so
  // hover-load resolves the right image).
  function computeTruncatedForms(name) {
    const forms = [];
    if (name.length > 16) {
      const comma = name.indexOf(',');
      if (comma !== -1 && comma < 12) {
        forms.push(name.substring(0, comma) + ',' +
                   name.substring(comma + 2, comma + 6));
      } else {
        forms.push(name.substring(0, 14) + '..');
      }
    }
    if (name.length > 8) {
      forms.push(name.substring(0, 6) + '..');
    }
    return forms;
  }

  // Two-level lookup so we can disambiguate truncated collisions using
  // the live game state:
  //   • FULL_NAMES — set of all canonical card names (registry-wide).
  //   • ALL_FORMS[trunc] — array of full names that truncate to `trunc`.
  // A truncated form like "Miss Fortune, .." has TWO candidates
  // ("Miss Fortune, Captain" and "Miss Fortune, Buccaneer"). At render
  // time we know which one is actually in play, so resolveName() picks
  // the live one. Previously the resolver was a single map with
  // first-write-wins, which silently pointed every "Miss Fortune, .."
  // click at Buccaneer regardless of the live state.
  const FULL_NAMES = new Set();
  const ALL_FORMS = {};
  for (const full of Object.keys(CARD_IMAGES)) {
    FULL_NAMES.add(full);
    for (const trunc of computeTruncatedForms(full)) {
      if (!ALL_FORMS[trunc]) ALL_FORMS[trunc] = [];
      ALL_FORMS[trunc].push(full);
    }
  }
  // Names currently observed in zones / battlefields — rebuilt on
  // every render(m). Used as the tiebreaker for ambiguous truncated
  // forms.
  const LIVE_NAMES = new Set();

  function resolveName(token) {
    if (FULL_NAMES.has(token)) return token;
    const candidates = ALL_FORMS[token];
    if (!candidates || candidates.length === 0) return null;
    for (const c of candidates) {
      if (LIVE_NAMES.has(c)) return c;
    }
    return candidates[0];  // best-effort fallback
  }

  function updateLiveNames(m) {
    LIVE_NAMES.clear();
    if (m && m.zones) {
      for (const pid of ['P1', 'P2']) {
        const z = m.zones[pid];
        if (!z) continue;
        for (const zone of ['hand','main_deck','trash','banishment','rune_deck','base']) {
          for (const c of (z[zone] || [])) {
            if (c && c.name) LIVE_NAMES.add(c.name);
          }
        }
      }
    }
    if (m && m.battlefields) {
      for (const bf of m.battlefields) {
        if (bf && bf.name) LIVE_NAMES.add(bf.name);
        for (const u of (bf.units || [])) {
          if (u && u.name) LIVE_NAMES.add(u.name);
        }
      }
    }
  }

  // Sort longest-first so "Miss Fortune, Captain" wins over "Miss Fortune"
  // AND truncated forms ("Miss Fortune, ..") win over their shorter
  // equivalents.
  const NAMES_SORTED = [...new Set([...FULL_NAMES, ...Object.keys(ALL_FORMS)])]
      .sort((a, b) => b.length - a.length);
  function escapeRegex(s) { return s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'); }
  const CARD_NAME_RE = NAMES_SORTED.length
      ? new RegExp('(' + NAMES_SORTED.map(escapeRegex).join('|') + ')', 'g')
      : null;

  function wrapCardNames(root) {
    if (!CARD_NAME_RE || !root) return;
    const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, null);
    const nodes = [];
    while (walker.nextNode()) nodes.push(walker.currentNode);
    for (const node of nodes) {
      const text = node.nodeValue;
      if (!CARD_NAME_RE.test(text)) continue;
      CARD_NAME_RE.lastIndex = 0;
      const parts = text.split(CARD_NAME_RE);
      if (parts.length <= 1) continue;
      const frag = document.createDocumentFragment();
      for (const part of parts) {
        const full = resolveName(part);
        if (full) {
          const span = document.createElement('span');
          span.className = 'card-link';
          span.dataset.card = full;       // full name for image lookup
          span.textContent = part;        // visible text stays truncated
          frag.appendChild(span);
        } else {
          frag.appendChild(document.createTextNode(part));
        }
      }
      node.parentNode.replaceChild(frag, node);
    }
  }

  // Hover-to-show, not click-to-show. Card-link spans sit INSIDE
  // legal-action buttons (e.g. "Play Miss Fortune"), so a click on the
  // name would also fire the button's onclick and send the action —
  // not what the user wants when they're just exploring card images.
  // Hover sidesteps the issue entirely; clicks pass straight through
  // to the surrounding button.
  let activeLink = null;
  document.addEventListener('mouseover', (e) => {
    const link = e.target.closest && e.target.closest('.card-link');
    if (!link || link === activeLink) return;
    const name = link.dataset.card;
    const url = CARD_IMAGES[name];
    if (!url) return;
    const img  = $('card-image');
    const lbl  = $('card-name');
    const ph   = $('image-placeholder');
    img.src = url;
    img.alt = name;
    img.style.display = 'block';
    lbl.textContent = name;
    if (ph) ph.style.display = 'none';
    if (activeLink && activeLink !== link) activeLink.classList.remove('active');
    link.classList.add('active');
    activeLink = link;
  });

  // ── Resize handles ──────────────────────────────────────────────────────
  // Two flavors of drag, picked per-handle by data-mode:
  //
  //  • default (outer board↔side, side↔image): set ONLY the next-sibling's
  //    flex-basis from the cursor's distance to that panel's far edge.
  //    Relies on board-pane being `flex: 1 1 auto` to absorb slack.
  //
  //  • "trade" (inner actions↔god, god↔log): set BOTH adjacent panels
  //    from a delta. This is the right behavior whenever the elastic
  //    panel is between two handles — otherwise dragging the upper
  //    handle would set the elastic panel's basis explicitly,
  //    breaking the "god-panel absorbs slack" relationship and
  //    making the lower handle resize the wrong thing.
  (function setupResize() {
    let drag = null;
    const minSize = 60;

    document.addEventListener('pointerdown', (e) => {
      const handle = e.target.closest('.resize-v, .resize-h');
      if (!handle) return;
      const isHorz = handle.dataset.axis === 'y';
      const mode = handle.dataset.mode || 'next';
      if (mode === 'trade') {
        const before = handle.previousElementSibling;
        const after  = handle.nextElementSibling;
        if (!before || !after) return;
        const beforeRect = before.getBoundingClientRect();
        const afterRect  = after.getBoundingClientRect();
        drag = {
          handle, mode, isHorz, before, after,
          startBefore: isHorz ? beforeRect.height : beforeRect.width,
          startAfter:  isHorz ? afterRect.height  : afterRect.width,
          startCursor: isHorz ? e.clientY : e.clientX,
        };
      } else {
        // Default: size next-sibling from the cursor relative to its
        // stable far edge. Works because board-pane is elastic and
        // absorbs the difference.
        const target = handle.nextElementSibling;
        if (!target) return;
        const targetRect = target.getBoundingClientRect();
        drag = {
          handle, mode, isHorz, target,
          farEdge: isHorz ? targetRect.bottom : targetRect.right,
        };
      }
      handle.classList.add('dragging');
      handle.setPointerCapture && handle.setPointerCapture(e.pointerId);
      document.body.classList.add(isHorz ? 'dragging-y' : 'dragging-x');
      e.preventDefault();
    });

    document.addEventListener('pointermove', (e) => {
      if (!drag) return;
      const cursor = drag.isHorz ? e.clientY : e.clientX;
      if (drag.mode === 'trade') {
        const delta = cursor - drag.startCursor;
        let nb = drag.startBefore + delta;
        let na = drag.startAfter  - delta;
        // Honor min sizes on both sides without losing pixels: when
        // one side hits the floor, the other absorbs the surplus so
        // the boundary stops moving but the total height/width of the
        // pair stays put.
        if (nb < minSize) { na -= (minSize - nb); nb = minSize; }
        if (na < minSize) { nb -= (minSize - na); na = minSize; }
        if (nb < minSize) nb = minSize;
        if (na < minSize) na = minSize;
        drag.before.style.flex = '0 0 ' + nb + 'px';
        drag.after.style.flex  = '0 0 ' + na + 'px';
      } else {
        const newSize = Math.max(minSize, drag.farEdge - cursor);
        drag.target.style.flex = '0 0 ' + newSize + 'px';
      }
    });

    document.addEventListener('pointerup', () => {
      if (!drag) return;
      drag.handle.classList.remove('dragging');
      document.body.classList.remove('dragging-x', 'dragging-y');
      drag = null;
    });
  })();
})();
</script>
</body>
</html>
)HTML";

} // namespace

std::string buildPlayIndexHtml(
    const std::map<std::string, std::string>& card_images,
    std::string_view version_tag,
    std::string_view build_date,
    uint64_t seed,
    bool god_mode_enabled) {
    std::string html = kHtmlTemplate;

    // Strip the god-panel block (markup + handles) entirely when
    // god-mode is disabled, so we don't ship DOM the user can't use.
    constexpr const char* kGodStart = "<!--__GOD_BLOCK_START__-->";
    constexpr const char* kGodEnd   = "<!--__GOD_BLOCK_END__-->";
    auto start = html.find(kGodStart);
    auto end   = html.find(kGodEnd);
    if (start != std::string::npos && end != std::string::npos && end > start) {
        if (god_mode_enabled) {
            html.erase(end,   std::strlen(kGodEnd));
            html.erase(start, std::strlen(kGodStart));
        } else {
            html.erase(start, end + std::strlen(kGodEnd) - start);
        }
    }

    nlohmann::json m = nlohmann::json::object();
    for (const auto& [name, url] : card_images) m[name] = url;

    replaceOnce(html, kTokenCardImages, m.empty() ? std::string("{}") : m.dump());
    replaceOnce(html, kTokenVersion,    std::string(version_tag));
    replaceOnce(html, kTokenBuildDate,  std::string(build_date));
    replaceOnce(html, kTokenSeed,       std::to_string(seed));
    return html;
}

} // namespace riftbound
