#include "riftbound_game.h"
#include "riftbound_state.h"

#include "ml/feature_extractor.h"  // kStateFeatureDim
#include "action_vocab.h"          // kNumActions

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
    { {"deck1",    ::open_spiel::GameParameter(std::string("decks/yi_rebuild.txt"))},
      {"deck2",    ::open_spiel::GameParameter(std::string("decks/yi_rebuild.txt"))},
      // Ignored. Accepted only so game strings written before card data
      // moved into the C++ classes still parse.
      {"registry", ::open_spiel::GameParameter(std::string(""))},
      {"seed",     ::open_spiel::GameParameter(0)},
      // The Mode of Play: "duel" (default; each setup battlefield drawn at
      // random, CR 487.5) or "match" (each player chooses it, CR 488.5).
      {"game_mode", ::open_spiel::GameParameter(std::string("duel"))} }
};

std::shared_ptr<const ::open_spiel::Game> MakeRiftboundGame(
    const ::open_spiel::GameParameters& params) {
    return std::shared_ptr<const ::open_spiel::Game>(new RiftboundGame(params));
}

const SharedCards& sharedCards() {
    // Card data — including ban status — is owned by the C++ classes, so
    // there is exactly one table to build. A function-local static is
    // initialised once, under the compiler's lock.
    static const SharedCards* const cards = [] {
        auto* built = new SharedCards();
        built->registry.loadAll();
        built->db.buildFromClasses(built->registry);
        return built;
    }();
    return *cards;
}

RiftboundGame::RiftboundGame(const ::open_spiel::GameParameters& params)
    : Game(kRiftboundGameType, params),
      card_db_(sharedCards().db),
      registry_(sharedCards().registry),
      seed_(static_cast<uint64_t>(ParameterValue<int>("seed"))),
      match_format_(::riftbound::matchFormatFromString(
          ParameterValue<std::string>("game_mode"))) {
    // The "registry" GameParameter is accepted but never read. The decks are
    // read per game: a deck file may change between two LoadGame calls.
    deck1_ = ::riftbound::DeckValidator::loadFromDeckList(
        ParameterValue<std::string>("deck1"), card_db_);
    deck2_ = ::riftbound::DeckValidator::loadFromDeckList(
        ParameterValue<std::string>("deck2"), card_db_);
}

std::unique_ptr<::open_spiel::State> RiftboundGame::NewInitialState() const {
    return std::make_unique<RiftboundState>(shared_from_this());
}

int RiftboundGame::NumDistinctActions() const {
    // A fixed number: kNumSlots descriptions times kTwinPlanes. An id says
    // what the action is made of (its slot) and which of the legal intents
    // with that description it is. See action_vocab.h.
    return kNumActions;
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
