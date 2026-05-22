#pragma once
/// @file v2_model.h
/// V2 model — entity-token transformer + spatial fusion + pointer heads.
///
/// Implements the architecture in docs/potential-model-architecture.txt
/// with the engineering-review simplification of cross-attention with
/// additive edge-type biases instead of a formal GNN (mathematically
/// equivalent over the 6-8 node spatial grid; saves a library
/// dependency).
///
/// Inputs (batched, one batch element per game state):
///   card_ids       : LongTensor (B, N)         — CardDefId per token, 0 = pad/unknown
///   zone_ids       : LongTensor (B, N)
///   domain_ids     : LongTensor (B, N)
///   stance_ids     : LongTensor (B, N)
///   stats          : FloatTensor (B, N, kEntityStatsDim) — raw integer stats; the
///                    model passes them through small per-token projections
///                    (the design doc prefers bucket embeddings but raw +
///                    projection works equivalently and is simpler from C++).
///   is_perspective : LongTensor (B, N)         — 0/1
///   chain_index    : LongTensor (B, N)         — -1 if not chain
///   spatial_node   : LongTensor (B, N)         — -1 if off-board
///   token_mask     : BoolTensor  (B, N)        — 1 = valid, 0 = pad
///
/// Outputs:
///   action_type_logits  : (B, num_action_types)
///   source_logits       : (B, N)        — pointer over tokens (legal-mask in caller)
///   target_logits       : (B, N)        — pointer over tokens
///   dest_node_logits    : (B, kSpatialNodeCount)  — pointer over spatial nodes
///   flat_policy_logits  : (B, num_action_slots) — flat-vocab head, the
///                          actual AlphaZero training target until the
///                          pointer-head decomposition + auxiliary
///                          losses land. Computed from pooled encoder
///                          state via a single Linear projection — the
///                          trunk still trains end-to-end via this head.
///   value               : (B, 1)        — tanh in [-1, +1]
///
/// V1 model.{h,cpp} stays in tree until V1 callers are migrated; V2 is
/// the path forward for all new training/inference work.

#include "ml/entity_tokens.h"

#include <torch/torch.h>

#include <cstdint>
#include <vector>

namespace riftbound::ml {

struct V2ModelConfig {
    /// Vocab / shape constants — pulled from action_vocab + entity_tokens.
    int num_card_defs   = 787 + 1;   // +1 for "unknown/pad" at index 0
    int num_zones       = kEntityZoneCount;
    int num_domains     = static_cast<int>(::riftbound::Domain::Count) + 1;
    int num_stances     = kEntityStanceCount;
    int num_spatial     = kSpatialNodeCount;
    int max_entities    = kMaxEntityTokens;
    int stats_dim       = kEntityStatsDim;
    int num_action_types = 22;     // IntentType count
    int max_chain_index = 8;       // chain depth bound for sinusoidal pos enc

    /// Embedding dims (per the doc; review notes recommend smaller for
    /// our data scale but exact-spec choice was made).
    int card_embed_dim   = 256;
    int zone_embed_dim   = 64;
    int domain_embed_dim = 64;
    int stance_embed_dim = 64;
    int stats_proj_dim   = 64;
    int perspective_dim  = 8;
    int spatial_dim      = 32;

    /// Transformer encoder.
    int d_model         = 512;
    int encoder_layers  = 6;
    int encoder_heads   = 8;
    int encoder_ffn_dim = 2048;
    double encoder_dropout = 0.1;

    /// Spatial cross-attention block (replaces formal GNN per review).
    int spatial_attn_layers = 2;
    int spatial_attn_heads  = 4;
    int num_edge_types      = 3;   // adjacent_move, combat_range, base_link

    /// Output head widths.
    int policy_hidden = 256;
    int value_hidden  = 128;

    /// Size of the flat policy vocab (action_vocab.h kVocabSize). The
    /// driver MUST override this from the live OpenSpiel game's
    /// NumDistinctActions() so the head matches the runtime vocab. The
    /// default below is a tripwire/floor that the smoke test compares
    /// against; production code shouldn't rely on it.
    int num_action_slots = 7141;
};

class V2ModelImpl : public torch::nn::Module {
public:
    explicit V2ModelImpl(const V2ModelConfig& cfg = {});

