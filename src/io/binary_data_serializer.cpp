#include "binary_data_serializer.h"

#include "ml/feature_extractor.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace riftbound {

constexpr char     BinaryDataSerializer::kMagic[4];
constexpr uint16_t BinaryDataSerializer::kVersion;
constexpr uint16_t BinaryDataSerializer::kStateDim;
constexpr uint16_t BinaryDataSerializer::kActionDim;
constexpr uint16_t BinaryDataSerializer::kMaxActions;

BinaryDataSerializer::BinaryDataSerializer(const CardDB& db,
                                            const std::string& output_path)
    : db_(db),
      file_(output_path, std::ios::out | std::ios::binary | std::ios::trunc),
      path_(output_path) {
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open binary output file: " + output_path);
    }
    writeHeader();
}

BinaryDataSerializer::~BinaryDataSerializer() {
    if (file_.is_open()) file_.close();
}

void BinaryDataSerializer::flush() {
    file_.flush();
}

void BinaryDataSerializer::writeHeader() {
    // Layout: 4 + 2 + 2 + 2 + 2 + 4 + 1 + 3 = 20 bytes
    file_.write(kMagic, 4);
    file_.write(reinterpret_cast<const char*>(&kVersion),    sizeof(kVersion));
    file_.write(reinterpret_cast<const char*>(&kStateDim),   sizeof(kStateDim));
    file_.write(reinterpret_cast<const char*>(&kActionDim),  sizeof(kActionDim));
    file_.write(reinterpret_cast<const char*>(&kMaxActions), sizeof(kMaxActions));
    file_.write(reinterpret_cast<const char*>(&num_decisions_), sizeof(num_decisions_));
    file_.write(reinterpret_cast<const char*>(&winner_byte_),   sizeof(winner_byte_));
    const uint8_t pad[3] = {0, 0, 0};
    file_.write(reinterpret_cast<const char*>(pad), sizeof(pad));
    header_written_ = true;
}

// ─── Decision recording ──────────────────────────────────────────────────────

void BinaryDataSerializer::recordDecision(const GameState& state,
                                          const std::vector<Intent>& legal_actions,
                                          const Intent& chosen_action) {
    // Perspective = turn_player. This matches what scripts/train_agent.py
    // does (it reads `turn_player` from the JSONL record), so binary records
    // are drop-in replacements for the JSONL → memmap pipeline. Note this
    // differs from ModelAgent inference, which uses legal[0].player — a
    // pre-existing pipeline inconsistency we preserve here for now.
    PlayerId perspective = state.turn.turn_player;
    uint8_t perspective_byte = (perspective == PlayerId::Player1) ? 0 : 1;

    int num_legal = std::min(static_cast<int>(legal_actions.size()),
                              static_cast<int>(kMaxActions));
    uint8_t num_legal_byte = static_cast<uint8_t>(num_legal);

    // Find chosen action index within the (possibly truncated) legal list.
    // Use full Intent equality — a partial-field match (type/card/targets/
    // ability_source only) collapses "twin" intents and biases chosen_idx
    // toward 0, corrupting training labels and the REINFORCE signal.
    int chosen_idx = 0;
    for (int i = 0; i < num_legal; ++i) {
        if (legal_actions[i] == chosen_action) {
            chosen_idx = i;
            break;
        }
    }
    uint16_t chosen_idx_u16 = static_cast<uint16_t>(
        std::clamp(chosen_idx, 0, num_legal - 1 >= 0 ? num_legal - 1 : 0));

    // Preamble: perspective, num_legal, chosen_idx
    file_.write(reinterpret_cast<const char*>(&perspective_byte),
                sizeof(perspective_byte));
    file_.write(reinterpret_cast<const char*>(&num_legal_byte),
                sizeof(num_legal_byte));
    file_.write(reinterpret_cast<const char*>(&chosen_idx_u16),
                sizeof(chosen_idx_u16));

    // State features (kStateDim float32)
    auto state_feats = ml::extractStateFeatures(state, perspective, db_);
    // Defensive: extractor should produce exactly kStateDim, but pad/truncate
    // to be safe.
    state_feats.resize(kStateDim, 0.0f);
    file_.write(reinterpret_cast<const char*>(state_feats.data()),
                sizeof(float) * kStateDim);

    // Action features (kMaxActions × kActionDim float32)
    std::vector<float> action_block(
        static_cast<size_t>(kMaxActions) * kActionDim, 0.0f);
    std::vector<uint8_t> mask(kMaxActions, 0);
    for (int i = 0; i < num_legal; ++i) {
        auto af = ml::featurizeAction(legal_actions[i], state);
        af.resize(kActionDim, 0.0f);
        std::memcpy(&action_block[i * kActionDim],
                    af.data(), sizeof(float) * kActionDim);
        mask[i] = 1;
    }
    file_.write(reinterpret_cast<const char*>(action_block.data()),
                sizeof(float) * action_block.size());
    file_.write(reinterpret_cast<const char*>(mask.data()),
                sizeof(uint8_t) * mask.size());

    ++num_decisions_;
}

void BinaryDataSerializer::recordGameSummary(const GameState& state,
                                              const std::string& /*game_id*/) {
    // Compute winner byte (0=P1, 1=P2, 2=draw)
    if (state.winner == PlayerId::Player1)      winner_byte_ = 0;
    else if (state.winner == PlayerId::Player2) winner_byte_ = 1;
    else                                         winner_byte_ = 2;

    // Final scores for score-differential REINFORCE reward. Stored in the
    // header's previously-pad region (no version bump needed; pre-change
    // binaries have these as 0 and the Python reader falls back to win/loss).
    // Both scores are clamped to int8 [0, 127] — game scores are 0..8 in
    // practice so we never overflow.
    int8_t final_score_p1 = static_cast<int8_t>(
        std::clamp(state.players[0].score, 0, 127));
    int8_t final_score_p2 = static_cast<int8_t>(
        std::clamp(state.players[1].score, 0, 127));

    // Patch header in place: seek to num_decisions field at offset 12.
    // Layout: magic(4) + version(2) + state_dim(2) + action_dim(2) + max_actions(2) = 12
    //         num_decisions(4) + winner(1) + score_p1(1) + score_p2(1) + pad(1) = 20
    flush();
    file_.seekp(12);
    file_.write(reinterpret_cast<const char*>(&num_decisions_),
                sizeof(num_decisions_));
    file_.write(reinterpret_cast<const char*>(&winner_byte_),
                sizeof(winner_byte_));
    file_.write(reinterpret_cast<const char*>(&final_score_p1),
                sizeof(final_score_p1));
    file_.write(reinterpret_cast<const char*>(&final_score_p2),
                sizeof(final_score_p2));
    flush();
}

} // namespace riftbound
