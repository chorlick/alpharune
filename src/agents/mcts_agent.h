#pragma once
/// @file mcts_agent.h
/// AgentInterface adapters for search: OpenSpiel's MCTSBot, and an information-set
/// MCTS (the ISMCTSBot algorithm, see IsMctsSearch in mcts_agent.cpp).
///
/// `MctsAgent` is perfect-information search (it sees hidden cards);
/// `IsMctsAgent` is honest by default: it searches worlds drawn from what its
/// player knows (determinizer.h). Both need the mirror described here — the
/// honest one only as the position its worlds are drawn from.
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
#include <vector>

namespace riftbound {

/// Perfect-information MCTS: searches in exact copies of the live game, so it
/// sees the opponent's hand, both deck orders and every future shuffle. A
/// benchmark rung only, never a teacher (it would teach acting on cards a
/// player cannot see); the honest search is IsMctsAgent.
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

/// What the last search of an `IsMctsAgent` found at its root, per option of
/// the live decision. For a recorder: the visit distribution is the search's
/// policy (a teacher's target), `root_value` its estimate of the position.
struct SearchSummary {
    /// False when the decision was answered without a search (one option, the
    /// setup battlefield choice, or no position to search from); the vectors
    /// are then empty and `chosen` is still the index answered.
    bool searched = false;
    /// Index into the live legal list of the option played.
    int chosen = -1;
    /// Per live legal index: root visits, and the mean return of the deciding
    /// player over them (0 where unvisited).
    std::vector<int> visits;
    std::vector<double> value;
    /// Visit-weighted mean return of the deciding player at the root.
    double root_value = 0.0;
    int simulations = 0;
};

/// Information-set MCTS — same wiring shape as MctsAgent. The search is the
/// algorithm of OpenSpiel's `ISMCTSBot` (UCT, one world per simulation, tree
/// nodes keyed by what the player to move observes), implemented in
/// mcts_agent.cpp with ordered containers so that a game is a function of its
/// seeds (`IsMctsSearch`).
///
/// **Honest by default** (`WorldModel::Sampled`): each simulation runs in a
/// world drawn by `WorldSampler` (determinizer.h) from what the deciding
/// player knows — the opponent's hand, facedown cards and deck drawn from a
/// model of a legal deck around their legend, both deck orders shuffled except
/// the slots the player knows, the engine's RNG reseeded. The search never
/// reads a card the player cannot see: two live games that differ only in
/// such cards give the same search (IsMctsHonestTest). It still needs both
/// deck lists and the engine seed, as MctsAgent does, to keep a mirror of the
/// live game that the worlds are drawn from — only the drawing is honest.
///
/// `WorldModel::Perfect` searches in copies of the real game instead: it sees
/// the opponent's hand and both deck orders. A benchmark rung only, never a
/// teacher.
///
/// The move played is the most visited root option (ties broken by the
/// agent's RNG); `lastSearch()` gives the whole root.
///
/// Decisions not searched: the setup battlefield choice (the opponent's pick
/// is held by the engine's suspended call, which a world cannot redraw; CR
/// 487.5 makes the choice random in a Duel, 488.5 chosen in a Match, and
/// which one an engine game follows is open — item G4) is answered uniformly
/// from the agent's RNG.
class IsMctsAgent : public AgentInterface {
public:
    enum class WorldModel { Sampled, Perfect };

    IsMctsAgent(std::string deck1_path,
                std::string deck2_path,
                std::string registry_path,
                uint64_t    engine_seed,
                uint64_t    mcts_seed,
                int         sims,
                WorldModel  world = WorldModel::Sampled);
    ~IsMctsAgent() override;

    IsMctsAgent(const IsMctsAgent&) = delete;
    IsMctsAgent& operator=(const IsMctsAgent&) = delete;

    Intent selectAction(
        const GameState& state,
        const std::vector<Intent>& legal_actions
    ) override;

    /// The root of the search behind the last answer (see SearchSummary).
    const SearchSummary& lastSearch() const;

    /// Check every sampled world against the searcher's knowledge
    /// (WorldSampler::check) and throw std::logic_error on a disagreement.
    /// Off by default (costs a second pass per simulation); also switched on
    /// by the environment variable RIFTBOUND_ISMCTS_CHECK_WORLDS=1.
    void setCheckWorlds(bool on);
    /// Worlds checked so far.
    long worldsChecked() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace riftbound
