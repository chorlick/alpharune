#pragma once
/// @file feature_extractor.h
/// Shared ML feature extraction surface.
///
/// Used by ModelAgent (online inference) and BinaryDataSerializer (offline
/// training-data generation). Keeping a single implementation guarantees the
/// features fed to a model during training are byte-identical to those it
/// sees at inference time.
///
/// Layout must remain in sync with scripts/train_agent.py extract_state_features.

#include "core/card_db.h"
#include "core/game_state.h"
#include "core/intent.h"

#include <vector>

namespace riftbound::ml {

/// State-feature layout (4671 dims total):
///   0..353       : base features (unchanged historical layout — see source).
///   354..361     : chain item targets (4 slots × 2 target def_ids).
///   362..365     : cost-modifier types (2 players × {next_spell_only, next_unit_only}).
///   366..373     : per-BF scored flags (4 BFs × 2 players).
///   374..469     : per-unit extended features (4 BFs × 2 sides × top-3 ×
///                  {combat_designation, attachment_def_id, might_delta,
///                   temp_might_bonus}).
///   470..1256    : self main_deck multi-hot counts (787 cards, card_def_id 1..787 → pos 0..786).
///   1257..2043   : self trash multi-hot counts.
///   2044..2830   : self banishment multi-hot counts.
///   2831..3617   : opponent trash multi-hot counts.
///   3618..4404   : opponent banishment multi-hot counts.
///   4405         : self.cant_play_cards_this_turn (Brynhir lockout).
///   4406         : opp.cant_play_cards_this_turn.
///   4407..4598   : Tier 2/3 extended per-unit features (4 BFs × 2 sides × top-3 ×
///                  {assault, shield, deflect, buff_count, temp_buff_count, tank_kw,
///                   backline_kw, ganking_kw}). 8 fields × 6 unit slots × 4 BFs = 192 floats.
///                  Layout: BF i (i=0..3), then side j (j=self,opp), then unit k (k=0..2),
///                  then field f (f=0..7). Position = 4407 + ((i*2 + j)*3 + k)*8 + f.
///   4599..4622   : extended globals (24 floats).
///                  Layout:
///                    4599..4600 ready base units (self, opp)
///                    4601..4602 cost modifier magnitude sum (self, opp)
///                    4603..4604 additional turn queue depth (self, opp)
///                    4605..4608 contested-by-self per BF (×4)
///                    4609..4612 combat_staged per BF (×4)
///                    4613..4616 showdown_staged per BF (×4)
///                    4617..4620 facedown age (oldest hidden card) per BF (×4)
///                    4621       focus_holder (1=self, 0=opp, -1=no showdown)
///                    4622       total battlefield count
///
/// Multi-hot zones encode perfect-information state per the game rules: each
/// player has full knowledge of their own deck/trash/banishment and the
/// opponent's public zones (trash, banishment). Opponent's deck and hand
/// remain hidden as per CR.
inline constexpr int kStateFeatureDim     = 4623;
inline constexpr int kActionFeatureDim    = 25;
inline constexpr int kMaxLegalActions     = 64;
inline constexpr int kMaxChainItems       = 4;
inline constexpr int kMaxBfUnitsPerSide   = 3;
inline constexpr int kPerUnitExtFields    = 4;   // cd, gear, delta, temp_might
inline constexpr int kPerUnitExtFieldsT2  = 8;   // T2/3 per-unit: assault, shield, deflect,
                                                 // buff_count, temp_buff_count, tank, backline, ganking
inline constexpr int kCardVocabSize       = 787;
inline constexpr int kZoneVectorOffset    = 470;  // first multi-hot zone starts here

/// Build a 354-dim state feature vector from `perspective`'s point of view.
/// Opponent-private information (hand, hidden cards) is masked.
std::vector<float> extractStateFeatures(const GameState& state,
                                        PlayerId perspective,
                                        const CardDB& card_db);

/// Build a 25-dim feature vector describing a single legal action.
/// The chosen intent vs the legal-action slot share the same featurization.
std::vector<float> featurizeAction(const Intent& intent,
                                   const GameState& state);

/// Map an IntentType to its one-hot index (0..13). Must match Python's
/// action_type_index in train_agent.py.
int actionTypeIndex(IntentType type);

} // namespace riftbound::ml
