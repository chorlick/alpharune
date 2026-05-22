#include "replay_writer.h"

#include "core/version.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace riftbound {

std::map<std::string, std::string> ReplayWriter::loadCardImageMap() {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates = {
        fs::path("cards/raw/gallery_raw.json"),
    };
    if (const char* root = std::getenv("RIFTBOUND_ROOT")) {
        candidates.push_back(fs::path(root) / "cards" / "raw" / "gallery_raw.json");
    }
    // Walk up from CWD as a fallback (devs often run from build/).
    for (fs::path p = fs::current_path(); !p.empty() && p != p.root_path(); p = p.parent_path()) {
        candidates.push_back(p / "cards" / "raw" / "gallery_raw.json");
    }

    std::map<std::string, std::string> out;
    for (const auto& path : candidates) {
        std::ifstream in(path);
        if (!in) continue;
        nlohmann::json arr;
        try { in >> arr; } catch (...) { continue; }
        if (!arr.is_array()) continue;
        for (const auto& card : arr) {
            if (!card.contains("name") || !card.contains("cardImage")) continue;
            const auto& img = card["cardImage"];
            if (!img.contains("url")) continue;
            std::string name = card["name"].get<std::string>();
            std::string url  = img["url"].get<std::string>();
            // First-write-wins so duplicate names across sets keep the
            // earliest entry. The image is the same illustration anyway
            // for shared names.
            out.emplace(std::move(name), std::move(url));
        }
        if (!out.empty()) return out;
    }
    return out;  // empty if no gallery file found
}

ReplayWriter::ReplayWriter(const CardDB& db, const std::string& output_path)
    : db_(db), output_path_(output_path) {}

void ReplayWriter::setGameHeader(const std::string& header) {
    game_header_ = header;
}

void ReplayWriter::setNextWinProb(double mean_value, int sims, int deciding_player) {
    pending_mcts_value_  = mean_value;
    pending_mcts_sims_   = sims;
    pending_mcts_player_ = deciding_player;
}

void ReplayWriter::addTraceLine(const std::string& line) {
    pending_trace_.push_back(line);
}

void ReplayWriter::recordPhaseSeam(const GameState& state,
                                    TurnPhase old_phase,
                                    TurnPhase new_phase,
                                    const StateRenderer& renderer) {
    // Skip until the first decision has been recorded — pre-game phase
    // transitions (Setup → AwakenPhase during engine startup) happen
    // before any user-visible decision and would create orphan seams.
    if (snapshots_.empty()) return;
    // Ignore no-op transitions (engine re-emits the current phase in
    // some paths — e.g. resetting AwakenPhase mid-startup).
    if (old_phase == new_phase) return;
    // Suppress back-to-back duplicate seams: if the most recent
    // snapshot is already a seam for this same phase, no new chapter
    // marker is needed. We STILL flush pending_trace_ to it — otherwise
    // phase-header trace lines (logged inside awakenPhase / beginningStep
    // / etc. just before they emit their PhaseChangedEvent, which the
    // duplicate-emit path re-fires) would leak forward onto the NEXT
    // seam, mis-labeling them.
    if (snapshots_.back().is_seam &&
        snapshots_.back().seam_phase == toString(new_phase)) {
        if (!pending_trace_.empty()) {
            auto& prev = snapshots_.back();
            prev.trace_lines.insert(
                prev.trace_lines.end(),
                std::make_move_iterator(pending_trace_.begin()),
                std::make_move_iterator(pending_trace_.end()));
            pending_trace_.clear();
        }
        return;
    }

    // Flush whatever trace has accumulated since the last snapshot to
    // the most recent snapshot — those events were caused by the decision
    // (or seam) that came before this transition. Each phase's trace
    // thus localizes to the snapshot that owns that phase.
    if (!pending_trace_.empty()) {
        auto& prev = snapshots_.back();
        prev.trace_lines.insert(
            prev.trace_lines.end(),
            std::make_move_iterator(pending_trace_.begin()),
            std::make_move_iterator(pending_trace_.end()));
        pending_trace_.clear();
    }

    Snapshot snap;
    snap.is_seam = true;
    snap.seam_phase = toString(new_phase);
    snap.decision_index = state.decision_index;

    std::ostringstream ti;
    ti << "Turn " << state.turn.turn_number
       << " | ▶ " << snap.seam_phase << " begins"
       << " | " << toString(state.turn.turn_player) << "'s turn"
       << " | Score: P1=" << state.players[0].score
       << " P2=" << state.players[1].score;
    snap.turn_info = ti.str();

    if (new_phase != TurnPhase::Mulligan) {
        snap.board_ascii = renderer.render(state);
    }

    std::ostringstream dec;
    dec << "  ── Phase transition ──\n";
    dec << "  " << toString(old_phase) << "  →  " << snap.seam_phase << "\n";
    snap.decision_text = dec.str();

    snapshots_.push_back(std::move(snap));
}

