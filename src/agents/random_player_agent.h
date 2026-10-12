#pragma once
/// @file random_player_agent.h
/// `random-player`: an agent that plays. It chooses uniformly at random among
/// the legal actions that are NOT a pass; when there is something else to do
/// it passes with probability P (`random-player:pass=P`), and it always
/// passes when a pass is all there is. **The default is pass=0.2**
/// (`random-player` alone, kDefaultPassProbability); `random-player:pass=0`
/// is the forced-play variant that never passes while anything else is
/// legal. A run can so sit anywhere between forced play and the plain random
/// agent's habit of passing far more than it acts.
///
/// Seeded like RandomAgent: the same seed and the same game give the same
/// choices.
///
/// What counts as a pass is decided from the Intent's own fields and the list
/// it is offered in, never from a label (isPassOption below lists every case).

#include "agent_interface.h"

#include <algorithm>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace riftbound {

/// Whether `legal[index]` is a do-nothing choice of the decision `legal`.
///
/// By intent type:
///   - PassPriority, PassFocus, EndTurn: the three passes of the turn
///     structure.
///   - DeclineOptionalTrigger, DeclineTriggeredCost: declines by type.
///   - MulliganDecision that sets no card aside: "keep".
///
/// A MakeChoice (a prompt) option is a pass when it names nothing — no
/// object, no card — and carries the value 0, and the prompt is one of:
///   - a prompt that also offers an object or a named card. There the bare
///     value 0 is the option that takes none of them: "None" (selectObject /
///     selectSeveral with may_decline), "No more targets" (splitDamageAmong),
///     "Move no more units" (Standard Move), "Keep it" / "Leave it" (Predict,
///     look-and-draw).
///   - a prompt of exactly the two values 0 and 1: no / yes (confirmOptional,
///     EffectExecutor::confirmNow, a [Repeat] cost, a triggered cost).
///
/// Known imprecision of the second rule, which the Intent's fields cannot
/// resolve: a two-option value prompt that is not a yes/no also has the
/// values 0 and 1 — a modal choice between two modes (Card::pickMode), an X
/// of 0 or 1 (Card::pickXAmount), "cost [1] less" / "cost [A] less", "pay
/// [A]" / "pay Energy" for a Hide cost, Void Hatchling's leave / recycle. At
/// pass=0 this agent therefore never takes option 0 of those; at the default
/// pass=0.2 it takes it one time in five.
inline bool isPassOption(const std::vector<Intent>& legal, std::size_t index) {
    const Intent& i = legal[index];
    switch (i.type) {
        case IntentType::PassPriority:
        case IntentType::PassFocus:
        case IntentType::EndTurn:
        case IntentType::DeclineOptionalTrigger:
        case IntentType::DeclineTriggeredCost:
            return true;
        case IntentType::MulliganDecision:
            return i.cards_to_mulligan.empty();
        case IntentType::MakeChoice:
            break;
        default:
            return false;
    }
    auto names_something = [](const Intent& o) {
        return !o.chosen_objects.empty() || o.chosen_card_def != kInvalidId;
    };
    if (names_something(i) || !i.chosen_value.has_value() || *i.chosen_value != 0) return false;
    bool other_names = false, all_prompt = true, has_one = false;
    for (const Intent& o : legal) {
        if (o.type != IntentType::MakeChoice) { all_prompt = false; continue; }
        if (names_something(o)) other_names = true;
        else if (o.chosen_value.has_value() && *o.chosen_value == 1) has_one = true;
    }
    if (other_names) return true;
    return all_prompt && legal.size() == 2 && has_one;
}

/// What `random-player` without options means.
inline constexpr double kDefaultPassProbability = 0.2;

class RandomPlayerAgent : public AgentInterface {
public:
    /// `pass_probability`: how often it passes when it could act (0 = never).
    explicit RandomPlayerAgent(uint64_t seed = 0,
                               double pass_probability = kDefaultPassProbability)
        : rng_(seed == 0 ? std::random_device{}() : seed), pass_(pass_probability) {
        if (!(pass_ >= 0.0 && pass_ <= 1.0))
            throw std::invalid_argument("random-player: pass must be between 0 and 1");
    }

    Intent selectAction(const GameState& state,
                        const std::vector<Intent>& legal_actions) override {
        if (legal_actions.empty()) return Intent::concede(state.turn.turn_player);
        acting_.clear();
        passing_.clear();
        for (std::size_t k = 0; k < legal_actions.size(); ++k)
            (isPassOption(legal_actions, k) ? passing_ : acting_).push_back(k);
        const std::vector<std::size_t>* from = &acting_;
        if (acting_.empty()) {
            from = &passing_;
        } else if (!passing_.empty() && pass_ > 0.0 &&
                   std::uniform_real_distribution<double>(0.0, 1.0)(rng_) < pass_) {
            from = &passing_;
        }
        std::uniform_int_distribution<std::size_t> dist(0, from->size() - 1);
        return legal_actions[(*from)[dist(rng_)]];
    }

private:
    std::mt19937_64 rng_;
    double pass_;
    std::vector<std::size_t> acting_, passing_;
};

/// Parses the options of a `random-player` spec (what follows the colon:
/// "pass=0.25", or nothing). Throws std::runtime_error naming what is wrong.
inline double parseRandomPlayerOptions(const std::string& options) {
    if (options.empty()) return kDefaultPassProbability;
    const std::string key = "pass=";
    if (options.compare(0, key.size(), key) != 0)
        throw std::runtime_error("random-player: unrecognised option '" + options +
                                 "'. Expected pass=P with P between 0 and 1.");
    const std::string val = options.substr(key.size());
    std::size_t used = 0;
    double p = -1;
    try { p = std::stod(val, &used); } catch (const std::exception&) { used = 0; }
    if (used == 0 || used != val.size() || !(p >= 0.0 && p <= 1.0))
        throw std::runtime_error("random-player: pass must be a number between 0 and 1, got '" +
                                 val + "'.");
    return p;
}

/// "random-player" or "random-player:<options>"; false for any other spec.
inline bool isRandomPlayerSpec(const std::string& spec, double* pass = nullptr) {
    const std::string kind = "random-player";
    if (spec.compare(0, kind.size(), kind) != 0) return false;
    if (spec.size() == kind.size()) {
        if (pass) *pass = kDefaultPassProbability;
        return true;
    }
    if (spec[kind.size()] != ':') return false;
    const double p = parseRandomPlayerOptions(spec.substr(kind.size() + 1));
    if (pass) *pass = p;
    return true;
}

} // namespace riftbound
