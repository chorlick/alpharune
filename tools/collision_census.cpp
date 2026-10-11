/// @file collision_census.cpp
/// Action-slot collision census (v6 task P2.1).
///
/// Plays random games and, at every decision, compares the engine's legal
/// intents with the action ids they get (openspiel::actionIds). Two distinct
/// intents sharing an id is a collision; an intent with no id (more than
/// kTwinPlanes legal intents with one description) is counted as unreachable.
/// Since the 2026-10 checkpoint break both counts are zero by construction and
/// tests/test_action_ids.cpp guards them; the tool remains for other decks.
/// Also reports how many decisions offer one option.
///
///   collision_census --deck1 A.txt --deck2 B.txt [--games 100] [--seed 1] [--json out.json]

#include "agents/random_agent.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "openspiel/action_vocab.h"
#include "rules/deck_validator.h"

#include <nlohmann/json.hpp>

#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

using namespace riftbound;

namespace {

const char* intentName(IntentType t) {
    switch (t) {
        case IntentType::PlayCard:                return "PlayCard";
        case IntentType::StandardMove:            return "StandardMove";
        case IntentType::ActivateAbility:         return "ActivateAbility";
        case IntentType::HideCard:                return "HideCard";
        case IntentType::EndTurn:                 return "EndTurn";
        case IntentType::PlayReaction:            return "PlayReaction";
        case IntentType::ActivateReactionAbility: return "ActivateReactionAbility";
        case IntentType::PassPriority:            return "PassPriority";
        case IntentType::PlayActionCard:          return "PlayActionCard";
        case IntentType::ActivateActionAbility:   return "ActivateActionAbility";
        case IntentType::PassFocus:               return "PassFocus";
        case IntentType::AssignCombatDamage:      return "AssignCombatDamage";
        case IntentType::MulliganDecision:        return "MulliganDecision";
        case IntentType::ChooseBattlefield:       return "ChooseBattlefield";
        case IntentType::MakeChoice:              return "MakeChoice";
        case IntentType::SideboardSwap:           return "SideboardSwap";
        case IntentType::PlayFirstDecision:       return "PlayFirstDecision";
        case IntentType::PlaceOptionalTrigger:    return "PlaceOptionalTrigger";
        case IntentType::DeclineOptionalTrigger:  return "DeclineOptionalTrigger";
        case IntentType::PayTriggeredCost:        return "PayTriggeredCost";
        case IntentType::DeclineTriggeredCost:    return "DeclineTriggeredCost";
        case IntentType::Concede:                 return "Concede";
    }
    return "?";
}

struct Census {
    long games = 0;
    long decisions = 0;
    long single_option = 0;          // decisions with exactly one distinct intent
    long intents = 0;                // distinct legal intents, summed over decisions
    long slots = 0;                  // distinct slots, summed over decisions
    long colliding_decisions = 0;    // decisions where intents > slots
    long unencodable = 0;            // intents with no slot (encodeAction < 0)
    // Per intent type: distinct intents offered, and how many of them were
    // unreachable (shared a slot with an earlier intent).
    std::map<std::string, std::array<long, 2>> by_type;
    // Among decisions with a real choice (2+ distinct intents).
    long choice_decisions = 0;
    long choice_colliding = 0;
};

} // namespace

