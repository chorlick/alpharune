#pragma once
/// @file binary_data_serializer.h
/// Binary training-data writer.
///
/// Same recording surface as DataSerializer (recordDecision +
/// recordGameSummary), but writes a compact binary file containing
/// pre-extracted ML features instead of full state JSON. Eliminates the
/// Python Pass-2 parse+featurize cost — the trainer can mmap binary files
/// and feed them directly to the model.
///
/// File layout (single game per file):
///   Header (20 bytes):
///     char[4]  magic       = "RFBA"
///     uint16   version     = 1
///     uint16   state_dim   (raw extracted size, e.g. 354)
///     uint16   action_dim  (25)
///     uint16   max_actions (64)
///     uint32   num_decisions  (patched on close)
///     uint8    winner      (0=P1, 1=P2, 2=draw; patched on close)
///     int8     final_score_p1  (0..8; patched on close, 0 = unknown for old files)
///     int8     final_score_p2  (0..8; patched on close, 0 = unknown for old files)
///     uint8    _pad[1]
///   Score fields enable score-differential REINFORCE rewards; pre-change
///   binaries leave them as 0 and the Python reader falls back to ±1 win/loss.
///
///   Decision record × num_decisions
///   (size = 4 + state_dim*4 + max_actions*action_dim*4 + max_actions bytes):
///     uint8    perspective  (0=P1, 1=P2)
///     uint8    num_legal    (clamped to max_actions)
///     uint16   chosen_idx   (clamped to [0, num_legal-1])
///     float32[state_dim]                  state features (perspective-aware)
///     float32[max_actions][action_dim]    action features (unused slots zero)
///     uint8[max_actions]                  action mask (0/1)

#include "core/card_db.h"
#include "core/game_state.h"
#include "core/intent.h"
#include "io/data_serializer.h"
#include "ml/feature_extractor.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace riftbound {

class BinaryDataSerializer : public IDataSerializer {
public:
    BinaryDataSerializer(const CardDB& db, const std::string& output_path);
    ~BinaryDataSerializer() override;

    /// Record a decision point: features extracted from acting player's
    /// perspective, all legal actions featurized, chosen index recorded.
    void recordDecision(const GameState& state,
                        const std::vector<Intent>& legal_actions,
                        const Intent& chosen_action) override;

    /// Patch header with final num_decisions + winner and flush.
    void recordGameSummary(const GameState& state,
                           const std::string& game_id) override;

    void flush() override;

private:
    static constexpr char     kMagic[4]     = {'R', 'F', 'B', 'A'};
    static constexpr uint16_t kVersion      = 1;
    static constexpr uint16_t kStateDim     = ml::kStateFeatureDim;
    static constexpr uint16_t kActionDim    = ml::kActionFeatureDim;
    static constexpr uint16_t kMaxActions   = ml::kMaxLegalActions;

    const CardDB& db_;
    std::ofstream file_;
    std::string path_;
    uint32_t num_decisions_ = 0;
    uint8_t winner_byte_ = 2;       // default to draw until summary patches it
    bool header_written_ = false;

    void writeHeader();
};

} // namespace riftbound
