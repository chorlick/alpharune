#pragma once
/// @file token_layout.h
/// The v6 observation: a set of tokens (v6 interface P0.3).
///
/// One observation is a short sequence of tokens, in this order:
///
///   [ global ] [ battlefield ... ] [ entity ... ] [ own-deck ... ]
///
/// Every token is the same three things: a kind, a card id (an embedding
/// index, 0 = no card / unknown card) and a fixed-width float vector whose
/// meaning depends on the kind. The model input is tokens only; there is no
/// flat state vector.
///
/// Everything is relative to the perspective player ("self" / "opp"); no
/// field says Player1 or Player2. Nothing the perspective player could not
/// know appears anywhere: hidden identities have card id 0, and hidden cards
/// are tokens only when the KnowledgeView says their identity is known.
///
/// CHANGING THIS FILE INVALIDATES CHECKPOINTS. Bump kLayoutVersion and never
/// reorder or remove a field; add at the end of a block and widen the block.
/// The field tables with normalisation are in the trainer repo,
/// plans/tooling-v6/interfaces.md.

#include "core/legal_action.h"
#include "ml/feature_extractor.h"
#include "core/types.h"

#include <array>
#include <cstdint>
#include <vector>

namespace riftbound::ml {

inline constexpr int kLayoutVersion = 3;   // 2: card ids 1..1150 (was 1..1100); 3: 1..2048

/// Size of the card embedding table: index 0 is "none / unknown", 1..2048 are
/// card ids (must equal feature_extractor.h's kCardVocabSize + 1).
inline constexpr int kCardEmbeddingRows = 2049;
static_assert(kCardEmbeddingRows == kCardVocabSize + 1,
              "card embedding table must cover every card id plus the 'none' row");

inline constexpr int kMaxBattlefieldTokens = 6;
inline constexpr int kMaxEntityTokens = 192;
inline constexpr int kMaxOwnDeckTokens = 48;
inline constexpr int kNumDomains = static_cast<int>(Domain::Count);      // 6
inline constexpr int kNumKeywords = static_cast<int>(Keyword::Count);    // 25
inline constexpr int kNumCardTypes = 6;                                  // CardType

enum class TokenKind : uint8_t { Global, Battlefield, Entity, OwnDeck, Count };

/// What the pending decision asks of the perspective player.
enum class DecisionKind : uint8_t {
    None,              // not this player's decision
    Mulligan,
    ChooseBattlefield,
    PlayFirst,
    MainPhase,         // neutral open: play, move, activate, end turn
    ChainResponse,     // closed state: react or pass priority
    Showdown,          // showdown open: act or pass focus
    AssignDamage,
    ChooseObject,      // prompted: pick a card or unit
    ChooseYesNo,       // prompted: "you may", pay or decline
    ChooseMode,
    ChooseAmount,      // prompted: X
    OptionalTrigger,
    TriggeredCost,
    Sideboard,
    Count,
};

/// Where an entity is, from the perspective player's point of view.
enum class EntityZone : uint8_t {
    Hand,            // own hand, or an opposing hand card whose identity is known
    Base,
    Battlefield,
    Facedown,        // hidden at a battlefield (card id 0 unless it is yours)
    Chain,
    Trash,
    Banishment,
    ChampionZone,
    LegendZone,
    Deck,            // a card with a known place in a deck (top / bottom)
    Count,
};

// ─── Float blocks ────────────────────────────────────────────────────────────
// Each namespace lists a kind's fields as offsets into its float vector.
// Flags are 0 / 1. Counts are divided by the scale given in interfaces.md and
// clipped to [0, 1] ([-1, 1] for signed values).

namespace global_field {
enum : int {
    SelfScore, OppScore,                  // / points needed to win
    SelfPointsToWin, OppPointsToWin,
    TurnNumber,
    IsSelfTurn, SelfIsStartingPlayer, IsAdditionalTurn,
    Phase,                                // one-hot, 11 wide (TurnPhase)
    InShowdown = Phase + 11,
    ChainOpen,                            // closed state
    SelfHasPriority, OppHasPriority, SelfHasFocus, OppHasFocus,
    SelfEnergy, OppEnergy,
    SelfPower,                            // per domain, kNumDomains wide
    OppPower = SelfPower + kNumDomains,
    SelfUniversalPower = OppPower + kNumDomains, OppUniversalPower,
    SelfXp, OppXp,
    SelfHandSize, OppHandSize,
    OppHandUnknown,                       // opposing hand cards with unknown identity
    SelfDeckSize, OppDeckSize,
    SelfRuneDeckSize, OppRuneDeckSize,
    SelfTrashSize, OppTrashSize,
    SelfBanishSize, OppBanishSize,
    SelfBurnedOut, OppBurnedOut,
    SelfCardsPlayedThisTurn, OppCardsPlayedThisTurn,
    SelfSpellsPlayedThisTurn, OppSpellsPlayedThisTurn,
    AnyUnitDiedThisTurn,
    Decision,                             // one-hot, DecisionKind::Count wide
    LegalActionCount = Decision + static_cast<int>(DecisionKind::Count),
    EntitiesDropped,                      // entities left out because of the token cap
    Count,
};
}

namespace battlefield_field {
enum : int {
    ControlledBySelf, ControlledByOpp, Uncontrolled,
    Contested, ContestedBySelf,
    ShowdownStaged, ShowdownInProgress, CombatStaged, CombatInProgress,
    SelfIsAttacker, SelfIsDefender,
    CombatPhase,                          // one-hot, 4 wide
    SelfUnits = CombatPhase + 4, OppUnits,
    SelfMight, OppMight,
    SelfFacedown, OppFacedown,
    ScoredBySelfThisTurn, ScoredByOppThisTurn,
    IsTokenBattlefield, ContributedBySelf,
    Count,
};
}

/// Shared by Entity and OwnDeck tokens.
namespace entity_field {
enum : int {
    Zone,                                 // one-hot, EntityZone::Count wide
    ControlledBySelf = Zone + static_cast<int>(EntityZone::Count),
    OwnedBySelf,
    IdentityKnown,
    Type,                                 // multi-hot, kNumCardTypes wide (a card may have several)
    IsChampion = Type + kNumCardTypes, IsSignature,
    IsToken,                              // intrinsic, independent of type and supertype
    Domain,                               // multi-hot, kNumDomains wide
    EnergyCost = Domain + kNumDomains, PowerCost,
    Might, BaseMight, Damage,
    Ready, Exhausted, Stunned,
    Empowered, Buffs,
    Keywords,                             // multi-hot, kNumKeywords wide
    AssaultValue = Keywords + kNumKeywords, ShieldValue, DeflectValue,
    IsAttacker, IsDefender,
    AtSelfBase, AtOppBase,
    AtBattlefield,                        // one-hot, kMaxBattlefieldTokens wide
    IsAttached = AtBattlefield + kMaxBattlefieldTokens,
    AttachmentCount,
    ChainPosition,                        // 0 = resolves next
    UntargetableByOpp, ImmuneToDamage,
    MovesThisTurn,
    // Knowledge tags (KnowledgeView). Zero for cards in public zones.
    KnownInHand, KnownInDeck, KnownDeckTop, KnownDeckBottom,
    DeckRank,                             // nearest slot it can be in
    DeckRankSpan,                         // slots it may occupy; 1 = exact place
    ZoneUnconfirmed,
    // OwnDeck tokens only: copies of this card at an unknown place in the deck.
    Copies,
    Count,
};
}

inline constexpr int kGlobalDim = global_field::Count;
inline constexpr int kBattlefieldDim = battlefield_field::Count;
inline constexpr int kEntityDim = entity_field::Count;

// ─── Observation ─────────────────────────────────────────────────────────────

struct GlobalToken {
    std::array<float, kGlobalDim> f{};
};

struct BattlefieldToken {
    int32_t card = 0;                      // the battlefield card
    std::array<float, kBattlefieldDim> f{};
};

struct EntityToken {
    int32_t card = 0;                      // 0 = identity unknown
    /// Index into Observation::entities of the object this is attached to,
    /// or -1.
    int32_t attached_to = -1;
    std::array<float, kEntityDim> f{};
};

/// A reference from an action part to a token: the token's index in the flat
/// sequence [global][battlefields][entities][own-deck], or -1 for "no token"
/// (no such part, or an object the observer cannot see).
using TokenRef = int32_t;
inline constexpr TokenRef kNoToken = -1;

/// One legal action, with its parts resolved to tokens. Produced together
/// with the observation so the two always agree.
struct ActionTokens {
    ActionKind kind = ActionKind::EndTurn;
    TokenRef source = kNoToken;
    std::vector<TokenRef> objects;         // parallel to LegalActionDesc::objects
    std::vector<TokenRef> targets;         // parallel to LegalActionDesc::targets
    std::vector<float> amounts;            // parallel to targets, / 10
    ActionDestKind dest_kind = ActionDestKind::None;
    bool dest_is_self_base = false;        // dest_kind == Base
    TokenRef dest = kNoToken;              // the battlefield token, dest_kind == Battlefield
    bool has_value = false;
    int32_t value = 0;                     // clipped to [0, 15]
    ActionPlaySource play_source = ActionPlaySource::Hand;
    bool alt_cost = false;
};

struct Observation {
    int layout_version = kLayoutVersion;
    GlobalToken global;
    std::vector<BattlefieldToken> battlefields;   // <= kMaxBattlefieldTokens
    std::vector<EntityToken> entities;            // <= kMaxEntityTokens
    std::vector<EntityToken> own_deck;            // <= kMaxOwnDeckTokens, one per distinct card
    /// The legal actions of the pending decision; index = action id. Empty
    /// when the perspective player is not the one deciding.
    std::vector<ActionTokens> actions;

    int tokenCount() const {
        return 1 + static_cast<int>(battlefields.size() + entities.size() + own_deck.size());
    }
    TokenRef battlefieldRef(int i) const { return 1 + i; }
    TokenRef entityRef(int i) const { return 1 + static_cast<int>(battlefields.size()) + i; }
    TokenRef ownDeckRef(int i) const {
        return 1 + static_cast<int>(battlefields.size() + entities.size()) + i;
    }
};

} // namespace riftbound::ml
