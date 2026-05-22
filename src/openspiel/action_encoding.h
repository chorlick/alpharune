#pragma once
/// @file action_encoding.h
/// Bit-packed int64 action encoding for the OpenSpiel wrapper.
///
/// Phase A used "index into current legal-actions list" as the action ID.
/// That's unstable across clones — the legal list is recomputed in the
/// cloned state and may have a different order, so a replayed action_id
/// can resolve to a different Intent than the original.
///
/// Phase B replaces this with a structural encoding derived from
/// CardDefIds (which ARE stable across clones because they're static
/// card-template IDs from registry.json).
///
/// Encoding layout (52 bits, fits in signed int64):
///   bits 47..51 (5)  : IntentType
///   bits 36..46 (11) : card_def_id slot
///   bits 25..35 (11) : target1 slot
///   bits 14..24 (11) : target2 slot
///   bits 11..13 (3)  : dest_bf slot
///   bits  0..10 (11) : ability_source slot
///
/// Each slot is repurposed per IntentType — see encodeAction() for the
/// full mapping. Decoding searches the current legal-action list for any
/// Intent whose re-encoded ID matches the input, returning the first hit.
/// When two legal Intents collide (e.g., two copies of the same card
/// targeting the same def), OpenSpiel collapses them into one Action,
/// which is correct since they're game-tree-equivalent decisions.

#include "core/game_state.h"
#include "core/intent.h"

#include <cstdint>
#include <vector>

namespace riftbound::openspiel {

// ── Bit-field layout ────────────────────────────────────────────────────────

constexpr int kTypeBits          = 5;
constexpr int kCardBits          = 11;
constexpr int kTarget1Bits       = 11;
constexpr int kTarget2Bits       = 11;
constexpr int kDestBfBits        = 3;
constexpr int kAbilitySourceBits = 11;

constexpr int kTypeShift          = 47;
constexpr int kCardShift          = 36;
constexpr int kTarget1Shift       = 25;
constexpr int kTarget2Shift       = 14;
constexpr int kDestBfShift        = 11;
constexpr int kAbilitySourceShift = 0;

constexpr int64_t kTypeMask          = (1LL << kTypeBits)          - 1;
constexpr int64_t kCardMask          = (1LL << kCardBits)          - 1;
constexpr int64_t kTarget1Mask       = (1LL << kTarget1Bits)       - 1;
constexpr int64_t kTarget2Mask       = (1LL << kTarget2Bits)       - 1;
constexpr int64_t kDestBfMask        = (1LL << kDestBfBits)        - 1;
constexpr int64_t kAbilitySourceMask = (1LL << kAbilitySourceBits) - 1;

/// Upper bound on returned action IDs. NumDistinctActions returns this
/// so OpenSpiel sizes policy tensors / Q-table buckets correctly.
constexpr int64_t kMaxActionId = 1LL << 52;

/// Encode an Intent into a 52-bit ActionID. CardDefIds are looked up
/// from the supplied state for any GameObjectId references in the Intent.
int64_t encodeAction(const Intent& intent, const GameState& state);

/// Find the first Intent in `legal_actions` whose encoding equals
/// `action_id`. Returns nullptr if none match.
const Intent* decodeAction(int64_t action_id,
                            const std::vector<Intent>& legal_actions,
                            const GameState& state);

} // namespace riftbound::openspiel
