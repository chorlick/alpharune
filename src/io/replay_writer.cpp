#include "replay_writer.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace riftbound {

ReplayWriter::ReplayWriter(const CardDB& db, const std::string& output_path)
    : db_(db), output_path_(output_path) {}

void ReplayWriter::setGameHeader(const std::string& header) {
    game_header_ = header;
}

void ReplayWriter::addTraceLine(const std::string& line) {
    pending_trace_.push_back(line);
}

void ReplayWriter::recordDecision(const GameState& state,
                                   const std::vector<Intent>& legal_actions,
                                   const Intent& chosen_action,
                                   const StateRenderer& renderer) {
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

    // Attach pending trace lines (from before this decision)
    snap.trace_lines = std::move(pending_trace_);
    pending_trace_.clear();

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
#main { display: flex; height: calc(100vh - 80px); }
#board-panel {
    flex: 3; overflow-y: auto; padding: 8px; border-right: 2px solid #0f3460;
    min-width: 0;
}
#right-panel { flex: 2; display: flex; flex-direction: column; min-width: 0; }
#decision-panel {
    padding: 8px; border-bottom: 2px solid #0f3460;
    max-height: 40%; overflow-y: auto;
}
#trace-panel { flex: 1; padding: 8px; overflow-y: auto; }
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
    <div id="right-panel">
        <div id="decision-panel"><div class="label">Decision</div><pre id="decision"></pre></div>
        <div id="trace-panel"><div class="label">Trace Log</div><pre id="trace"></pre></div>
    </div>
</div>
<script>
)HTML";

    // Write snapshot data as JS array
    file << "const HEADER = \"" << escapeJs(game_header_) << "\";\n";
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
        file << "]\n";
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
    document.getElementById('counter').textContent = (i + 1) + ' / ' + SNAPSHOTS.length;
    document.getElementById('turn-info').textContent = s.turn;
    document.getElementById('board').textContent = s.board;
    document.getElementById('decision').textContent = s.decision || '(trivial decision)';

    // Show trace lines from the NEXT snapshot (what happened after this decision)
    let traceLines = [];
    if (i + 1 < SNAPSHOTS.length) {
        traceLines = SNAPSHOTS[i + 1].trace;
    } else {
        traceLines = FINAL_TRACE;
    }
    document.getElementById('trace').innerHTML = traceLines.map(colorTrace).join('');
}

function go(delta) { show(pos + delta); }

document.addEventListener('keydown', function(e) {
    if (e.key === 'ArrowRight' || e.key === 'l') go(1);
    else if (e.key === 'ArrowLeft' || e.key === 'h') go(-1);
    else if (e.key === 'Home') show(0);
    else if (e.key === 'End') show(SNAPSHOTS.length - 1);
    else if (e.key === 'PageDown') go(10);
    else if (e.key === 'PageUp') go(-10);
});

document.getElementById('title').textContent = 'Riftbound Replay — ' + HEADER;
show(0);
</script>
</body>
</html>
)HTML";

    file.close();
}

} // namespace riftbound