void ReplayWriter::recordDecision(const GameState& state,
                                   const std::vector<Intent>& legal_actions,
                                   const Intent& chosen_action,
                                   const StateRenderer& renderer) {
    // Forward-shift the trace: events accumulated since the previous
    // recordDecision were caused by applying the previous decision's
    // chosen_action (plus any intervening system steps). Attach them to
    // the PREVIOUS snapshot so each decision's trace shows ITS
    // consequences, not the prior decision's. For the very first call
    // there's no previous snapshot — pre-game/mulligan-setup trace
    // attaches to snapshot 0 below.
    if (!snapshots_.empty() && !pending_trace_.empty()) {
        auto& prev = snapshots_.back();
        prev.trace_lines.insert(
            prev.trace_lines.end(),
            std::make_move_iterator(pending_trace_.begin()),
            std::make_move_iterator(pending_trace_.end()));
        pending_trace_.clear();
    }

    Snapshot snap;
    snap.decision_index = state.decision_index;

    // Turn info
    std::ostringstream ti;
    ti << "Turn " << state.turn.turn_number
       << " | " << toString(state.turn.phase)
       << " | " << toString(state.turn.turn_player) << "'s turn"
       << " | Decision #" << state.decision_index
       << " | Score: P1=" << state.players[0].score
       << " P2=" << state.players[1].score;
    snap.turn_info = ti.str();

    // Board ASCII (only for non-mulligan)
    if (state.turn.phase != TurnPhase::Mulligan) {
        snap.board_ascii = renderer.render(state);
    }

    // Decision text
    snap.decision_text = renderer.renderDecision(state, legal_actions, chosen_action);

    // Pre-game trace (anything emitted before the very first decision —
    // mulligan setup, opening hand, etc.) lives on snapshot 0 since there
    // is no "previous decision" to own it. Once the first snapshot exists,
    // all subsequent trace flushes go to the previous snapshot above.
    if (snapshots_.empty()) {
        snap.trace_lines = std::move(pending_trace_);
        pending_trace_.clear();
    }

    // Consume any pending MCTS value (set by setNextWinProb just before
    // this call). Cleared after consumption so the next decision either
    // gets a fresh value or reads as "no estimate."
    snap.mcts_value  = pending_mcts_value_;
    snap.mcts_sims   = pending_mcts_sims_;
    snap.mcts_player = pending_mcts_player_;
    pending_mcts_value_  = -2.0;
    pending_mcts_sims_   = 0;
    pending_mcts_player_ = -1;

    snapshots_.push_back(std::move(snap));
}

std::string ReplayWriter::escapeHtml(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

std::string ReplayWriter::escapeJs(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 10);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    out += ' ';
                } else {
                    out += c;
                }
        }
    }
    return out;
}

