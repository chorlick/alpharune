#pragma once
// V2 tensor batch — Phase C-2A authoritative type.
// Filled by extractors in src/ml/v2_*.{h,cpp}.
// Consumed by BinaryDataSerializerV2 (src/io/) and BatchRunner::emitTensorBatch (src/engine/).
//
// Field types are deliberately LibTorch-free so this header compiles with
// OPEN_SPIEL_BUILD_WITH_LIBTORCH=OFF. C-2B will add a torch::Tensor adapter
// that wraps a const V2TensorBatch&.

#include <array>
#include <cstdint>
#include <vector>

namespace riftbound {
class GameState;
struct Intent;
class CardDB;
enum class PlayerId : uint8_t;
}  // namespace riftbound

namespace riftbound::ml {

inline constexpr int kV2MaxEntities      = 128;
inline constexpr int kV2MaxLegalActions  = 64;
inline constexpr int kV2NumActionTypes   = 22;  // §6: matches n_action_types (engine has 14; 8 reserved)
inline constexpr int kV2NumSpatialNodes  = 6;   // Left Base, Right Base, BF1..BF4
inline constexpr int kV2NumDestSlots     = 8;   // n_spatial_nodes + 2 base slots
inline constexpr int kV2CardVocab        = 787;

// V2 per-decision tensor batch.
// Parallel arrays of length num_entities are filled by the extractor functions
// declared below. Fixed-size std::array fields are mask buffers.
struct V2TensorBatch {
    // --- Per-decision scalar header ---
    uint8_t  perspective    = 0;          // 0=P1, 1=P2
    uint16_t chain_depth    = 0;
    uint16_t num_entities   = 0;
    uint16_t num_legal      = 0;

    // --- Entity arrays (parallel, length = num_entities) ---
    std::vector<int32_t> card_def_id;
    std::vector<int32_t> zone_id;
    std::vector<int32_t> domain_id;
    std::vector<int32_t> stance_id;
    std::vector<uint8_t> perspective_flag;
    std::vector<int32_t> spatial_node_mapping;  // -1 sentinel for off-board
    std::vector<int32_t> chain_index;           // -1 sentinel for off-chain
    std::vector<int32_t> might_bucket;
    std::vector<int32_t> damage_bucket;
    std::vector<int32_t> score_bucket;
    std::vector<int32_t> xp_bucket;
    std::vector<int32_t> runes_ready_bucket;
    std::vector<int32_t> runes_exhausted_bucket;
    std::vector<int32_t> buff_count;
    std::vector<int32_t> temp_buff_count;
    std::vector<int32_t> shield_value;
    std::vector<int32_t> assault_value;
    std::vector<int32_t> deflect_value;
    std::vector<uint64_t> game_object_id;
    std::vector<uint8_t> entity_attention_mask;
    std::vector<uint8_t> chain_token_mask;

    // --- Factored legal masks (the 8 named padding masks) ---
    std::array<uint8_t, kV2NumActionTypes> action_type_mask = {};
    std::vector<uint8_t> source_pointer_mask;
    std::vector<uint8_t> target1_pointer_mask;
    std::vector<uint8_t> target2_pointer_mask;
    std::array<uint8_t, kV2NumDestSlots>  destination_pointer_mask = {};
    std::vector<uint8_t> ability_source_pointer_mask;

    // --- Factored chosen-action labels (TOKEN INDICES, not card_def_id) ---
    int32_t chosen_action_type                = -1;
    int32_t chosen_source_token_index         = -1;
    int32_t chosen_target1_token_index        = -1;
    int32_t chosen_target2_token_index        = -1;
    int32_t chosen_destination_node           = -1;
    int32_t chosen_ability_source_token_index = -1;

    // --- Aux labels ---
    uint8_t aux_value_label = 2;  // 0=P1 win, 1=P2 win, 2=draw
    std::array<uint16_t, kV2CardVocab> aux_hidden_hand_count = {};
    std::array<uint16_t, kV2CardVocab> observed_cards_self   = {};
    std::array<uint16_t, kV2CardVocab> observed_cards_opp    = {};
};

// --- Free-function API filled by separate PRs (declarations only here) ---

// PR2: per-instance entity tokenization. Fills entity arrays.
void extractEntityTokens(const ::riftbound::GameState& state,
                         ::riftbound::PlayerId perspective,
                         const ::riftbound::CardDB& card_db,
                         V2TensorBatch& out);

// PR3: spatial_node_mapping (-1 sentinel) and chain_indices (FEPR order).
void extractSpatialAndChainIndices(const ::riftbound::GameState& state,
                                   V2TensorBatch& out);

// PR4: factored legal masks from a list of legal Intents.
void decomposeToFactoredMasks(const ::riftbound::GameState& state,
                              const std::vector<::riftbound::Intent>& legal_actions,
                              V2TensorBatch& out);

// PR5: factored chosen-action labels (token-index references, NOT card_def_id).
void extractChosenActionFactors(const ::riftbound::GameState& state,
                                const ::riftbound::Intent& chosen,
                                V2TensorBatch& out);

}  // namespace riftbound::ml
