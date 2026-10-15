/// What a decision asks (testing phase T1; trainer plan section 0, item 8).
///
/// Every decision carries a `Prompt` (core/prompt.h) on `StepResult::prompt`,
/// and `describePrompt` (engine/prompt_description.h) says what a player may
/// be shown of it. Checked here at every decision of random games — deck
/// lists and random constructed decks, `random` and `random-player` choices:
///   - every decision says what kind of question it is (no Unspecified), who
///     decides, and has a description whose options are the decision's options
///     (each once, in action-id order);
///   - the kind agrees with the options (a priority decision offers a pass, a
///     card's prompt offers only answers, ...);
///   - two decisions whose descriptions ask the same question mean the same:
///     the question's meaning is identified independently of the description,
///     by the code that asked (Prompt::where, the card's own call site for a
///     card's helper) and the asking card;
///   - another observer is told that a decision is made, of which kind and
///     by whom, but not the words or the options.
/// The leak tests (test_hidden_order_leak.cpp) compare descriptions between
/// positions a player cannot tell apart.

#include <gtest/gtest.h>

#include "agents/random_player_agent.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "engine/prompt_description.h"
#include "openspiel/action_vocab.h"
#include "rules/deck_validator.h"
#include "rules/random_deck.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace riftbound::test::prompt_description {

std::string deckDir() {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    return std::string(root ? root : "..") + "/decks/";
}

int gamesOr(int dflt) {
    if (const char* e = std::getenv("RIFTBOUND_PROMPT_GAMES")) return std::max(1, std::atoi(e));
    return dflt;
}

/// The meaning of a question, identified without the description: the code
/// that asked and the card that asked.
std::string meaningOf(const GameState& s, const Prompt& p) {
    std::string m = std::string(p.where.file_name()) + ":" + std::to_string(p.where.line());
    m += " kind " + std::to_string(static_cast<int>(p.kind));
    if (p.source != kInvalidId && s.objectExists(p.source))
        m += " card " + toString(s.getObject(p.source).card_def_id);
    return m;
}

class PromptCensus : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        registry_ = new CardRegistry();
        registry_->loadAll();
        db_ = new CardDB();
        db_->buildFromClasses(*registry_);
        builder_ = new RandomDeckBuilder(*db_);
        for (const auto& entry : std::filesystem::directory_iterator(deckDir()))
            if (entry.path().extension() == ".txt") lists_.push_back(entry.path().string());
        std::sort(lists_.begin(), lists_.end());
    }
    static void TearDownTestSuite() {
        delete builder_;
        delete db_;
        delete registry_;
    }
    static std::pair<DeckSubmission, DeckSubmission> decks(int g) {
        if (g % 2 == 0) {
            const size_t a = static_cast<size_t>(g / 2) * 5 % lists_.size();
            const size_t b = (a + 1 + static_cast<size_t>(g / 2) % (lists_.size() - 1)) %
                             lists_.size();
            return {DeckValidator::loadFromDeckList(lists_[a], *db_),
                    DeckValidator::loadFromDeckList(lists_[b], *db_)};
        }
        return {builder_->build(3000 + g), builder_->build(7000 + g)};
    }
    static CardRegistry* registry_;
    static CardDB* db_;
    static RandomDeckBuilder* builder_;
    static std::vector<std::string> lists_;
};

CardRegistry* PromptCensus::registry_ = nullptr;
CardDB* PromptCensus::db_ = nullptr;
RandomDeckBuilder* PromptCensus::builder_ = nullptr;
std::vector<std::string> PromptCensus::lists_;

bool has(const std::vector<Intent>& legal, IntentType t) {
    return std::any_of(legal.begin(), legal.end(), [t](const Intent& i) { return i.type == t; });
}