int main(int argc, char* argv[]) {
    std::string deck1_path, deck2_path, json_path;
    int games = 100;
    uint64_t seed = 1;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string flag = argv[i];
        if (flag == "--deck1") deck1_path = argv[i + 1];
        else if (flag == "--deck2") deck2_path = argv[i + 1];
        else if (flag == "--games") games = std::stoi(argv[i + 1]);
        else if (flag == "--seed") seed = std::stoull(argv[i + 1]);
        else if (flag == "--json") json_path = argv[i + 1];
        else { std::cerr << "Unknown argument " << flag << "\n"; return 2; }
    }
    if (deck1_path.empty()) {
        std::cerr << "usage: collision_census --deck1 A.txt [--deck2 B.txt] "
                     "[--games N] [--seed N] [--json out.json]\n";
        return 2;
    }
    if (deck2_path.empty()) deck2_path = deck1_path;

    CardRegistry registry;
    registry.loadAll();
    CardDB db;
    db.buildFromClasses(registry);
    const DeckSubmission deck1 = DeckValidator::loadFromDeckList(deck1_path, db);
    const DeckSubmission deck2 = DeckValidator::loadFromDeckList(deck2_path, db);

    Census c;
    for (int g = 0; g < games; ++g) {
        const uint64_t game_seed = ((seed + g) & 0x7FFFFFFFu) | 1u;
        std::mt19937_64 rng(game_seed * 7919);
        EventBus events;
        GameEngine engine(db, events, registry);
        StepResult step = engine.beginGame(deck1, deck2, game_seed);
        while (step.kind == StepKind::NeedDecision) {
            const auto& state = engine.state();

            // Distinct intents (the engine may list exact duplicates).
            std::vector<const Intent*> distinct;
            for (const auto& intent : step.legal) {
                bool seen = false;
                for (const Intent* d : distinct) {
                    if (*d == intent) { seen = true; break; }
                }
                if (!seen) distinct.push_back(&intent);
            }

            std::vector<Intent> distinct_list;
            for (const Intent* intent : distinct) distinct_list.push_back(*intent);
            const std::vector<int> ids = openspiel::actionIds(distinct_list, state);
            std::unordered_map<int, int> slot_uses;
            int lost = 0;
            for (size_t k = 0; k < distinct.size(); ++k) {
                const Intent* intent = distinct[k];
                auto& counts = c.by_type[intentName(intent->type)];
                ++counts[0];
                const int slot = ids[k];
                if (slot < 0) {
                    ++c.unencodable;
                    ++counts[1];
                    ++lost;
                    continue;
                }
                if (slot_uses[slot]++ > 0) {
                    ++counts[1];
                    ++lost;
                }
            }

            ++c.decisions;
            c.intents += static_cast<long>(distinct.size());
            c.slots += static_cast<long>(slot_uses.size());
            if (distinct.size() == 1) ++c.single_option;
            if (lost > 0) ++c.colliding_decisions;
            if (distinct.size() >= 2) {
                ++c.choice_decisions;
                if (lost > 0) ++c.choice_colliding;
            }

            std::uniform_int_distribution<size_t> pick(0, step.legal.size() - 1);
            step = engine.applyChoice(static_cast<int>(pick(rng)));
        }
        ++c.games;
    }

    auto pct = [](long a, long b) { return b == 0 ? 0.0 : 100.0 * a / b; };
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "games " << c.games << ", decisions " << c.decisions
              << " (" << (c.games ? c.decisions / c.games : 0) << " per game)\n";
    std::cout << "single-option decisions: " << c.single_option << " ("
              << pct(c.single_option, c.decisions) << "%)\n";
    std::cout << "decisions with a real choice: " << c.choice_decisions
              << ", of which with a collision: " << c.choice_colliding << " ("
              << pct(c.choice_colliding, c.choice_decisions) << "%)\n";
    std::cout << "distinct intents " << c.intents << ", distinct slots " << c.slots
              << ", unreachable intents " << (c.intents - c.slots) << " ("
              << pct(c.intents - c.slots, c.intents) << "%)\n";
    std::cout << "intents with no slot: " << c.unencodable << "\n";
    std::cout << "\nby intent type (offered, unreachable):\n";
    for (const auto& [name, n] : c.by_type) {
        std::cout << "  " << std::left << std::setw(26) << name << std::right
                  << std::setw(9) << n[0] << std::setw(9) << n[1]
                  << std::setw(7) << pct(n[1], n[0]) << "%\n";
    }

    if (!json_path.empty()) {
        nlohmann::json by_type = nlohmann::json::object();
        for (const auto& [name, n] : c.by_type)
            by_type[name] = {{"offered", n[0]}, {"unreachable", n[1]}};
        nlohmann::json j = {
            {"deck1", deck1_path}, {"deck2", deck2_path}, {"seed", seed},
            {"games", c.games}, {"decisions", c.decisions},
            {"single_option_decisions", c.single_option},
            {"choice_decisions", c.choice_decisions},
            {"choice_decisions_with_collision", c.choice_colliding},
            {"distinct_intents", c.intents}, {"distinct_slots", c.slots},
            {"intents_without_slot", c.unencodable},
            {"by_intent_type", by_type},
        };
        std::ofstream(json_path) << j.dump(2) << "\n";
    }
    return 0;
}
