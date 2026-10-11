#pragma once
/// @file mcts_agent.h
/// AgentInterface adapter for OpenSpiel's MCTSBot / ISMCTSBot.
///
/// MCTS works on a clone-able `open_spiel::State` that wraps the
/// engine. The agent keeps one such state as a mirror of the game it is
/// playing in: at each `selectAction` it replays the choices the live
/// game has made since the agent's previous decision
/// (`GameState::action_history`, one exact token per decision) onto the
/// mirror, then searches from it. The mirror is built from the same
/// decks and engine seed, so it is the live position exactly — RNG stream
/// included — at every kind of decision, mid-chain and mid-combat too.
///
/// If the history cannot reproduce the live state (wrong `engine_seed`,
/// or `StateEditor` god-mode edits, which bypass the engine and its
/// history), the agent falls back to resuming a copy of the live
/// GameState. That works only at top-level main-phase decisions, and the
/// copy's future shuffles are not the live game's; any other decision is
/// then answered with a uniformly random legal action instead of a search.
///
/// The agent is single-threaded internally. Two instances in the same
/// game are independent: each owns its mirror and its search tree.
///
/// Wiring: see `buildAgent` in `src/main.cpp` (`mcts:sims=N`).

#include "agent_interface.h"

#include <cstdint>
#include <memory>
#include <string>

namespace riftbound {

class MctsAgent : public AgentInterface {
public:
    /// `deck1_path` / `deck2_path` MUST match the decks of the engine
    /// running this game. `registry_path` is ignored (card data is
    /// compiled in); the parameter is kept for source compatibility.
    /// `engine_seed` MUST equal the seed passed to GameEngine::runGame
    /// for this game: the mirror replays the live game's choices on an
    /// engine seeded with it. With a different seed the replay does not
    /// reproduce the live state; the agent detects that and degrades as
    /// described above rather than planning in a phantom game.
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
/// the resampler is a Clone (no hidden-info determinization yet).
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