TEST_F(PromptCensus, EveryDecisionSaysWhatItAsksAndNoTwoMeaningsShareADescription) {
    const int games = gamesOr(40);
    long decisions = 0, prompts = 0;
    std::map<PromptKind, long> by_kind;
    std::map<std::string, std::set<std::string>> meanings;   // key -> meanings
    std::map<std::string, std::string> example;
    std::vector<std::string> problems;
    auto problem = [&](const std::string& s) { if (problems.size() < 30) problems.push_back(s); };
    for (int g = 0; g < games; ++g) {
        const auto [d1, d2] = decks(g);
        EventBus bus;
        GameEngine engine(*db_, bus, *registry_);
        const uint64_t seed = 4242000 + static_cast<uint64_t>(g);
        std::mt19937_64 rng(seed * 7 + 1);
        const double pass_p = g % 4 < 2 ? -1.0 : 0.2;
        engine.beginGame(d1, d2, seed);
        for (int n = 0; n < 5000 && !engine.isStepDone(); ++n) {
            const StepResult& step = engine.currentStep();
            const GameState& s = engine.state();
            const std::string where = "game " + std::to_string(g) + " decision " + std::to_string(n);
            ++decisions;
            const Prompt& p = step.prompt;
            ++by_kind[p.kind];
            if (p.kind == PromptKind::Unspecified)
                problem(where + ": no kind (asked at " + p.where.file_name() + ":" +
                        std::to_string(p.where.line()) + ")");
            if (p.decider != step.perspective) problem(where + ": the prompt names another decider");
            if (!(engine.pendingPrompt().kind == p.kind && engine.pendingPrompt().source == p.source))
                problem(where + ": pendingPrompt() is not the step's prompt");
            // The kind agrees with the options.
            const bool all_answers = std::all_of(step.legal.begin(), step.legal.end(),
                [](const Intent& i) { return i.type == IntentType::MakeChoice; });
            if (has(step.legal, IntentType::EndTurn) && p.kind != PromptKind::MainPhase)
                problem(where + ": an End Turn decision of kind " + toString(p.kind));
            if (has(step.legal, IntentType::PassPriority) && p.kind != PromptKind::ChainPriority)
                problem(where + ": a priority decision of kind " + toString(p.kind));
            if (has(step.legal, IntentType::PassFocus) && p.kind != PromptKind::ShowdownFocus)
                problem(where + ": a Focus decision of kind " + toString(p.kind));
            if (all_answers && (p.kind == PromptKind::MainPhase || p.kind == PromptKind::ChainPriority ||
                                p.kind == PromptKind::ShowdownFocus || p.kind == PromptKind::Mulligan))
                problem(where + ": a prompt of answers of kind " + toString(p.kind) + " ('" +
                        p.question + "')");
            if (all_answers) ++prompts;

            const PromptDescription mine = describePrompt(s, p, step.legal, step.perspective, *db_);
            if (mine.text.empty()) problem(where + ": no words");
            if (mine.decider != 1) problem(where + ": the decider is not told it decides");
            if (mine.options.size() != step.legal.size())
                problem(where + ": " + std::to_string(mine.options.size()) + " options described of " +
                        std::to_string(step.legal.size()));
            std::vector<bool> seen(step.legal.size(), false);
            int last_id = -1;
            for (const auto& o : mine.options) {
                if (o.index >= seen.size() || seen[o.index]) problem(where + ": an option twice");
                else seen[o.index] = true;
                if (o.action_id <= last_id) problem(where + ": options not in action-id order");
                last_id = o.action_id;
                if (o.text.empty()) problem(where + ": an option with no words");
                if ((o.role == OptionRole::Action) != (step.legal[o.index].type != IntentType::MakeChoice))
                    problem(where + ": an option's role does not match its type");
                if (o.role == OptionRole::ChainItem && o.value != -1)
                    problem(where + ": a chain item option shows the item's id");
            }
            // Another observer: who decides and what kind, nothing of the words.
            const PromptDescription theirs =
                describePrompt(s, p, step.legal, opponent(step.perspective), *db_);
            if (theirs.decider != 2 || theirs.kind != p.kind || !theirs.options.empty() ||
                !theirs.question.empty())
                problem(where + ": the other player is shown the decider's question or options");

            const std::string key = promptKey(mine);
            meanings[key].insert(meaningOf(s, p));
            if (!example.count(key)) example[key] = mine.text;

            const size_t pick = [&] {
                const std::vector<int> ids = openspiel::actionIds(step.legal, s);
                std::vector<size_t> order(step.legal.size());
                for (size_t i = 0; i < order.size(); ++i) order[i] = i;
                std::stable_sort(order.begin(), order.end(),
                                 [&](size_t a, size_t b) { return ids[a] < ids[b]; });
                if (pass_p < 0) return order[rng() % order.size()];
                std::vector<size_t> act, pass;
                for (size_t i : order) (isPassOption(step.legal, i) ? pass : act).push_back(i);
                const double u = std::uniform_real_distribution<double>(0.0, 1.0)(rng);
                if (act.empty() || (!pass.empty() && u < pass_p)) {
                    const auto& from = pass.empty() ? act : pass;
                    return from[rng() % from.size()];
                }
                return act[rng() % act.size()];
            }();
            engine.applyChoice(static_cast<int>(pick));
        }
    }
    for (const auto& [key, m] : meanings) {
        if (m.size() < 2) continue;
        std::string all;
        for (const auto& x : m) all += "\n    " + x;
        problem("one description, " + std::to_string(m.size()) + " meanings: '" + example[key] +
                "'" + all);
    }
    for (const auto& p : problems) ADD_FAILURE() << p;
    std::cout << "[ prompt census: " << games << " games, " << decisions << " decisions, " << prompts
              << " card or engine prompts, " << meanings.size() << " distinct questions ]\n[ kinds:";
    for (const auto& [k, n] : by_kind) std::cout << " " << toString(k) << "=" << n;
    std::cout << " ]\n";
    EXPECT_GT(decisions, 100L * games);
}

}  // namespace riftbound::test::prompt_description
