#pragma once
/// Agents shared by the batch M1 tests (tests/cards/test_batch_m1_*.cpp):
/// whether a choice is made at finalization is read off when the controller
/// is asked compared with when the opponent first gets priority.

#include "card_test_fixture.h"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace riftbound::test::batch_m1 {

/// P2: passes, counts the priority / Focus decisions it is given, and — once,
/// at the first of them with something on the chain — may do `respond` to the
/// live engine (bounce a target, move it, pump it), as a response would.
class Them : public EnginePathAgent {
public:
    int priority = 0;
    GameEngine* engine = nullptr;
    std::function<void(GameEngine&)> respond;
    bool responded = false;
    /// Called with the state at P2's first priority (before `respond`).
    std::function<void(const GameState&)> look;
    bool looked = false;
    using EnginePathAgent::EnginePathAgent;
    Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) {
                ++priority;
                if (look && !looked) {
                    looked = true;
                    look(st);
                }
                if (respond && !responded && engine && !st.chain.items.empty()) {
                    responded = true;
                    respond(*engine);
                }
                break;
            }
        return EnginePathAgent::selectAction(st, legal);
    }
};

/// P1: answers prompts as an EnginePathAgent with `pick`, and notes for each
/// prompt how many priority decisions P2 had been given before it.
class Me : public EnginePathAgent {
public:
    const Them* other = nullptr;
    std::vector<int> other_priority_at;
    using EnginePathAgent::EnginePathAgent;
    Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
        const bool is_choice = !legal.empty() && std::all_of(legal.begin(), legal.end(),
            [](const Intent& i) { return i.type == IntentType::MakeChoice; });
        if (is_choice && !isTriggerOrderPrompt(legal) && !isBeginPrompt(legal))
            other_priority_at.push_back(other ? other->priority : 0);
        return EnginePathAgent::selectAction(st, legal);
    }
    /// The first prompt offering object `id`, or -1.
    int promptOffering(GameObjectId id) const {
        for (size_t k = 0; k < prompts.size(); ++k)
            for (const auto& i : prompts[k])
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == id)
                    return static_cast<int>(k);
        return -1;
    }
    /// The first prompt with an option whose label contains `text`, or -1.
    int promptWith(const std::string& text) const {
        for (size_t k = 0; k < prompts.size(); ++k)
            for (const auto& i : prompts[k])
                if (i.choice_label.find(text) != std::string::npos) return static_cast<int>(k);
        return -1;
    }
    /// How many prompts came after P2 first had priority.
    int promptsAfterPriority() const {
        return static_cast<int>(std::count_if(other_priority_at.begin(), other_priority_at.end(),
                                              [](int n) { return n > 0; }));
    }
};

inline int raw(GameObjectId id) { return static_cast<int>(id.raw()); }

}  // namespace riftbound::test::batch_m1