void ReplayWriter::writeHtml() {
    std::ofstream file(output_path_);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open replay file: " + output_path_);
    }

    file << R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Riftbound Game Replay</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
    background: #1a1a2e; color: #e0e0e0;
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
    font-size: 13px; overflow: hidden; height: 100vh;
}
#header {
    background: #16213e; padding: 8px 16px; border-bottom: 2px solid #0f3460;
    display: flex; justify-content: space-between; align-items: center;
}
#header h1 { font-size: 16px; color: #e94560; }
#nav { display: flex; gap: 8px; align-items: center; }
#nav button {
    background: #0f3460; color: #e0e0e0; border: 1px solid #e94560;
    padding: 4px 12px; cursor: pointer; font-family: inherit; font-size: 12px;
}
#nav button:hover { background: #e94560; }
#counter { color: #a0a0a0; font-size: 13px; min-width: 120px; text-align: center; }
#turn-info {
    background: #0f3460; padding: 6px 16px; font-size: 13px;
    color: #e94560; font-weight: bold; border-bottom: 1px solid #333;
}
/* Phase-seam visual treatment — distinct from decision snapshots so a
   reader scrubbing through with arrow keys can spot phase boundaries
   instantly. Yellow band; the right-panel decision area carries a big
   "▶ PHASE BEGINS" banner. */
body.seam #turn-info {
    background: #5d3a00; color: #ffd166;
    box-shadow: 0 0 8px #ffd16640 inset;
}
body.seam #decision-panel {
    background: #2a1e08;
    border-bottom: 2px solid #ffd166;
}
body.seam #decision-panel pre { color: #ffd166; }
#main { display: flex; height: calc(100vh - 80px); }
#board-panel {
    flex: 3; overflow-y: auto; padding: 8px; border-right: 2px solid #0f3460;
    min-width: 0;
}
#right-panel { flex: 2; display: flex; flex-direction: column; min-width: 0;
    border-right: 2px solid #0f3460; }
#decision-panel {
    padding: 8px; border-bottom: 2px solid #0f3460;
    max-height: 40%; overflow-y: auto;
}
#trace-panel { flex: 1; padding: 8px; overflow-y: auto; }
/* Card-details side pane: shows the official card image for any name
   the user clicked in board / decision / trace panes. Card-link spans
   are added by JS at show() time over every text node. */
