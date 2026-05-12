#pragma once
/// @file replay_writer.h
/// HTML game replay writer — generates a self-contained HTML file
/// with arrow-key navigation through game decision points.
///
/// Each snapshot captures the ASCII board state, the decision made,
/// and trace log lines. The HTML viewer shows board + decision + log
/// side by side with keyboard navigation.

#include "core/events.h"
#include "core/game_state.h"
#include "core/intent.h"
#include "io/state_renderer.h"

#include <string>
#include <vector>

namespace riftbound {

class ReplayWriter {
public:
    ReplayWriter(const CardDB& db, const std::string& output_path);

    /// Record a decision point snapshot.
    void recordDecision(const GameState& state,
                        const std::vector<Intent>& legal_actions,
                        const Intent& chosen_action,
                        const StateRenderer& renderer);

    /// Record a trace log line (accumulated between decisions).
    void addTraceLine(const std::string& line);

    /// Record game header info.
    void setGameHeader(const std::string& header);

    /// Write the HTML file. Call once at game end.
    void writeHtml();

private:
    const CardDB& db_;
    std::string output_path_;
    std::string game_header_;

    struct Snapshot {
        int decision_index;
        std::string turn_info;       // "Turn 5 | MainPhase | P1"
        std::string board_ascii;     // full ASCII board render
        std::string decision_text;   // decision block (options + chosen)
        std::vector<std::string> trace_lines; // trace lines AFTER this decision
    };

    std::vector<Snapshot> snapshots_;
    std::vector<std::string> pending_trace_; // trace lines not yet assigned

    static std::string escapeHtml(const std::string& s);
    static std::string escapeJs(const std::string& s);
};

} // namespace riftbound
