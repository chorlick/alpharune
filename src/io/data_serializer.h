#pragma once
/// @file data_serializer.h
/// JSON-lines output for ML training corpus.
///
/// Captures game state snapshots at every decision point, writing
/// one JSON object per line to a .jsonl file. Each snapshot includes
/// the full observable state, legal actions, and chosen action.

#include "core/card_db.h"
#include "core/events.h"
#include "core/game_state.h"
#include "core/intent.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace riftbound {

class DataSerializer {
public:
    DataSerializer(const CardDB& db, const std::string& output_path);
    ~DataSerializer();

    /// Record a decision point: state + legal actions + chosen action.
    void recordDecision(const GameState& state,
                        const std::vector<Intent>& legal_actions,
                        const Intent& chosen_action);

    /// Record game summary at end of game.
    void recordGameSummary(const GameState& state,
                           const std::string& game_id);

    /// Flush output.
    void flush();

private:
    const CardDB& db_;
    std::ofstream file_;

    nlohmann::json serializeState(const GameState& state) const;
    nlohmann::json serializePlayer(const GameState& state, PlayerId player) const;
    nlohmann::json serializeBattlefield(const GameState& state,
                                         const BattlefieldState& bf) const;
    nlohmann::json serializeObject(const GameState& state,
                                    GameObjectId id) const;
    nlohmann::json serializeIntent(const Intent& intent) const;
    nlohmann::json serializeActions(const std::vector<Intent>& actions) const;
};

} // namespace riftbound
