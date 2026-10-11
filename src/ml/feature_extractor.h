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

/// State-feature layout (kStateFeatureDim = 6188 dims total):
///   0..353       : base features (unchanged historical layout — see source).
///   354..361     : chain item targets (4 slots × 2 target def_ids).
///   362..365     : cost-modifier types (2 players × {next_spell_only, next_unit_only}).
///   366..373     : per-BF scored flags (4 BFs × 2 players).
///   374..469     : per-unit extended features (4 BFs × 2 sides × top-3 ×
///                  {combat_designation, attachment_def_id, might_delta,
///                   temp_might_bonus}).
///   Z = kZoneVectorOffset (470), V = kCardVocabSize (1100); card_def_id i → pos i-1.
///   Z      .. Z+V-1   : self main_deck multi-hot counts.
///   Z+V    .. Z+2V-1  : self trash multi-hot counts.
///   Z+2V   .. Z+3V-1  : self banishment multi-hot counts.
///   Z+3V   .. Z+4V-1  : opponent trash multi-hot counts.
///   Z+4V   .. Z+5V-1  : opponent banishment multi-hot counts.
///   T = Z+5V (5970):
///   T                 : self.cant_play_cards_this_turn (Brynhir lockout).
///   T+1               : opp.cant_play_cards_this_turn.
///   T+2 .. T+193      : Tier 2/3 extended per-unit features (4 BFs × 2 sides × top-3 ×
///                  {assault, shield, deflect, buff_count, temp_buff_count, tank_kw,
///                   backline_kw, ganking_kw}). 8 fields × 6 unit slots × 4 BFs = 192 floats.
///                  Layout: BF i (i=0..3), then side j (j=self,opp), then unit k (k=0..2),
///                  then field f (f=0..7). Position = T+2 + ((i*2 + j)*3 + k)*8 + f.
///   T+194 .. T+217    : extended globals (24 floats), in order:
///                    ready base units (self, opp); cost modifier magnitude sum (self, opp);
///                    additional turn queue depth (self, opp); contested-by-self per BF (×4);
///                    combat_staged per BF (×4); showdown_staged per BF (×4);
///                    facedown age (oldest hidden card) per BF (×4);
///                    focus_holder (1=self, 0=opp, -1=no showdown); total battlefield count.
///
/// Multi-hot zones encode perfect-information state per the game rules: each
/// player has full knowledge of their own deck/trash/banishment and the
/// opponent's public zones (trash, banishment). Opponent's deck and hand
/// remain hidden as per CR.

inline constexpr int kActionFeatureDim    = 25;
inline constexpr int kMaxLegalActions     = 64;
inline constexpr int kMaxChainItems       = 4;
inline constexpr int kMaxBfUnitsPerSide   = 3;
inline constexpr int kPerUnitExtFields    = 4;   // cd, gear, delta, temp_might
inline constexpr int kPerUnitExtFieldsT2  = 8;   // T2/3 per-unit: assault, shield, deflect,
                                                 // buff_count, temp_buff_count, tank, backline, ganking
// Card-def id space for the multi-hot zones. Registered ids run 1..955
// (Origins/Proving Grounds/Spiritforged/Unleashed 1..787, Vendetta 788..960);
// 961..1100 is reserved for Radiance so its release doesn't shift the layout
// (and invalidate checkpoints) a second time.
inline constexpr int kCardVocabSize       = 1100;
inline constexpr int kZoneVectorOffset    = 470;  // first multi-hot zone starts here
inline constexpr int kNumZoneVectors      = 5;    // self deck/trash/banish, opp trash/banish
// 470 base+extended dims, 5 zone multi-hots, 2 play-lockout flags, 192 Tier-2/3
// per-unit dims, 24 extended globals.  = 6188 with kCardVocabSize = 1100.
inline constexpr int kStateFeatureDim     =
    kZoneVectorOffset + kNumZoneVectors * kCardVocabSize + 2 + 192 + 24;

/// Build a 354-dim state feature vector from `perspective`'s point of view.
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

/// Build a 25-dim feature vector describing a single legal action.
/// The chosen intent vs the legal-action slot share the same featurization.
std::vector<float> featurizeAction(const Intent& intent,
                                   const GameState& state);

/// Map an IntentType to its one-hot index (0..13). Must match Python's
/// action_type_index in train_agent.py.
int actionTypeIndex(IntentType type);

} // namespace riftbound::ml
