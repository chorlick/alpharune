#pragma once
/// @file feature_extractor.h
/// Shared ML feature extraction surface.
///
/// Used by `RiftboundState::ObservationTensor` (the OpenSpiel info-set
/// view) so that any future C++ AlphaZero / NN training driver receives
/// the same feature layout the engine produces in production.
///
/// The previous Python training pipeline + ONNX inference path is gone
/// (see docs); the layout is no longer constrained to mirror a Python
/// extractor. Keep the header stable so future training drivers can
/// rely on it.

#include "core/card_db.h"
#include "core/game_state.h"
#include "core/intent.h"

#include <vector>

namespace riftbound::ml {

/// State-feature layout (kStateFeatureDim dims). Every block has a named
/// offset in `namespace layout` below; `extractStateFeatures` checks each
/// offset as it writes.
///
/// ONE ORDERING CONVENTION: everything is relative to the perspective player.
/// Wherever a block holds one entry per player, the perspective player
/// ("self") comes first and the opponent second; a field that names a player
/// is 1 = self, 2 = opponent, 0 = nobody (or the yes/no "is it self"). No
/// field says Player1 or Player2, so the vector of a position seen by one seat
/// equals the vector of the seat-swapped position seen by the other
/// (FeaturePerspective.SeatSwapGivesTheSameVector*). Who goes first is the
/// global `kGlobalSelfStarts`; the seat itself has no rules meaning and is not
/// a feature. Battlefield slots are not reordered (slot i is battlefield i for
/// both players, as in the action vocabulary); which player brought each slot
/// to the game is the `kBfContributedBy` block.
///
///   kGlobals (10)        turn number, phase, self is turn player, self is the
///                        starting player, score difference (self - opp), chain
///                        size, showdown, closed state, additional turn,
///                        delayed-ability count.
///   kChainItems (8)      4 chain slots x {source card id, controlled by self}.
///   kSelfPlayer (70)     the perspective player's block (see the source).
///   kOppPlayer (70)      the opponent's block, hand identities masked.
///   kBattlefields (196)  4 slots x 49: 9 header fields {card id, controller
///                        (0 none / 1 self / 2 opp), contested, combat, showdown,
///                        facedown count, self is attacker (-1 outside combat),
///                        combat phase, token} then the SELF side (20) then the
///                        OPPONENT side (20): 11 counts, top-3 unit card ids,
///                        top-3 Might, top-3 damage (top-3 by Might, descending).
///   kChainTargets (8)    4 chain slots x 2 target card ids.
///   kCostModTypes (4)    {next spell only, next unit only} for self, then opp.
///   kScoredFlags (8)     4 slots x {scored by self, scored by opp} this turn.
///   kUnitExt (96)        4 slots x {self side, opp side} x top-3 units x
///                        {combat designation, attachment card id, Might delta,
///                        temporary Might}; same units, same order as kBattlefields.
///   kZones (5 x V)       V = kCardVocabSize; card id i at position i-1; counts:
///                        self Main Deck, self trash, self banishment, opponent
///                        trash, opponent banishment.
///   kPlayLockout (2)     can't play cards this turn: self, opp.
///   kUnitT2 (192)        4 slots x {self side, opp side} x top-3 units x
///                        {assault, shield, deflect, buffs, temporary buffs, Tank,
///                        Backline, Ganking}.
///   kExtGlobals (24)     ready base units (self, opp); cost-modifier sum (self,
///                        opp); additional turns queued (self, opp); contested by
///                        self x4; combat staged x4; showdown staged x4; facedown
///                        age x4; focus holder (1 self / 0 opp / -1 none);
///                        battlefield count.
///   kBfContributedBy (4) per slot: who brought the battlefield (0 none / token,
///                        1 self, 2 opp).
///
/// The multi-hot zones are what a player knows by the rules: the composition
/// (never the order) of their own Main Deck, and both players' public trash
/// and banishment. The opponent's deck and hand identities are never written.
/// docs/checkpoint-breaking-2026-10.md lists what changed from the 6188-dim
/// layout.

inline constexpr int kActionFeatureDim    = 25;
inline constexpr int kMaxLegalActions     = 64;
inline constexpr int kMaxChainItems       = 4;
inline constexpr int kMaxBfUnitsPerSide   = 3;
inline constexpr int kPerUnitExtFields    = 4;   // cd, gear, delta, temp_might
inline constexpr int kPerUnitExtFieldsT2  = 8;   // T2/3 per-unit: assault, shield, deflect,
                                                 // buff_count, temp_buff_count, tank, backline, ganking
// Card-def id space for the multi-hot zones. Registered ids run 1..955
// (Origins/Proving Grounds/Spiritforged/Unleashed 1..787, Vendetta 788..955);
// 956..1150 (195 ids) is reserved for Radiance so its release doesn't shift
// the layout (and invalidate checkpoints) a second time. Radiance needs 169
// to 174 by the gallery data of 2026-10 (plans/09 item 9.SS2): the 167 cards
// of its numbered range, two tokens it introduces (Bomb, the 3-Might Mech),
// and up to five over-numbered champions that may be cards of their own; its
// other gallery entries are second printings or basic runes that have ids.
// The 21 to 26 spare cover what the 23 cards of the range not yet shown may
// add. It was 1100: 145 reserved, at least 24 short. The derivation is in
// docs/checkpoint-breaking-2026-10.md, section 0.
inline constexpr int kCardVocabSize       = 1150;
inline constexpr int kZoneVectorOffset    = 470;  // first multi-hot zone starts here
inline constexpr int kNumZoneVectors      = 5;    // self deck/trash/banish, opp trash/banish

/// Where each block of the state vector starts. Side 0 is the perspective
/// player, side 1 the opponent, in every helper.
namespace layout {
inline constexpr int kBfSlots         = 4;     // battlefield slots in the vector
inline constexpr int kPlayerDim       = 70;
inline constexpr int kBfHeaderDim     = 9;
inline constexpr int kBfSideDim       = 20;
inline constexpr int kBfDim           = kBfHeaderDim + 2 * kBfSideDim;        // 49
inline constexpr int kUnitExtSideDim  = kMaxBfUnitsPerSide * kPerUnitExtFields;    // 12
inline constexpr int kUnitT2SideDim   = kMaxBfUnitsPerSide * kPerUnitExtFieldsT2;  // 24

inline constexpr int kGlobals         = 0;
inline constexpr int kGlobalSelfIsTurnPlayer = kGlobals + 2;
inline constexpr int kGlobalSelfStarts       = kGlobals + 3;
inline constexpr int kGlobalScoreDiff        = kGlobals + 4;
inline constexpr int kChainItems      = kGlobals + 10;
inline constexpr int kSelfPlayer      = kChainItems + 2 * kMaxChainItems;      // 18
inline constexpr int kOppPlayer       = kSelfPlayer + kPlayerDim;              // 88
inline constexpr int kBattlefields    = kOppPlayer + kPlayerDim;               // 158
inline constexpr int kChainTargets    = kBattlefields + kBfSlots * kBfDim;     // 354
inline constexpr int kCostModTypes    = kChainTargets + 2 * kMaxChainItems;    // 362
inline constexpr int kScoredFlags     = kCostModTypes + 4;                     // 366
inline constexpr int kUnitExt         = kScoredFlags + 2 * kBfSlots;           // 374
inline constexpr int kZones           = kUnitExt + kBfSlots * 2 * kUnitExtSideDim;  // 470
inline constexpr int kPlayLockout     = kZones + kNumZoneVectors * kCardVocabSize;  // 5970
inline constexpr int kUnitT2          = kPlayLockout + 2;
inline constexpr int kExtGlobals      = kUnitT2 + kBfSlots * 2 * kUnitT2SideDim;
inline constexpr int kExtGlobalsDim   = 24;
inline constexpr int kExtFocusHolder  = kExtGlobals + 22;
inline constexpr int kBfContributedBy = kExtGlobals + kExtGlobalsDim;
inline constexpr int kEnd             = kBfContributedBy + kBfSlots;

static_assert(kZones == kZoneVectorOffset);

/// Header of battlefield slot `bf` (field 1 = controller, 6 = self is attacker).
constexpr int bf(int bf) { return kBattlefields + bf * kBfDim; }
/// The 20-float block of one side of battlefield slot `bf`.
constexpr int bfSide(int bf, int side) {
    return kBattlefields + bf * kBfDim + kBfHeaderDim + side * kBfSideDim;
}
/// Within a side block: 11 counts, then 3 card ids, 3 Might, 3 damage.
inline constexpr int kSideUnitIds = 11, kSideUnitMight = 14, kSideUnitDamage = 17;
constexpr int unitExt(int bf, int side, int unit) {
    return kUnitExt + (bf * 2 + side) * kUnitExtSideDim + unit * kPerUnitExtFields;
}
constexpr int unitT2(int bf, int side, int unit) {
    return kUnitT2 + (bf * 2 + side) * kUnitT2SideDim + unit * kPerUnitExtFieldsT2;
}
/// Zone multi-hot `zone` (0 self deck, 1 self trash, 2 self banishment,
/// 3 opp trash, 4 opp banishment).
constexpr int zone(int zone) { return kZones + zone * kCardVocabSize; }
} // namespace layout

// 6442 with kCardVocabSize = 1150. Before the 2026-10 checkpoint break it was
// 6188: + the 4 kBfContributedBy dims, + 5 x 50 for the larger card id space.
inline constexpr int kStateFeatureDim     = layout::kEnd;
static_assert(kStateFeatureDim ==
              kZoneVectorOffset + kNumZoneVectors * kCardVocabSize + 2 + 192 + 24 + 4);

/// Build the state feature vector (kStateFeatureDim dims, layout above) from
/// `perspective`'s point of view.
/// Opponent-private information (hand, hidden cards) is masked.
std::vector<float> extractStateFeatures(const GameState& state,
                                        PlayerId perspective,
                                        const CardDB& card_db);

/// The same vector written into `out`, which is cleared first and keeps its
/// capacity: a caller that extracts repeatedly (search keys a node on every
/// visit) passes the same buffer and pays no allocation.
void extractStateFeatures(const GameState& state,
                          PlayerId perspective,
                          const CardDB& card_db,
                          std::vector<float>& out);

/// Build a kActionFeatureDim-dim (25) feature vector describing a single legal
/// action.
/// The chosen intent vs the legal-action slot share the same featurization.
std::vector<float> featurizeAction(const Intent& intent,
                                   const GameState& state);

/// Map an IntentType to its one-hot index (0..13). Must match Python's
/// action_type_index in train_agent.py.
int actionTypeIndex(IntentType type);

} // namespace riftbound::ml
