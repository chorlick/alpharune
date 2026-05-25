#pragma once
/// @file mcts_agent.h
/// AgentInterface adapter for OpenSpiel's MCTSBot / ISMCTSBot.
///
/// MCTS works on a clone-able `open_spiel::State` that wraps the
/// engine; this agent keeps one OpenSpiel game handle for the
/// duration of its life and, on each `selectAction` call, builds a
/// fresh state by replaying `state.action_history` from the start.
/// That's O(history-length) per decision — fine for an interactive
/// UI and modest sim counts; for high-throughput batch use the
/// dedicated openspiel match runner remains the better path until a
/// snapshot-based fast-clone agent lands.
///
/// Caveats:
///   • `StateEditor` god-mode edits do NOT participate in MCTS — the
///     OpenSpiel state is reconstructed from `action_history` alone.
///     MCTS will plan against the engine-implied state, not the
///     edited one. Documented in docs/play-api.md.
///   • The agent is single-threaded internally. Constructing two
///     MctsAgent instances on the same OpenSpiel Game is safe; the
///     bots own their own search trees.
///
/// Wiring: see `src/main_play.cpp`'s `buildAgent("mcts:sims=N", ...)`.

#include "agent_interface.h"

#include <cstdint>
#include <memory>
#include <string>

namespace riftbound {

class MctsAgent : public AgentInterface {
public:
    /// `deck1_path` / `deck2_path` / `registry_path` MUST match the
    /// values passed to the engine running this game.
    /// `engine_seed` MUST equal the seed passed to GameEngine::runGame
    /// for this game — the OpenSpiel game spec forwards it into the
    /// cloned state's internal engine, and replaying action_history
    /// only reproduces the live state when both engines use the same
    /// RNG seed. Passing a different seed here yields a phantom state
    /// where `decodeAction` can't map MCTS's chosen action into the
    /// real `legal_actions` vector and the agent silently degrades to
    /// `legal_actions[0]` every turn.
    /// `mcts_seed` drives MCTSBot's internal exploration RNG — it
    /// affects tiebreak order in the search tree but not the game
    /// state. Distinct per-seat so two MctsAgent instances in the
    /// same game explore independent trees.
    /// `sims` is the MCTSBot simulation budget per decision.
    MctsAgent(std::string deck1_path,
              std::string deck2_path,
              std::string registry_path,
              uint64_t    engine_seed,
              uint64_t    mcts_seed,
              int         sims);
    ~MctsAgent() override;

    MctsAgent(const MctsAgent&) = delete;
    MctsAgent& operator=(const MctsAgent&) = delete;

    Intent selectAction(
        const GameState& state,
        const std::vector<Intent>& legal_actions
    ) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Information-set MCTS variant — same wiring shape as MctsAgent but
/// uses `ISMCTSBot`. Currently behaves like perfect-info MCTS because
/// the resampler is a Clone (no hidden-info determinization yet — see
/// CLAUDE.md "Phase 6b" backlog).
class IsMctsAgent : public AgentInterface {
public:
    IsMctsAgent(std::string deck1_path,
                std::string deck2_path,
                std::string registry_path,
                uint64_t    engine_seed,
                uint64_t    mcts_seed,
                int         sims);
    ~IsMctsAgent() override;

    IsMctsAgent(const IsMctsAgent&) = delete;
    IsMctsAgent& operator=(const IsMctsAgent&) = delete;

    Intent selectAction(
        const GameState& state,
        const std::vector<Intent>& legal_actions
    ) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace riftbound
