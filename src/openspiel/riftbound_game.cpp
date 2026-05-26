#include "riftbound_game.h"
#include "riftbound_state.h"

#include "ml/feature_extractor.h"  // kStateFeatureDim
#include "action_vocab.h"          // kVocabSize

namespace riftbound::openspiel {

// GameType lives here so the Game constructor and the registration call
// share a single source of truth.
const ::open_spiel::GameType kRiftboundGameType{
    /*short_name=*/"riftbound",
    /*long_name=*/"Riftbound TCG (1v1)",
    /*dynamics=*/::open_spiel::GameType::Dynamics::kSequential,
    // Phase B-1: engine still owns draws / mulligan replacements / shuffles
    // / coin toss internally. The OpenSpiel view sees deterministic
    // sampled-internal stochasticity. ChanceOutcomes-driven incremental
    // deck realization is Phase B-2 work (see CLAUDE.md).
    /*chance_mode=*/::open_spiel::GameType::ChanceMode::kSampledStochastic,
    // Phase B: hidden information (opponent hand, deck order, facedown
    // contents) is masked in ObservationTensor.
    /*information=*/::open_spiel::GameType::Information::kImperfectInformation,
    /*utility=*/::open_spiel::GameType::Utility::kZeroSum,
    /*reward_model=*/::open_spiel::GameType::RewardModel::kTerminal,
    /*max_num_players=*/2,
    /*min_num_players=*/2,
    /*provides_information_state_string=*/false,
    /*provides_information_state_tensor=*/false,
    // ObservationString is implemented (FNV-1a hash over the masked
    // observation tensor bytes) — needed by ISMCTSBot to key tree
    // nodes. Same masking rules as ObservationTensor: two states the
    // perspective player cannot distinguish hash identically.
    /*provides_observation_string=*/true,
    /*provides_observation_tensor=*/true,
    /*parameter_specification=*/
    { {"deck1",    ::open_spiel::GameParameter(std::string("decks/leblanc_test.txt"))},
      {"deck2",    ::open_spiel::GameParameter(std::string("decks/leblanc_test.txt"))},
      {"registry", ::open_spiel::GameParameter(std::string("cards/ban-list.csv"))},
      {"seed",     ::open_spiel::GameParameter(0)} }
};

std::shared_ptr<const ::open_spiel::Game> MakeRiftboundGame(
    const ::open_spiel::GameParameters& params) {
    return std::shared_ptr<const ::open_spiel::Game>(new RiftboundGame(params));
}

RiftboundGame::RiftboundGame(const ::open_spiel::GameParameters& params)
    : Game(kRiftboundGameType, params),
      seed_(static_cast<uint64_t>(ParameterValue<int>("seed"))) {
    // Card data — including ban status — is owned by the C++ classes. The
    // "registry" GameParameter is retained for back-compat but ignored.
    registry_.loadAll();
    card_db_.buildFromClasses(registry_);
    deck1_ = ::riftbound::DeckValidator::loadFromDeckList(
        ParameterValue<std::string>("deck1"), card_db_);
    deck2_ = ::riftbound::DeckValidator::loadFromDeckList(
        ParameterValue<std::string>("deck2"), card_db_);
}

std::unique_ptr<::open_spiel::State> RiftboundGame::NewInitialState() const {
    return std::make_unique<RiftboundState>(shared_from_this());
}

int RiftboundGame::NumDistinctActions() const {
    // Dense vocab: contiguous [0, kVocabSize) slots. AlphaZero-style
    // policy heads emit a softmax of this size. Each verb bucket lives
    // in its own slot range; semantic variants (PlayCard / PlayReaction)
    // intentionally collapse to one slot. See action_vocab.h.
    return kVocabSize;
}

std::vector<int> RiftboundGame::ObservationTensorShape() const {
    return {::riftbound::ml::kStateFeatureDim};
}

} // namespace riftbound::openspiel

// ── Registration ─────────────────────────────────────────────────────────────
// REGISTER_SPIEL_GAME expands to an unqualified `GameRegisterer` reference,
// which lives in `namespace open_spiel`. So this block has to be inside that
// namespace.

namespace open_spiel {
namespace {
REGISTER_SPIEL_GAME(::riftbound::openspiel::kRiftboundGameType,
                    ::riftbound::openspiel::MakeRiftboundGame);
} // namespace
} // namespace open_spiel
