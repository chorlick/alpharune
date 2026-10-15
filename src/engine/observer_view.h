#pragma once
/// @file observer_view.h
/// Everything one player may know of a game, and the facts the engine derives
/// for its own rules, as per-observer queries (testing phase T4, T6; trainer
/// plan section 2 and section 0 items 8-10).
///
/// `KnowledgeView` (core/knowledge_view.h) says what a player knows about
/// cards in hidden zones. `ObserverView` puts it beside the rest of what that
/// player may know — its own hand, every public zone in full, the chain with
/// its links, Rune Deck slots it knows — and the derived facts the engine
/// itself computes (effective Might and keywords, lethal damage, combat,
/// spendable resources, the timing window, the opponent's legal card pool),
/// so a model never has to re-derive a rule.
///
/// Everything is computed when asked, from the state and the engine's own
/// rule functions: nothing is kept up to date per Cleanup, and a view costs
/// nothing while nobody builds one. A view holds a reference to the state:
/// build one per question, do not keep it across a state change.
///
/// Object ids appear only as links (a chain item's source and targets, an
/// attachment), to tie one answer to another; they are random labels
/// (batch 5b T2) and are never a feature. `fingerprint()` is everything the
/// view shows with no id in it.

#include "core/card_db.h"
#include "core/game_state.h"
#include "core/knowledge_view.h"
#include "engine/knowledge_tracker.h"
#include "rules/deck_model.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace riftbound {

class CardRegistry;

/// A permanent (or a legend, a battlefield card, a rune) on the board as the
/// rules see it now (T6).
struct PermanentFacts {
    GameObjectId id = kInvalidId;          // a link, never a feature
    CardDefId card = kInvalidId;           // kInvalidId when the observer may not see it
    PlayerId controller = PlayerId::None;
    std::optional<LocationId> location;
    int might = 0;                         // after continuous effects (GameObject::current_might)
    uint32_t keywords = 0;                 // bit k: has Keyword k now (printed or granted)
    int assault = 0, shield = 0, deflect = 0;
    int damage = 0;
    bool lethal = false;                   // the next Cleanup kills it (the engine's lethal rule)
    bool exhausted = false;
    bool stunned = false;
    int buffs = 0;
    int empowered = 0;
    CombatDesignation designation = CombatDesignation::None;
    GameObjectId attached_to = kInvalidId; // a link
};

/// The combat or showdown state of one battlefield (T6).
struct BattlefieldFacts {
    BattlefieldId id = kInvalidId;
    CardDefId card = kInvalidId;
    std::optional<PlayerId> controller;
    bool contested = false;
    PlayerId contested_by = PlayerId::None;
    bool showdown_staged = false, showdown_in_progress = false;
    bool combat_staged = false, combat_in_progress = false;
    std::optional<PlayerId> attacker, defender;
    int facedown = 0;                      // cards facedown here
};

/// What one player can spend now (T6), by what it is spent for.
struct ResourceFacts {
    RunePool pool;                         // floating Energy / Power, restricted pools included
    int ready_runes = 0, exhausted_runes = 0;
    /// Index: SpendFor (Other, Unit, Spell, Gear). The most Energy, and the
    /// most Power of any Domain, the player could pay for that purpose now
    /// (resource::payableEnergy / payablePower: the engine's own test).
    int energy[4] = {};
    int power_any[4] = {};
    /// The most Power of each Domain (Domain::Count entries), for SpendFor::Other.
    std::vector<int> power_by_domain;
};

/// The timing window (T6).
struct TimingFacts {
    TurnPhase phase = TurnPhase::Setup;
    int turn_number = 0;
    PlayerId turn_player = PlayerId::None;
    PlayerId starting_player = PlayerId::None;
    NeutralShowdownState ns = NeutralShowdownState::Neutral;
    OpenClosedState oc = OpenClosedState::Open;
    std::optional<PlayerId> priority_holder;
    std::optional<PlayerId> focus_holder;
};

/// One chain item with its links (T4). Position 0 is the bottom; the item
/// being resolved (or finalized) right now is listed last with `resolving`.
struct ChainItemFacts {
    int position = 0;
    bool resolving = false;
    PlayerId controller = PlayerId::None;
    GameObjectId source = kInvalidId;      // link
    CardDefId card = kInvalidId;           // its card, if the observer may see it
    enum class Kind : uint8_t { Spell, Activated, Triggered, Keyword, Permanent, Equip } kind =
        Kind::Spell;
    int ability_index = 0;
    TriggerType trigger = TriggerType::None;
    bool finalized = false;
    std::vector<GameObjectId> targets;     // links: ChainItem::targets
    std::vector<GameObjectId> chosen;      // links: objects chosen at finalization
    int chosen_item = -1;                  // a counter's chosen item: its position, or -1
    GameObjectId subject = kInvalidId;     // link: the object whose event triggered it
};

class ObserverView {
public:
    /// `registry` is needed for the lethal rule's "any damage is lethal"
    /// cards; without it only CR 142.4 lethal damage counts.
    ObserverView(const GameState& state, PlayerId observer, const CardDB& db,
                 const CardRegistry* registry = nullptr);

    PlayerId observer() const { return observer_; }
    const knowledge::GameKnowledgeView& knowledge() const { return knowledge_; }

    // ── T4: what the observer may know ──
    /// Its own hand, sorted by card (knowledge::sortedHand).
    std::vector<GameObjectId> ownHand() const;
    /// A public pile in full, in the order cards entered it (Trash,
    /// Banishment): never truncated.
    const std::vector<GameObjectId>& publicPile(PlayerId owner, ZoneType zone) const;
    /// Cards of `owner`'s hand the observer knows (revealed, or seen going
    /// there) while they are still in that hand.
    std::vector<CardKnowledge> knownInHand(PlayerId owner) const;
    /// Hidden zone sizes are public.
    int handSize(PlayerId owner) const;
    int deckSize(PlayerId owner) const;
    int runeDeckSize(PlayerId owner) const;
    /// The chain, bottom first, then the item being resolved if any.
    std::vector<ChainItemFacts> chain() const;

    // ── T6: derived facts ──
    /// Every object on the board (and in the Legend Zones), in an order of
    /// what the observer sees (openspiel::visibleObjectDescription).
    std::vector<PermanentFacts> board() const;
    PermanentFacts permanent(GameObjectId id) const;
    std::vector<BattlefieldFacts> battlefields() const;
    ResourceFacts resources(PlayerId player) const;
    TimingFacts timing() const;
    /// The opponent's unseen cards may be (DeckModel::unseenPool).
    std::vector<DeckModel::PoolCard> opponentCardPool(const DeckModel& model) const;

    /// Everything above (and the scores, XP, counts, known deck and Rune Deck
    /// slots, compositions) as numbers with no object id: equal for two
    /// positions the observer cannot tell apart, different for two that differ
    /// in a fact it may know. Players are relative (1 = observer, 2 = the
    /// other, 0 = none).
    std::vector<int64_t> fingerprint() const;

private:
    int rel(PlayerId p) const { return p == PlayerId::None ? 0 : p == observer_ ? 1 : 2; }
    const GameState& state_;
    PlayerId observer_;
    const CardDB& db_;
    const CardRegistry* registry_;
    knowledge::GameKnowledgeView knowledge_;
};

}  // namespace riftbound
