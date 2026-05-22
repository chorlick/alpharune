#pragma once
/// @file entity_tokens.h
/// Entity-token state featurization for the V2 model architecture.
///
/// Parallel to `extractStateFeatures` (the flat 4623-dim vector used by
/// V1). V2 emits a STRUCTURED tensor: one token per game entity (card
/// instance, chain item) with explicit (card_def_id, zone, domain,
/// stance, stats, perspective, chain_index, spatial_node) fields per
/// token + a padding mask.
///
/// The model consumes these via embedding lookups + transformer encoder
/// + GNN spatial fusion + pointer heads (see docs/potential-model-
/// architecture.txt and src/ml/v2_model.h).
///
/// Information-set masking: the extractor takes `perspective` and zeroes
/// out OPP-hidden card identities (opp hand and main_deck contents,
/// facedown card identities at BFs when perspective is not the owner).
/// The TOKEN itself is emitted (so the count is observable), but its
/// `card_def_id` field is set to 0 (= unknown).

#include "core/card_db.h"
#include "core/game_state.h"

#include <cstdint>
#include <vector>

namespace riftbound::ml {

// ─── Token-field enums ──────────────────────────────────────────────────────

/// Coarse zone tag per token. Distinguishes own vs opponent zones so the
/// model can apply per-side reasoning without an external perspective
/// flag (though we also carry `is_perspective` per token for redundancy).
enum class EntityZone : int32_t {
    Unknown      = 0,
    SelfHand     = 1,
    SelfMainDeck = 2,
    SelfRuneDeck = 3,
    SelfTrash    = 4,
    SelfBanish   = 5,
    SelfBase     = 6,    // units/gear/runes at self base
    SelfChampion = 7,
    SelfLegend   = 8,
    OppHand      = 9,    // only emitted for revealed-to-self cards
    OppMainDeck  = 10,
    OppRuneDeck  = 11,
    OppTrash     = 12,
    OppBanish    = 13,
    OppBase      = 14,
    OppChampion  = 15,
    OppLegend    = 16,
    Battlefield  = 17,   // units at BF (spatial_node identifies which)
    Facedown     = 18,   // hidden card at BF
    ChainItem    = 19,   // spell/ability on the chain
};
inline constexpr int kEntityZoneCount = 20;

/// Coarse stance — runtime ready/exhausted status. Cards in non-board
/// zones (hand, deck, trash) report None.
enum class EntityStance : int32_t {
    None       = 0,
    Ready      = 1,
    Exhausted  = 2,
    Facedown   = 3,
    Stunned    = 4,    // composite Exhausted + Stunned for now
};
inline constexpr int kEntityStanceCount = 5;

/// Maximum entities per state token list. Larger than typical mid-game
/// counts (~100-150 game objects + ~0-4 chain items) so we don't lose
/// information; padding mask handles unused slots. Padded to a round
/// power of 2 for batched-tensor friendliness.
inline constexpr int kMaxEntityTokens = 256;

/// Per-entity continuous-stat dim. Currently [might, damage, buff_count,
/// energy_cost, power_cost]. Model uses bucket embeddings, but the
/// extractor emits raw integer values; bucketization happens model-side.
inline constexpr int kEntityStatsDim = 5;

/// Spatial-node count = 2 bases + up to N BFs. Riftbound starts with 2
/// BFs and can grow to 4 via token battlefields (Baron Pit, Brush, etc.).
/// We reserve up to 6 BFs = 8 spatial nodes total. Off-board entities
/// (hand, deck, chain) use -1.
inline constexpr int kSpatialNodeCount = 8;
inline constexpr int kSpatialNodeSelfBase  = 0;
inline constexpr int kSpatialNodeOppBase   = 1;
inline constexpr int kSpatialNodeBfBase    = 2;  // BF[0..5] -> nodes 2..7
inline constexpr int kSpatialNodeOffBoard  = -1;

// ─── Output container ───────────────────────────────────────────────────────

/// Struct-of-arrays output. Each field is `kMaxEntityTokens`-sized,
/// padded with zeros (and `valid_mask=0`) for unused slots. The
/// model-side adapter packs these into LibTorch tensors of shape
/// (batch, kMaxEntityTokens) or (batch, kMaxEntityTokens, kEntityStatsDim).
struct EntityTokens {
    int n_entities = 0;   // number of populated tokens (entries [0, n_entities) are valid)

    // Categorical fields (one int per token).
    std::vector<int32_t> card_def_id;   // [kMaxEntityTokens]  CardDefId (1..787), 0 = unknown/pad
    std::vector<int32_t> zone_id;       // [kMaxEntityTokens]  EntityZone
    std::vector<int32_t> domain_id;     // [kMaxEntityTokens]  Domain (0..5), 6 = none/pad
    std::vector<int32_t> stance_id;     // [kMaxEntityTokens]  EntityStance
    std::vector<int32_t> is_perspective;// [kMaxEntityTokens]  1 = belongs to perspective player
    std::vector<int32_t> chain_index;   // [kMaxEntityTokens]  position in chain (0..N), -1 if not chain
    std::vector<int32_t> spatial_node;  // [kMaxEntityTokens]  0..kSpatialNodeCount-1, -1 if off-board
    std::vector<int32_t> valid_mask;    // [kMaxEntityTokens]  1 = valid token, 0 = padding

    // Continuous stats (flattened [token_idx * kEntityStatsDim + field]).
    std::vector<float>   stats;         // [kMaxEntityTokens * kEntityStatsDim]
};

/// Build entity tokens from the game state, masked for `perspective`.
EntityTokens extractEntityTokens(const GameState& state,
                                  PlayerId perspective,
                                  const CardDB& card_db);

}  // namespace riftbound::ml
