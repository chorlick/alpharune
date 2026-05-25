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

#include <map>
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

    /// Record a phase-transition "seam" — a noop visual stop in the
    /// snapshot stream that pairs phase-scoped trace events with their
    /// phase. Pre-seam trace accumulates against the previous snapshot;
    /// post-seam trace accumulates against the seam itself. Engine-side
    /// behavior is untouched — this is purely a rendering feature, only
    /// active when ReplayWriter is constructed (i.e., --render-html or
    /// the legacy --render flag).
    void recordPhaseSeam(const GameState& state,
                          TurnPhase old_phase,
                          TurnPhase new_phase,
                          const StateRenderer& renderer);

    /// Record a trace log line (accumulated between decisions).
    void addTraceLine(const std::string& line);

    /// Record game header info.
    void setGameHeader(const std::string& header);

    /// Attach an MCTS root-value estimate to the NEXT recordDecision call.
    /// `mean_value` is in [-1, +1] from `deciding_player`'s perspective
    /// (= MCTSBot root.total_reward / root.explore_count). The viewer
    /// converts to win probability for that player ((mean+1)/2) AND for
    /// the opponent (1 - that, since the game is zero-sum). `sims` is
    /// the simulation count for context. `deciding_player` is 0 for P1,
    /// 1 for P2. Cleared after consumption — only the next snapshot
    /// picks it up. Random-agent decisions skip this and the snapshot
    /// reads as "no estimate."
    void setNextWinProb(double mean_value, int sims, int deciding_player);

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
        std::vector<std::string> trace_lines; // events caused BY this
                                              // decision's chosen_action
                                              // (filled when the NEXT
                                              // recordDecision is called)
        // MCTS root-value estimate (mean simulated reward) from
        // mcts_player's perspective. -2.0 = no estimate (random agent
        // or value never set). Range [-1,+1] when present. The viewer
        // computes both players' win-prob from this in the zero-sum
        // sense: P(win, mcts_player) = (v+1)/2; P(win, opponent) = 1 - that.
        double mcts_value  = -2.0;
        int    mcts_sims   = 0;
        int    mcts_player = -1;   // 0=P1, 1=P2; -1 = no estimate

        // Phase-seam snapshots are noop visual stops inserted at each
        // PhaseChangedEvent. They carry the new phase's name in
        // `seam_phase` and have empty decision_text. Regular decision
        // snapshots leave is_seam=false.
        bool        is_seam = false;
        std::string seam_phase;       // e.g. "Channel Phase"
    };

    std::vector<Snapshot> snapshots_;
    std::vector<std::string> pending_trace_; // trace lines not yet assigned
    // Pending MCTS root value for the next recordDecision call. Same
    // sentinel convention as Snapshot::mcts_value.
    double pending_mcts_value_  = -2.0;
    int    pending_mcts_sims_   = 0;
    int    pending_mcts_player_ = -1;

    static std::string escapeHtml(const std::string& s);
    static std::string escapeJs(const std::string& s);

public:
    /// Load card-name → image-URL map from `cards/raw/gallery_raw.json`.
    /// Probes ./cards/raw/gallery_raw.json first, then
    /// $RIFTBOUND_ROOT/cards/raw/gallery_raw.json, then walks upward
    /// from the current directory. Returns an empty map if no gallery
    /// file is found — callers should still render their HTML, the
    /// click-to-image affordance just won't have anything to display.
    /// Reused by `riftbound_play` to populate its in-browser card
    /// details panel.
    static std::map<std::string, std::string> loadCardImageMap();
};

} // namespace riftbound