#card-details-panel {
    flex: 1.6; padding: 12px; overflow-y: auto;
    display: flex; flex-direction: column; align-items: center;
    background: #0c1933; min-width: 0;
}
#card-details-panel .label { align-self: stretch; }
#card-details-name {
    color: #feca57; font-size: 14px; font-weight: bold;
    margin-bottom: 8px; text-align: center; min-height: 1.4em;
}
#card-details-image {
    max-width: 100%; max-height: calc(100vh - 200px);
    object-fit: contain; border: 1px solid #0f3460;
    background: #16213e;
    display: none;  /* hidden until first click */
}
#card-details-placeholder {
    color: #5a6985; font-size: 12px; text-align: center; margin-top: 40px;
    font-style: italic;
}
.card-link {
    cursor: pointer; color: #54a0ff; text-decoration: underline dotted;
    text-underline-offset: 2px;
}
.card-link:hover { color: #feca57; background: #16213e; }
.card-link.active { color: #feca57; background: #2a1e08; }
/* Drag handles for resizing panes. Vertical handles (col-resize)
   sit between flex children of #main and resize the panel to their
   LEFT by tweaking its flex-basis. Horizontal handles (row-resize)
   resize the panel ABOVE them within #right-panel.
   The handle itself is 4px wide/tall with a 1px accent line; hover
   widens the visual hit area. Drag state lives in JS — no
   localStorage, sizing resets each page load. */
.resize-handle-v {
    flex: 0 0 4px; background: #0f3460; cursor: col-resize;
    transition: background 0.1s;
}
.resize-handle-v:hover, .resize-handle-v.dragging { background: #e94560; }
.resize-handle-h {
    flex: 0 0 4px; background: #0f3460; cursor: row-resize;
    transition: background 0.1s;
}
.resize-handle-h:hover, .resize-handle-h.dragging { background: #e94560; }
body.dragging-pane { cursor: col-resize; user-select: none; }
body.dragging-pane-v { cursor: row-resize; user-select: none; }
pre {
    white-space: pre; font-family: inherit; font-size: 12px; line-height: 1.4;
}
#board-panel pre { color: #c8d6e5; }
#decision-panel pre { color: #feca57; }
#trace-panel pre { color: #a0a0a0; }
.trace-line { margin: 0; }
.trace-trc { color: #54a0ff; }
.trace-dbg { color: #ff9ff3; }
.trace-score { color: #00d2d3; font-weight: bold; }
.trace-kill { color: #ee5a24; }
.trace-effect { color: #7bed9f; }
.trace-combat { color: #f9ca24; font-weight: bold; }
.label { color: #666; font-size: 11px; text-transform: uppercase; margin-bottom: 4px; }
</style>
</head>
<body>
<div id="header">
    <h1 id="title">Riftbound Replay</h1>
    <div id="nav">
        <button onclick="go(-10)">&laquo;-10</button>
        <button onclick="go(-1)">&larr; Prev</button>
        <span id="counter">0 / 0</span>
        <button onclick="go(1)">Next &rarr;</button>
        <button onclick="go(10)">+10&raquo;</button>
    </div>
</div>
<div id="turn-info"></div>
<div id="main">
    <div id="board-panel"><div class="label">Board State</div><pre id="board"></pre></div>
    <div class="resize-handle-v" data-target="board-panel" data-axis="x"></div>
    <div id="right-panel">
        <div id="decision-panel"><div class="label">Decision</div><pre id="decision"></pre></div>
        <div class="resize-handle-h" data-target="decision-panel" data-axis="y"></div>
        <div id="trace-panel"><div class="label">Trace Log</div><pre id="trace"></pre></div>
    </div>
    <div class="resize-handle-v" data-target="right-panel" data-axis="x"></div>
    <div id="card-details-panel">
        <div class="label">Card Details</div>
        <div id="card-details-name"></div>
        <img id="card-details-image" alt="" />
        <div id="card-details-placeholder">Click any card name to view its image.</div>
    </div>
</div>
<script>
)HTML";

    // Write snapshot data as JS array
    file << "const HEADER = \"" << escapeJs(game_header_) << "\";\n";
    file << "const ENGINE_VERSION = \"" << kVersionTag << "\";\n";
    file << "const ENGINE_BUILD = \"" << kBuildDate << "\";\n";

    // Card-name → official image URL. Populated from gallery_raw.json.
    // Used by the JS card-name wrapper to make every name in board /
    // decision / trace panels clickable; clicking sets the side-pane
    // image src to the URL below.
    {
        auto image_map = loadCardImageMap();
        file << "const CARD_IMAGES = {\n";
        bool first = true;
        for (const auto& [name, url] : image_map) {
            if (!first) file << ",\n";
            file << "  \"" << escapeJs(name) << "\":\"" << escapeJs(url) << "\"";
            first = false;
        }
        file << "\n};\n\n";
    }

    file << "const SNAPSHOTS = [\n";

    for (size_t i = 0; i < snapshots_.size(); ++i) {
        auto& snap = snapshots_[i];
        file << "{\n";
        file << "  idx: " << snap.decision_index << ",\n";
        file << "  turn: \"" << escapeJs(snap.turn_info) << "\",\n";
        file << "  board: \"" << escapeJs(snap.board_ascii) << "\",\n";
        file << "  decision: \"" << escapeJs(snap.decision_text) << "\",\n";
        file << "  trace: [";
        for (size_t j = 0; j < snap.trace_lines.size(); ++j) {
            if (j > 0) file << ",";
            file << "\"" << escapeJs(snap.trace_lines[j]) << "\"";
        }
        file << "],\n";
        file << "  v: " << snap.mcts_value
             << ", s: " << snap.mcts_sims
             << ", mp: " << snap.mcts_player
             << ", seam: " << (snap.is_seam ? 1 : 0)
             << ", phase: \"" << escapeJs(snap.seam_phase) << "\"\n";
        file << "}";
        if (i + 1 < snapshots_.size()) file << ",";
        file << "\n";
    }

    file << "];\n\n";

    // Write remaining trace lines (after last decision, e.g. game over)
    file << "const FINAL_TRACE = [";
    for (size_t i = 0; i < pending_trace_.size(); ++i) {
        if (i > 0) file << ",";
        file << "\"" << escapeJs(pending_trace_[i]) << "\"";
    }
    file << "];\n\n";

    file << R"HTML(
let pos = 0;

// Build a single regex that matches any known card name. Sort
// longer-first so "Ivern, Friend to All" doesn't get pre-empted by
// "Ivern". Each name is regex-escaped so commas / apostrophes /
// punctuation work as literals.
const CARD_NAMES_SORTED = Object.keys(CARD_IMAGES).sort((a, b) => b.length - a.length);
function escapeRegex(s) { return s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'); }
const CARD_NAME_RE = CARD_NAMES_SORTED.length
    ? new RegExp('(' + CARD_NAMES_SORTED.map(escapeRegex).join('|') + ')', 'g')
    : null;

// Replace every known card-name occurrence in `elem`'s descendant text
// nodes with a clickable `<span class="card-link" data-card="…">`.
// Operates on text nodes only — preserves the surrounding <pre>
// whitespace and existing color spans in the trace panel. Safe to
// call repeatedly; new text replaces old DOM each show() cycle.
function wrapCardNames(elem) {
    if (!CARD_NAME_RE) return;
    const walker = document.createTreeWalker(elem, NodeFilter.SHOW_TEXT, null);
    const nodes = [];
    while (walker.nextNode()) nodes.push(walker.currentNode);
    for (const node of nodes) {
        const text = node.nodeValue;
        if (!CARD_NAME_RE.test(text)) continue;
        CARD_NAME_RE.lastIndex = 0; // reset for split
        const parts = text.split(CARD_NAME_RE);
        if (parts.length <= 1) continue;
        const frag = document.createDocumentFragment();
        for (const part of parts) {
            if (CARD_IMAGES.hasOwnProperty(part)) {
                const span = document.createElement('span');
                span.className = 'card-link';
                span.dataset.card = part;
                span.textContent = part;
                frag.appendChild(span);
            } else {
                frag.appendChild(document.createTextNode(part));
            }
        }
        node.parentNode.replaceChild(frag, node);
    }
}

// Pane resizing — handles between panels let the user drag to
// re-proportion. Vertical handles (data-axis="x") resize the panel
// to their LEFT by setting its explicit flex basis. Horizontal
// handles (data-axis="y") resize the panel ABOVE them likewise.
// No persistence — sizes reset each page load.
(function setupResize() {
    let drag = null;  // { handle, target, axis, startPos, startSize }
    document.addEventListener('pointerdown', function(e) {
        const handle = e.target.closest('.resize-handle-v, .resize-handle-h');
        if (!handle) return;
        const target = document.getElementById(handle.dataset.target);
        if (!target) return;
        const axis = handle.dataset.axis;
        const rect = target.getBoundingClientRect();
        drag = {
            handle: handle,
            target: target,
            axis: axis,
            startPos: axis === 'x' ? e.clientX : e.clientY,
            startSize: axis === 'x' ? rect.width : rect.height,
        };
        handle.classList.add('dragging');
        handle.setPointerCapture && handle.setPointerCapture(e.pointerId);
        document.body.classList.add(axis === 'x' ? 'dragging-pane' : 'dragging-pane-v');
        // The decision-panel has max-height: 40% baked into CSS;
        // override on drag so the user can grow it past that ceiling.
        if (axis === 'y') target.style.maxHeight = 'none';
        e.preventDefault();
    });
    document.addEventListener('pointermove', function(e) {
        if (!drag) return;
        const pos = drag.axis === 'x' ? e.clientX : e.clientY;
        const delta = pos - drag.startPos;
        const newSize = Math.max(80, drag.startSize + delta);
        // 80px minimum so a pane can't disappear; user can drag back.
        drag.target.style.flex = '0 0 ' + newSize + 'px';
    });
    document.addEventListener('pointerup', function() {
        if (!drag) return;
        drag.handle.classList.remove('dragging');
        document.body.classList.remove('dragging-pane', 'dragging-pane-v');
        drag = null;
    });
})();

// Single delegated click handler for all card-link spans across the
// three panels. Updates the side-pane image + name; tracks the active
// link with a class so the user can see what they just clicked.
let activeLink = null;
document.addEventListener('click', function(e) {
    const link = e.target.closest && e.target.closest('.card-link');
    if (!link) return;
    const name = link.dataset.card;
    const url = CARD_IMAGES[name];
    if (!url) return;
    const img = document.getElementById('card-details-image');
    const nameLabel = document.getElementById('card-details-name');
    const placeholder = document.getElementById('card-details-placeholder');
    img.src = url;
    img.alt = name;
    img.style.display = 'block';
    nameLabel.textContent = name;
    if (placeholder) placeholder.style.display = 'none';
    if (activeLink && activeLink !== link) activeLink.classList.remove('active');
    link.classList.add('active');
    activeLink = link;
});

function colorTrace(line) {
    let cls = 'trace-line';
    if (line.includes('SCORE') || line.includes('CONQUER')) cls = 'trace-score';
    else if (line.includes('KILL') || line.includes('BURN_OUT')) cls = 'trace-kill';
    else if (line.includes('EFFECT:')) cls = 'trace-effect';
    else if (line.includes('COMBAT') || line.includes('DAMAGE_STEP') || line.includes('SHOWDOWN')) cls = 'trace-combat';
    else if (line.includes('[DBG]')) cls = 'trace-dbg';
    else if (line.includes('[TRC]')) cls = 'trace-trc';
    return '<div class="' + cls + '">' + line.replace(/</g,'&lt;').replace(/>/g,'&gt;') + '</div>';
}

function show(i) {
    if (i < 0) i = 0;
    if (i >= SNAPSHOTS.length) i = SNAPSHOTS.length - 1;
    pos = i;
    const s = SNAPSHOTS[i];
    // Toggle the seam class on <body> so the CSS picks up the
    // distinctive yellow palette for phase-transition stops.
    document.body.classList.toggle('seam', !!s.seam);
    // Counter shows position + abbreviated turn context so you can tell
    // at a glance which turn/phase/player you're on without having to
    // read the turn-info bar. Parses the snapshot's turn string
    //   "Turn N | <Phase> | Pn's turn | …"
    // or the seam variant
    //   "Turn N | ▶ XxxPhase begins | Pn's turn | …"
    // into "T<N> P<n> <ShortPhase>".
    function shortTurn(turn) {
        const m = /Turn (\d+) \| ([^|]+) \| P(\d)/.exec(turn);
        if (!m) return '';
        const turnN = m[1];
        const p = m[3];
        let phase = m[2].trim();
        const seam = /^▶ (\w+?)(?:Phase|Step)? begins/.exec(phase);
        if (seam) phase = seam[1];
        else phase = phase.replace(/Phase$/, '').replace(/Step$/, '');
        return ' · T' + turnN + ' P' + p + ' ' + phase;
    }
    document.getElementById('counter').textContent =
        (i + 1) + ' / ' + SNAPSHOTS.length + shortTurn(s.turn);
    // Build the turn-info bar:
    //   [base turn info]  |  [optional SCORE DELTA banner]
    //
    // The score delta banner only appears when this snapshot's totals
    // differ from the previous one — i.e. a SCORE event fired in the
    // trace between them. Bright cyan, prefixed with "+N" so a reader
    // can scan the replay timeline visually to spot scoring moments
    // instead of grepping the trace for "[TRC] SCORE:".
    let turnLine = s.turn;
    // MCTS root-value badge — appears whenever the deciding agent was
    // MCTS (random-agent decisions store v = -2.0 as the "no estimate"
    // sentinel). v is the root mean reward from mcts_player's POV.
    // Both players' win-probs are computed in the zero-sum sense:
    //   P(win, mcts_player) = (v + 1) / 2
    //   P(win, opponent)    = 1 - that
    // Each side gets its own color-coded swatch so both trajectories
    // are visible side-by-side as you scrub through decisions.
    function colorFor(p) {
        if (p >= 0.7) return '#1dd1a1'; // green — winning
        if (p < 0.3)  return '#ee5253'; // red   — losing
        return '#feca57';               // yellow — uncertain
    }
    if (typeof s.v === 'number' && s.v >= -1.0 && (s.mp === 0 || s.mp === 1)) {
        const p_mcts = (s.v + 1) / 2;
        const p_opp  = 1 - p_mcts;
        const p1 = (s.mp === 0) ? p_mcts : p_opp;
        const p2 = (s.mp === 1) ? p_mcts : p_opp;
        const p1Str = (p1 * 100).toFixed(1);
        const p2Str = (p2 * 100).toFixed(1);
        turnLine += '  |  P(win) ' +
                    '<span style="color:' + colorFor(p1) + '; font-weight:bold;">P1=' + p1Str + '%</span>' +
                    ' / ' +
                    '<span style="color:' + colorFor(p2) + '; font-weight:bold;">P2=' + p2Str + '%</span>' +
                    ' <span style="color:#888;">[v=' + s.v.toFixed(3) +
                    ', sims=' + s.s +
                    ', deciding=P' + (s.mp + 1) + ']</span>';
    }
    if (i > 0) {
        // Parse "Score: P1=X P2=Y" out of the turn strings and compare.
        const re = /P1=(\d+)\s+P2=(\d+)/;
        const cur = re.exec(s.turn);
        const prev = re.exec(SNAPSHOTS[i - 1].turn);
        if (cur && prev) {
            const d1 = parseInt(cur[1]) - parseInt(prev[1]);
            const d2 = parseInt(cur[2]) - parseInt(prev[2]);
            const parts = [];
            if (d1 > 0) parts.push('P1 +' + d1);
            if (d2 > 0) parts.push('P2 +' + d2);
            if (parts.length > 0) {
                turnLine += '  |  <span style="color:#00d2d3; font-weight:bold;">' +
                            '★ SCORE: ' + parts.join(', ') + '</span>';
            }
        }
    }
    document.getElementById('turn-info').innerHTML = turnLine;
    document.getElementById('board').textContent = s.board;
    document.getElementById('decision').textContent = s.decision || '(no decision recorded)';
    wrapCardNames(document.getElementById('board'));
    wrapCardNames(document.getElementById('decision'));

    // Show trace lines caused BY this snapshot's chosen action — the
    // events fired between this decision being applied and the next
    // decision prompting (effect resolutions, peeks, reveals, scoring,
    // etc.). Each decision's log thus aligns with its own consequences.
    // Snapshot 0 additionally carries any pre-game setup trace
    // (mulligan, opening hand) since it has no predecessor.
    let traceLines = s.trace || [];
    // Append FINAL_TRACE on the very last snapshot so end-of-game events
    // (final scoring, GAME_OVER) still show up.
    if (i === SNAPSHOTS.length - 1) {
        traceLines = traceLines.concat(FINAL_TRACE);
    }
    document.getElementById('trace').innerHTML = traceLines.map(colorTrace).join('');
    wrapCardNames(document.getElementById('trace'));
}

function go(delta) { show(pos + delta); }

// Skip over consecutive seams in one direction so users who don't
// want to stop on every phase boundary can still scrub quickly with
// Shift+Arrow.
function goSkipSeams(dir) {
    let i = pos + dir;
    while (i >= 0 && i < SNAPSHOTS.length && SNAPSHOTS[i].seam) {
        i += dir;
    }
    show(i);
}

document.addEventListener('keydown', function(e) {
    if (e.shiftKey && (e.key === 'ArrowRight' || e.key === 'L')) goSkipSeams(1);
    else if (e.shiftKey && (e.key === 'ArrowLeft' || e.key === 'H')) goSkipSeams(-1);
    else if (e.key === 'ArrowRight' || e.key === 'l') go(1);
    else if (e.key === 'ArrowLeft' || e.key === 'h') go(-1);
    else if (e.key === 'Home') show(0);
    else if (e.key === 'End') show(SNAPSHOTS.length - 1);
    else if (e.key === 'PageDown') go(10);
    else if (e.key === 'PageUp') go(-10);
});

document.getElementById('title').textContent =
    'Riftbound Replay — ' + HEADER + '  [engine ' + ENGINE_VERSION +
    ', built ' + ENGINE_BUILD + ']';
show(0);
</script>
</body>
</html>
)HTML";

    file.close();
}

} // namespace riftbound