    /// Output struct so caller doesn't have to remember tuple ordering.
    struct Output {
        torch::Tensor action_type_logits;   // (B, num_action_types)
        torch::Tensor source_logits;        // (B, N)
        torch::Tensor target_logits;        // (B, N)
        torch::Tensor dest_node_logits;     // (B, num_spatial)
        torch::Tensor flat_policy_logits;   // (B, num_action_slots)
        torch::Tensor value;                // (B, 1)
    };

    Output forward(
        torch::Tensor card_ids,        // (B, N) long
        torch::Tensor zone_ids,        // (B, N) long
        torch::Tensor domain_ids,      // (B, N) long
        torch::Tensor stance_ids,      // (B, N) long
        torch::Tensor stats,           // (B, N, stats_dim) float
        torch::Tensor is_perspective,  // (B, N) long
        torch::Tensor chain_index,     // (B, N) long  (-1 for non-chain)
        torch::Tensor spatial_node,    // (B, N) long  (-1 for off-board)
        torch::Tensor token_mask);     // (B, N) bool

    const V2ModelConfig& config() const { return cfg_; }

private:
    V2ModelConfig cfg_;

    // Embedding tables.
    torch::nn::Embedding card_embed_{nullptr};
    torch::nn::Embedding zone_embed_{nullptr};
    torch::nn::Embedding domain_embed_{nullptr};
    torch::nn::Embedding stance_embed_{nullptr};
    torch::nn::Embedding perspective_embed_{nullptr};
    // Spatial-node embedding (per-token). Index 0 reserved for "off-board"
    // (i.e., the kSpatialNodeOffBoard sentinel of -1, shifted by +1).
    torch::nn::Embedding spatial_embed_{nullptr};

    // Continuous stats → per-token MLP projection.
    torch::nn::Linear   stats_proj_{nullptr};

    // Token assembly: concat embeddings + LayerNorm + Linear → d_model.
    torch::nn::LayerNorm token_ln_{nullptr};
    torch::nn::Linear    token_proj_{nullptr};

    // Sinusoidal positional encoding buffer for chain items
    // (registered as a non-trainable buffer).
    torch::Tensor chain_pos_enc_;

    // Transformer encoder (6L x 8H x d=512).
    torch::nn::TransformerEncoder encoder_{nullptr};

    // Spatial fusion — for each spatial node, mean-pool tokens at that
    // node, then run cross-attention layers with additive edge-type
    // biases between nodes. Implemented as standard MultiheadAttention
    // with a (num_spatial, num_spatial) bias matrix per edge type that
    // gets added to the attention logits.
    //
    // One set of (MHA + LN + FFN1 + FFN2 + LN2) per spatial-attn layer.
    // We use std::vector<TypedModule> + register_module per element so
    // checkpoint state-dict keys are deterministic and readable.
    std::vector<torch::nn::MultiheadAttention> spatial_attns_;
    std::vector<torch::nn::LayerNorm>          spatial_lns_pre_;
    std::vector<torch::nn::Linear>             spatial_ffns1_;
    std::vector<torch::nn::Linear>             spatial_ffns2_;
    std::vector<torch::nn::LayerNorm>          spatial_lns_post_;
    // Edge bias matrix per edge type. Shape (num_edge_types, num_spatial,
    // num_spatial). Learned scalar bias added to attention logits when
    // the (query_node, key_node) pair is connected by that edge type.
    // Initialized to zero (model learns connectivity from data).
    torch::Tensor edge_bias_;

    // Pointer heads: action-type categorical + three dot-product pointers
    // (source/target/dest) computed from a query projection of the
    // pooled state against encoder token embeddings.
    torch::nn::Linear action_type_head_{nullptr};
    torch::nn::Linear source_query_proj_{nullptr};
    torch::nn::Linear target_query_proj_{nullptr};
    torch::nn::Linear dest_query_proj_{nullptr};
    torch::nn::Linear dest_node_key_proj_{nullptr};

    // Flat policy projection — pooled state → kVocabSize logits. Used
    // as the AlphaZero training target until pointer-head decomposition
    // + auxiliary losses land.
    torch::nn::Linear flat_policy_head_{nullptr};

    // Value head.
    torch::nn::Linear value_hidden_{nullptr};
    torch::nn::Linear value_out_{nullptr};

    // Static sinusoidal encoding builder.
    torch::Tensor buildChainPositionalEncoding();
};
TORCH_MODULE(V2Model);

}  // namespace riftbound::ml
