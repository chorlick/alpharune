/// @file prompt_trace.cpp
/// One traced game, as `riftbound --trace --debug` plays it, with what every
/// decision asks (describePrompt, testing phase T1) for the player deciding.
///
///   prompt_trace --deck1 D1.txt --deck2 D2.txt --agent1 SPEC --agent2 SPEC --seed S
///
/// SPEC: random | random-player[:pass=P] | mcts:sims=N | ismcts:sims=N[,world=perfect].
/// The decks, agents and seeds are built as `riftbound` builds them for one
/// game (game seed = S & 0x7FFFFFFF, agent seeds 2*seed and 2*seed+1), so the
/// game is the one `riftbound --seed S` plays: every trace line it prints is
/// the same line in the same order. Interleaved with them, after each
/// decision is answered (between its DECISION and CHOSE lines):
///
///   [PRM] #<decision index> P<n> <describePrompt text>
///   [PRM]   <k> <role> <option text>  (the chosen one marked "*")
///
/// Written for the round 2 reading of ISMCTS games (find and record only):
/// nothing here changes how a game is played.

#include "agents/mcts_agent.h"
#include "agents/random_agent.h"
#include "agents/random_player_agent.h"
#include "core/card_db.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "engine/prompt_description.h"
#include "openspiel/riftbound_game.h"
#include "rules/deck_validator.h"

#include <iostream>
#include <memory>
#include <string>

using namespace riftbound;

namespace {

class PromptPrinter : public AgentInterface {
public:
    PromptPrinter(AgentInterface& inner, const CardDB& db) : inner_(inner), db_(db) {}
    Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
        return inner_.selectAction(state, legal);
    }
    Intent decide(const GameState& state, const std::vector<Intent>& legal,
                  const Prompt& prompt) override {
        Intent chosen = inner_.decide(state, legal, prompt);
        const PlayerId who = prompt.decider;
        const PromptDescription d = describePrompt(state, prompt, legal, who, db_);
        std::cout << "  [PRM] #" << state.decision_index << " P"
                  << (who == PlayerId::Player1 ? 1 : who == PlayerId::Player2 ? 2 : 0) << " "
                  << d.text << "\n";
        int k = 0;
        for (const auto& o : d.options) {
            const bool is_chosen = legal[o.index] == chosen;
            std::cout << "  [PRM]   " << (is_chosen ? "*" : " ") << k++ << " "
                      << toString(o.role) << " " << o.text << "\n";
        }
        return chosen;
    }

private:
    AgentInterface& inner_;
    const CardDB& db_;
};

std::unique_ptr<AgentInterface> build(const std::string& spec, uint64_t game_seed,
                                      uint64_t derived, const std::string& d1,
                                      const std::string& d2) {
    if (spec == "random") return std::make_unique<RandomAgent>(derived);
    if (spec.rfind("random-player", 0) == 0) {
        double pass = kDefaultPassProbability;
        const auto at = spec.find("pass=");
        if (at != std::string::npos) pass = std::stod(spec.substr(at + 5));
        return std::make_unique<RandomPlayerAgent>(derived, pass);
    }
    const auto at = spec.find("sims=");
    if (at == std::string::npos) throw std::runtime_error("bad agent spec " + spec);
    const int sims = std::stoi(spec.substr(at + 5));
    if (spec.rfind("ismcts", 0) == 0) {
        const bool perfect = spec.find("world=perfect") != std::string::npos;
        return std::make_unique<IsMctsAgent>(d1, d2, "", game_seed, derived, sims,
                                             perfect ? IsMctsAgent::WorldModel::Perfect
                                                     : IsMctsAgent::WorldModel::Sampled);
    }
    if (spec.rfind("mcts", 0) == 0)
        return std::make_unique<MctsAgent>(d1, d2, "", game_seed, derived, sims);
    throw std::runtime_error("bad agent spec " + spec);
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string d1, d2, a1 = "random", a2 = "random";
    uint64_t seed = 1;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string f = argv[i], v = argv[i + 1];
        if (f == "--deck1") d1 = v;
        else if (f == "--deck2") d2 = v;
        else if (f == "--agent1") a1 = v;
        else if (f == "--agent2") a2 = v;
        else if (f == "--seed") seed = std::stoull(v);
        else { std::cerr << "unknown flag " << f << "\n"; return 2; }
    }
    if (d1.empty() || d2.empty()) { std::cerr << "--deck1 and --deck2 are required\n"; return 2; }
    const auto& shared = openspiel::sharedCards();
    const CardDB& db = shared.db;
    const DeckSubmission deck1 = DeckValidator::loadFromDeckList(d1, db);
    const DeckSubmission deck2 = DeckValidator::loadFromDeckList(d2, db);
    const uint64_t game_seed = seed & 0x7FFFFFFFu;
    auto agent1 = build(a1, game_seed, game_seed * 2, d1, d2);
    auto agent2 = build(a2, game_seed, game_seed * 2 + 1, d1, d2);
    PromptPrinter seat1(*agent1, db), seat2(*agent2, db);

    EventBus events;
    GameEngine engine(db, events, shared.registry);
    events.on_log.connect([](const LogEvent& e) {
        const char* prefix = "[TRC]";
        if (e.level == LogLevel::Debug) prefix = "[DBG]";
        if (e.level == LogLevel::Info) prefix = "[INF]";
        if (e.level == LogLevel::Warning) prefix = "[WRN]";
        std::cout << "  " << prefix << " " << e.message << "\n";
    });
    try {
        auto r = engine.runGame(deck1, deck2, seat1, seat2, game_seed);
        std::cout << "\nGame over: winner=" << static_cast<int>(r.winner)
                  << " | reason=" << r.termination_reason << " | turns=" << r.total_turns << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[engine] exception: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
