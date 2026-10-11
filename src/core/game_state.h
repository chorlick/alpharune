#pragma once
/// @file game_state.h
/// Complete game state container.
///
/// GameState holds ALL mutable state for a single game in progress.
/// It is the single source of truth — every subsystem reads from and
/// writes to this structure.

#include "effects/effect_types.h"
#include "game_object.h"
#include "intent.h"
#include "knowledge_state.h"
#include "types.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

namespace riftbound {

// ─── Rune Pool ───────────────────────────────────────────────────────────────

struct RunePool {
    int energy = 0;
    // Energy that may be spent "only to play units or activated abilities of
    // units" (Butcher of the Sands). Counted and spent first when the engine
    // pays for one of those; invisible to every other payment.
    int unit_energy = 0;
    int power[static_cast<int>(Domain::Count)] = {};  // per-domain
    int universal_power = 0;  // [A] — any domain

    void clear() {
        energy = 0;
        unit_energy = 0;
        universal_power = 0;
        for (int i = 0; i < static_cast<int>(Domain::Count); ++i)
            power[i] = 0;
    }

    int totalPower() const {
        int sum = universal_power;
        for (int i = 0; i < static_cast<int>(Domain::Count); ++i)
            sum += power[i];
        return sum;
    }
};

// ─── Player State ────────────────────────────────────────────────────────────

struct PlayerState {
    PlayerId id = PlayerId::None;
    int score = 0;
    int xp = 0;
    // Count of this player's own turns taken so far (incremented at the start
    // of each of their Awaken Phases). 1 on their first turn. Drives
    // turn-gated rules like Forgotten Monument ("until their third turn").
    int turns_taken = 0;

    RunePool rune_pool;

    // Zone contents (game object IDs)
    std::vector<GameObjectId> main_deck;    // ordered (top = back)
    std::vector<GameObjectId> rune_deck;    // ordered (top = back)
    std::vector<GameObjectId> hand;
    std::vector<GameObjectId> trash;
    std::vector<GameObjectId> banishment;

    // This player's battlefield-deck candidates (CardDefIds, NOT
    // GameObjectIds — these aren't instantiated as runtime objects
    // until selection). Used during setup to offer a `ChooseBattlefield`
    // decision per CR 480.5 / 481.5, and consumed afterwards. The
    // renderer reads this to label the candidates by their printed
    // card name in the legal-action UI; the action-vocab encoder
    // uses the Intent's `chosen_battlefield` field (carrying the
    // index into this vector) for slot encoding.
    std::vector<CardDefId> battlefield_pool;

    // Special zones (single card or empty)
    GameObjectId champion_zone = kInvalidId;
    GameObjectId legend_zone = kInvalidId;
    // The object that is this player's Chosen Champion, wherever it is now;
    // champion_zone only says what currently sits in the zone.
    GameObjectId chosen_champion = kInvalidId;

    // Per-turn tracking
    int cards_played_this_turn = 0;
    int equipment_played_this_turn = 0;  // gears tagged "Equipment" played this turn
    int gears_played_this_turn = 0;      // any gear played this turn (Ornn's Forge)
    bool has_discarded_this_turn = false;
    std::set<BattlefieldId> battlefields_scored_this_turn;
    bool burned_out = false;
    bool is_first_turn = true;  // for first-turn adjustments
    bool cant_play_cards_this_turn = false;  // Brynhir Thundersong lockout, resets at turn end
    bool cant_play_spells_this_turn = false; // Lilting Lullaby lockout, resets at turn end
    // Unyielding Spirit [145] — "Prevent all spell and ability damage
    // this turn." Set by the spell's onResolve; consulted in
    // EffectExecutor::dealDamage to short-circuit when the damage's
    // source object is a spell or activated ability AND the target
    // belongs to this player. Resets in resetTurnTracking. Phase 6q+.
    bool prevent_spell_ability_damage_this_turn = false;
    // ── Wave B per-turn trackers (reset in resetTurnTracking) ──
    int power_spent_this_turn = 0;   // power (recycled runes) spent this turn (Sivir)
    int xp_gained_this_turn = 0;     // XP gained this turn (Wily Newtfish)
    int hold_points_this_turn = 0;   // points scored via Hold this turn (Needlessly Large Yordle)
    int draws_this_turn = 0;         // cards drawn this turn (Frigid Jewel "2nd card")
    // A friendly unit died during THIS player's Beginning Phase this turn (Shadow Watcher).
    bool unit_died_in_beginning_this_turn = false;
    // Turn-scoped effects of a resolved spell (reset in resetTurnTracking):
    bool units_enter_ready_this_turn = false;   // "Units you play this turn enter ready" (Confront, Bushwhack)
    bool unit_died_this_turn = false;           // a unit this player controlled died this turn (Spoils of War)
    int  buff_might_bonus_this_turn = 0;        // "Buffs give an additional +N [M] to friendly units this turn" (Stand United)
    bool hide_ignoring_costs_this_turn = false; // "You can hide cards ignoring costs this turn" (Guerilla Warfare)
    // Continuous-effect values (reset + recomputed during aura recompute; set by
    // cards' applyPassiveAura — default = no effect):
    bool cannot_gain_points = false; // Tianna Crownguard ("opponents can't gain points")
    int bonus_damage_dealt = 0;      // Annie, Fiery ("your spells/abilities deal +N")
    bool grant_friendly_units_open_bf = false; // Miss Fortune, Buccaneer ("friendly units may be played to open battlefields")
    bool units_play_base_only = false;         // set on the RESTRICTED player — Mageseeker Warden ("opponents can only play units to their base")
    bool tokens_enter_ready = false;           // Renata Glasc, Industrialist ("your tokens enter ready")
    // Magma Wurm ("Other friendly units enter ready"): the on-board units of
    // this player that say so. A unit this player plays enters ready if any
    // of them is another object (otherUnitsEnterReady).
    std::vector<GameObjectId> units_enter_ready_by;
    bool otherUnitsEnterReady(GameObjectId entering) const {
        for (auto src : units_enter_ready_by) if (src != entering) return true;
        return false;
    }
    bool recall_all_on_attacker_tie = false;   // Symbol of the Solari (227): attacker-side combat tie recalls ALL units
    bool effects_cant_ready_my_units = false;  // set on the RESTRICTED player — Mageseeker Warden cl2 ("spells/abilities can't ready enemy units/gear")
    int  spells_have_repeat_energy = 0;        // Syndra, Transcendent (708): granted [Repeat] tranche cost (energy)
    int  spells_have_repeat_power = 0;         // ... and power
    Domain spells_have_repeat_domain = Domain::Chaos; // ... and its domain ([P] default)
    int  repeat_cost_reduction = 0;            // Marai Spire (525): friendly [Repeat] costs cost N less
    bool has_reveal_peek = false;              // Void Hatchling (341): peek/recycle top before revealing
    bool zilean_present = false;               // Zilean, Time Mage (648): a Zilean is at one of my battlefields (aura)
    // Turn-scoped riders (NOT aura-derived — reset in resetTurnTracking):
    int max_spell_spent_this_turn = 0;         // most-expensive single spell paid for this turn (Jhin, Meticulous Killer: "spent [4]+ to play a spell")
    int next_spell_bonus_damage = 0;           // Ravenborn Tome: "the next spell you play this turn deals N Bonus Damage" (consumed when that spell deals damage)
    bool grant_repeat_base_to_next_spell = false; // The Academy (772): next spell gets [Repeat] at tranche cost = its base energy
    int zilean_double_token_turn = -1;         // Zilean, Time Mage (648): turn on which the once/turn token-doubling was used
    // Staged around an effect-driven play that ignores part of a card's cost
    // ("ignoring its Energy cost" — you still pay its Power). Read by
    // canAfford / beginCostPayment; always cleared by the caller.
    int transient_power_discount = 0;   // staged power discount for the card being played (Sandswept Tomb)
    bool transient_ignore_energy = false;
    bool transient_ignore_power = false;
    int transient_play_discount = 0;           // Irelia, Graceful (462): energy discount staged just before paying a specific spell (set+consumed in executePlaySpell)
    // Phase 6q+ engine-audit follow-on: reset last_spell_energy_spent
    // at turn start. Pre-fix, a Virtuoso/Forgotten Library trigger that
    // fires off a turn-N spell could be delayed (via chain priority)
    // and read a stale last_spell_energy_spent value set in turn N-1
    // if no spell has been played yet this turn. Resetting in
    // resetTurnTracking guarantees the field reads 0 until the first
    // play of the turn writes it.

    // Energy spent on the most recently played spell by this player. Set
    // by the engine in executePlaySpell at CardPlayedEvent emit time.
    // Forgotten Library [767] / Virtuoso [782] read this in their
    // WhenYouPlayASpell trigger to gate their "if you spent [4] or more"
    // effect — by the time their ability resolves on the chain, the
    // triggering spell is in trash so its base def is reachable, but if
    // Repeat was paid the actual spent amount > base. This field captures
    // the play-time amount.
    int last_spell_energy_spent = 0;

    // Cost modifiers (Phase 5)
    struct CostModifier {
        GameObjectId source = kInvalidId;  // card granting the reduction
        int energy_reduction = 0;          // reduce energy cost by this
        int energy_increase = 0;           // ADD to energy cost (Vaults of Helia "cost [1] more")
        int min_cost = 0;                  // minimum cost after reduction (0 = no min)
        bool this_turn_only = true;        // expires at end of turn
        bool next_spell_only = false;      // expires after next spell played
        bool next_unit_only = false;       // expires after next unit played
        bool next_card_only = false;       // expires after the next card of ANY type (Astral Heron)
        int power_reduction = 0;           // power component of the discount ([A] less)
        bool affects_non_token_only = false; // restricts the modifier to non-token cards
        // Combat-active scope (Vex Cheerless: "While I'm in combat,
        // friendly spells cost [1] less and enemy spells cost [1] more").
        // Set TRUE at combat start and FALSE at combat end via
        // GameEngine's EventBus subscribers. canAfford / payCardCost
        // skip modifiers with combat_active_only=true when the engine
        // is not currently in a combat showdown.
        bool combat_active_only = false;
        bool affects_friendly_only = false; // restricts to plays by the modifier's controller
        bool affects_enemy_only = false;    // restricts to plays by the opponent
        bool gear_only = false;             // applies only to gear plays (Ornn's Forge)
        bool units_only = false;            // applies only to unit plays (Vaults of Helia)
        std::string required_tag;           // applies only to cards with this tag (Herald of Scales: "Dragon")
        // Pushed by a Card::applyPassiveAura hook, i.e. a continuous effect.
        // Set by GameEngine::recalculateAuras (cards do not set it), which
        // drops and rebuilds these on every aura pass.
        bool from_aura = false;
        bool first_gear_per_turn = false;   // applies only to the FIRST gear played this
                                            // turn (consult gears_played_this_turn == 0)
    };
    std::vector<CostModifier> cost_modifiers;

    // Trash-replay grants (CR: "You may play this from your trash for <cost>").
    // Death from Below pushes one of these after it resolves; the action
    // generator offers the specific trash card with the OVERRIDE cost (not its
    // printed cost), and executePlaySpell pays the override and consumes the
    // grant. Cleared each turn — the permission does not persist.
    struct TrashReplayGrant {
        GameObjectId card = kInvalidId;   // the specific card object (in trash)
        int energy = 0;
        int power = 0;
        Domain power_domain = Domain::Fury;
        bool any_domain = false;          // [A] — power may be any single domain
        bool banish_after = false;        // granted [Flow]: banish the spell after it's played
    };
    std::vector<TrashReplayGrant> trash_replay_grants;

    // Karthus, Eternal (and similar): "Your Deathknell effects trigger
    // an additional time." Counted from board via aura recalculation —
    // bumped per controlled Karthus. TriggerManager::onUnitDied adds
    // (1 + deathknell_double_count) chain items for any friendly card
    // with a Deathknell trigger. 0 = normal (single fire).
    int deathknell_double_count = 0;

    // Additional turns queue (Phase 5)
    std::vector<PlayerId> additional_turns;

    // ── Observation memory ("revealed card bank") ─────────────────────
    // Counts how many times this player has observed each card identity
    // via reveals (Sabotage, Mindsplitter, Aurora, Vision, Predict-public,
    // facedown flips, etc.). Keyed by CardDefId so the size is sparse.
    // Updated by GameEngine's CardRevealedEvent subscriber when:
    //   • revealed_to_all == true (every player learns the identity), OR
    //   • revealed_to == this player's id (private observation).
    // Note: card_def_id is the static registry index, not a runtime
    // GameObjectId. Two reveals of the same registry identity (e.g.
    // seeing Brazen Buccaneer twice in different turns) increment the
    // same slot. Not yet featurized for the ML trainer — see Phase B-2
    // "Observation tracking" in CLAUDE.md for the trainer wire-up plan.
    std::map<CardDefId, int> observed_cards;

    // Transient: source of the play currently being processed (set by
    // executePlayCard before payCardCost, cleared after). Lets the cost
    // path consult play_source without threading it through every
    // intermediate signature. Default Hand for resetTurnTracking.
    // Consumed by Rek'Sai Breacher's auto-Accelerate path.
    Intent::PlaySource current_play_source = Intent::PlaySource::Hand;

    // ── Vendetta ─────────────────────────────────────────────────────────
    int cards_burned_this_turn = 0;       // cards this player Burned this turn (CR 441)
    int cards_banished_this_turn = 0;     // owned cards banished this turn
    bool chose_enemy_unit_this_turn = false;   // Hungry Wolf

    // This player's most recent combat this turn: where it was, whether they
    // attacked, and how much combat damage they assigned beyond what was
    // needed for lethal ("if you assigned N or more excess damage").
    // last_combat_bf is kInvalidId when they have not been in a combat this turn.
    BattlefieldId last_combat_bf = kInvalidId;
    bool last_combat_attacker = false;
    int excess_combat_damage = 0;
    bool played_nontoken_unit_this_turn = false;  // Swain, Visionary
    bool played_nontoken_gear_this_turn = false;
    bool played_spell_this_turn = false;
    int cards_played_not_from_hand_this_turn = 0;
    int spells_played_this_turn = 0;      // Crumbling Sands
    int gear_abilities_activated_this_turn = 0;   // Piltovan Forge ("first ... each turn")
    int abilities_activated_this_turn = 0;
    // Continuous-effect flags/counters below are recomputed by
    // recalculateAuras (reset there, then set by cards' applyPassiveAura).
    bool skip_draw_phase = false;         // Endless Riches: "Skip your Draw Phase."
    bool may_play_from_trash = false;     // Endless Riches: "You may play cards from your trash."
    bool trash_goes_to_banish = false;    // Endless Riches: non-deck cards headed to trash are banished
    int flow_cost_reduction = 0;          // Stargazer: Flow plays cost [N] less (min [1])
    int spell_tax_energy = 0;             // Helm of Suppression: THIS player's spells cost more
    int spell_tax_power = 0;
    int spell_discount_energy = 0;        // Applied Researchers: your spells cost less (min [1])
    int spell_discount_power = 0;
    bool spells_cant_be_countered = false;  // Mel, Newly Awakened
    int extra_minus_might = 0;            // Mel, Newly Awakened: your -[M] effects give extra
    std::vector<std::string> locked_spell_names;  // Fallen Feline: this player can't play these
    int first_gear_ability_discount = 0;  // Piltovan Forge: first friendly gear ability each turn costs [N] less

    void resetTurnTracking() {
        cards_burned_this_turn = 0;
        cards_banished_this_turn = 0;
        chose_enemy_unit_this_turn = false;
        last_combat_bf = kInvalidId;
        last_combat_attacker = false;
        excess_combat_damage = 0;
        played_nontoken_unit_this_turn = false;
        played_nontoken_gear_this_turn = false;
        played_spell_this_turn = false;
        cards_played_not_from_hand_this_turn = 0;
        spells_played_this_turn = 0;
        gear_abilities_activated_this_turn = 0;
        abilities_activated_this_turn = 0;
        cards_played_this_turn = 0;
        equipment_played_this_turn = 0;
        gears_played_this_turn = 0;
        has_discarded_this_turn = false;
        battlefields_scored_this_turn.clear();
        cant_play_cards_this_turn = false;
        cant_play_spells_this_turn = false;
        prevent_spell_ability_damage_this_turn = false;
        last_spell_energy_spent = 0;
        max_spell_spent_this_turn = 0;
        next_spell_bonus_damage = 0;
        grant_repeat_base_to_next_spell = false;
        transient_play_discount = 0;
        power_spent_this_turn = 0;
        xp_gained_this_turn = 0;
        hold_points_this_turn = 0;
        draws_this_turn = 0;
        unit_died_in_beginning_this_turn = false;
        units_enter_ready_this_turn = false;
        unit_died_this_turn = false;
        buff_might_bonus_this_turn = 0;
        hide_ignoring_costs_this_turn = false;
        // (cannot_gain_points / extra_points_to_win are continuous-effect flags,
        //  recomputed during aura cleanup — not per-turn counters.)
        // Expire this-turn cost modifiers
        cost_modifiers.erase(
            std::remove_if(cost_modifiers.begin(), cost_modifiers.end(),
                [](const CostModifier& m) { return m.this_turn_only; }),
            cost_modifiers.end());
        // Trash-replay permissions do not persist across turns.
        trash_replay_grants.clear();
    }
};

// ─── Battlefield State ───────────────────────────────────────────────────────

struct BattlefieldState {
    BattlefieldId id = kInvalidId;
    GameObjectId card_object_id = kInvalidId;  // the battlefield card itself

    std::optional<PlayerId> controller;
    // ── Vendetta battlefield continuous effects (recomputed each recalculateAuras) ──
    bool ignore_tank[2] = {false, false};   // Dune Surfer: this player ignores [Tank] assigning here
    bool ignore_deflect_here = false;       // Heisho: players ignore [Deflect] choosing something here
    int reaction_tax_power = 0;             // Mystic Vortex: [Reaction] cards cost [A] more in showdowns here
    int friendly_spell_discount_power = 0;  // Sandswept Tomb: spells choosing friendly units here cost [A] less
    // "Any player may pay [A]... as an additional cost to play a <tag>. If they
    // do, they play it to this battlefield." (Dragon Roost). 0 = no such offer.
    int paid_play_power = 0;
    std::string paid_play_tag;
    int empower_cost_reduction[2] = {0, 0}; // Risen Altar: per-player [Empower] discount for units here
    bool is_contested = false;
    PlayerId contested_by = PlayerId::None;
    PlayerId contributed_by = PlayerId::None;  // who brought this BF to the game

    // Facedown sub-zone
    std::vector<GameObjectId> facedown;
    int facedown_max_occupancy = 1;

    // Staged events
    bool showdown_staged = false;
    bool combat_staged = false;

    // Active combat/showdown
    bool showdown_in_progress = false;
    bool combat_in_progress = false;
    std::optional<PlayerId> attacker;
    std::optional<PlayerId> defender;
    CombatPhase combat_phase = CombatPhase::None;

    // Token tracking (CR 438, 439)
    bool is_token = false;
    std::optional<GameObjectId> replaced_card;  // original in banishment
    bool was_replaced = false;

    // Movement-rule relaxer. Baron Pit's "Units can move here from
    // anywhere" sets this to true, allowing any unit at any other
    // battlefield to move here without needing Ganking. The main-phase
    // move generator honors this flag in addition to the standard
    // base→BF and Ganking BF→BF rules.
    bool accepts_any_inbound = false;

    // Movement-rule restriction (Vilemaw's Lair [290]). When true, units
    // at this BF cannot move BACK to their controller's base. They may
    // still move to other battlefields (with Ganking). Honored by
    // GameEngine::generateMainPhaseActions when emitting BF→Base moves.
    bool blocks_move_to_base = false;

    // Play-location restriction (Rockfall Path [530]). When true, no
    // unit/gear may be played to this battlefield. Honored by play-
    // location enumeration in the main-phase / showdown action
    // generators.
    bool blocks_unit_play = false;

    // Turn-gated scoring (Forgotten Monument [523]: "Players can't score here
    // until their third turn"). A player may only score this BF once their
    // PlayerState::turns_taken >= min_turn_to_score. 0 = no restriction.
    // Populated from BattlefieldCard::minTurnToScore() at setup.
    int min_turn_to_score = 0;

    // Last-conquered tracking (Perched Grimwyrm 338: "play me only to a
    // battlefield you conquered this turn"). Stamped in GameEngine::scoreConquer
    // with the current turn number + conquering player; consumers compare
    // against the live turn so the marker auto-expires next turn (no reset).
    int conquered_on_turn = -1;
    PlayerId conquered_by_player = PlayerId::None;

    // Aura-derived per-BF flags (reset + recomputed each recalculateAuras by the
    // BF/unit cards' applyPassiveAura):
    bool surcharge_enemy_multi_move = false; // Mageseeker Investigator (725): enemy multi-unit moves here cost [A]/extra unit
    // Noxus Saboteur (018): "Your opponents' [Hidden] cards can't be revealed
    // here." [p] is set for the player whose facedown cards here can't be
    // turned face up, i.e. can't be played from facedown (CR 811).
    bool opp_hidden_unrevealable[2] = {false, false};

    /// Get all unit IDs at this battlefield belonging to a player.
    std::vector<GameObjectId> unitsControlledBy(
        PlayerId player,
        const std::unordered_map<GameObjectId, GameObject>& objects) const;

    /// Check if any player has units here.
    bool hasUnitsFrom(
        PlayerId player,
        const std::unordered_map<GameObjectId, GameObject>& objects) const;
};

// ─── Turn State ──────────────────────────────────────────────────────────────

struct TurnState {
    PlayerId turn_player = PlayerId::Player1;
    PlayerId starting_player = PlayerId::Player1;  // who won the coin toss
    TurnPhase phase = TurnPhase::Setup;
    NeutralShowdownState ns_state = NeutralShowdownState::Neutral;
    OpenClosedState oc_state = OpenClosedState::Open;

    std::optional<PlayerId> priority_holder;
    std::optional<PlayerId> focus_holder;

    int turn_number = 0;
    bool is_additional_turn = false;

    // True once any unit has died during the current turn (reset each runTurn).
    // Read by cards like Towering Pairofant ("if a unit died this turn, ...").
    bool any_unit_died_this_turn = false;

    // Points needed to win beyond the mode's Victory Score, from battlefields
    // in play (Card::victoryScoreModifier). Recomputed each recalculateAuras,
    // for code that has only the state to read ("within 3 points of the
    // Victory Score" in a cost reduction): mode.victory_score + this.
    int victory_score_bonus = 0;

    // Sandstone Chimera: "players only channel 1 rune at the start of their
    // Channel Phase." 0 = no cap. Recomputed each recalculateAuras.
    int channel_phase_cap = 0;
    // Otterpus: "If a player would score 1 point from conquering or holding
    // during their first or second turn, they draw 1 instead." Recomputed each
    // recalculateAuras.
    bool early_score_draws_instead = false;

    // Per-turn decision counters (reset each turn)
    int turn_decisions_p1 = 0;
    int turn_decisions_p2 = 0;

    // Priority tracking for chain resolution
    std::set<PlayerId> players_passed_priority;
    std::set<PlayerId> players_passed_focus;

    bool isNeutralOpen() const {
        return ns_state == NeutralShowdownState::Neutral &&
               oc_state == OpenClosedState::Open;
    }

    bool isShowdownOpen() const {
        return ns_state == NeutralShowdownState::Showdown &&
               oc_state == OpenClosedState::Open;
    }

    bool isClosedState() const {
        return oc_state == OpenClosedState::Closed;
    }
};

// ─── Chain State ─────────────────────────────────────────────────────────────

enum class ChainItemStatus : uint8_t { Pending, Finalized };

struct ChainItem {
    ChainItemId id = kInvalidId;
    ChainItemStatus status = ChainItemStatus::Pending;
    GameObjectId source = kInvalidId;      // the spell/ability game object
    CardDefId card_def_id = kInvalidId;    // for looking up effects
    PlayerId controller = PlayerId::None;

    // Targeting
    std::vector<GameObjectId> targets;
    // Everything this item has chosen so far: play-time targets plus objects
    // picked at resolution (Card::pickTarget / pickTargetPair). Lets other
    // effects ask "what does this chain item choose" (Deflect, Irelia,
    // Sandswept Tomb, Decree of Focus, Repulse) for resolve-time-target cards.
    std::vector<GameObjectId> chosen_objects;

    // Classification
    bool is_spell = false;        // true for spells (go to trash on resolve)
    bool is_permanent = false;    // true for units/gear (resolve on finalize, CR 337.1.c)
    bool is_ability = false;      // true for activated/triggered abilities

    /// Distinguishes activated abilities (player chose to activate, e.g.
    /// [E]: <effect>) from triggered abilities (event fired, e.g. "When I
    /// play"). When `is_ability && is_activated_ability`, the engine
    /// dispatches resolution through Card::onActivate. When
    /// `is_ability && !is_activated_ability`, dispatch goes through
    /// Card::onTrigger. Cards must override the matching method; the
    /// other defaults to a no-op.
    bool is_activated_ability = false;

    // Which trigger event produced this (triggered) ability, so multi-trigger
    // cards can branch in onTrigger via CardContext::firing_trigger. None for
    // spells / activated abilities / single-trigger cards.
    TriggerType fired_trigger = TriggerType::None;

    // Phase 6r — multi-ability per Card. Identifies which of the source's
    // activated abilities this chain item resolves. 0 = first/only ability
    // (back-compat). Only meaningful when is_activated_ability is true.
    int ability_index = 0;

    // Aura-granted activated ability (Forge/Gardens/Heimerdinger): nonzero = this
    // ActivateAbility runs the GRANTED ability whose logic lives on card def
    // `granted_ability_def`, dispatched with `source` (the bearer) as the actor.
    // 0 = the source's own ability. Copied from Intent::granted_ability_def when
    // the activation is enqueued.
    CardDefId granted_ability_def = 0;

    // Phase C-1 commit 6 — resumable resolution.
    // `resume_point == 0` means "fresh — Card::onResolve has not run on this
    // ChainItem yet." Values >0 are card-specific saved-progress markers.
    // `resume_data` is a generic int32 scratchpad — cards interpret it
    // however they like (e.g. discard slot index, predict outcome flags).
    // These fields are unused until a Card's onResolve has been refactored
    // to be re-entrant (see CLAUDE.md "Phase C-1 commit 6 — resume pattern"
    // for the refactor template). Cards that resolve in one shot ignore
    // them entirely.
    int resume_point = 0;
    std::vector<int32_t> resume_data;

    // ── Choices made at finalization (CR 355, 383.3) ──
    // A card that opts in (Card::choicesAtFinalization) has its callback run
    // once when the item is finalized, with `finalizing` set: the choice
    // helpers ask the agent as usual and the card returns before its effect
    // (Card::finalizingChoices). The answers stay on the item; when it
    // resolves (`choices_finalized`) the helpers return them without asking.
    bool finalizing = false;
    bool choices_finalized = false;
    // A cost was paid during the finalize pass (Card::costDueNow), so it is
    // not charged again at resolution.
    bool finalize_costs_paid = false;
    // confirmOptional's answer: -1 not asked, 0 no, 1 yes, 2 yes and
    // re-validated at resolution.
    int8_t may_answer = -1;
    // Finalized targets are checked for legality once as the item resolves
    // (a target that is no longer legal is a mistarget). 1 = single, 2 = pair.
    uint8_t finalize_checked = 0;

    // ── Repeat (CR 820) — optional additional cost ──
    // `repeats_paid` is the number of times the Repeat cost was paid as
    // the spell was played. The spell's effect resolves `1 + repeats_paid`
    // times. Currently the play-action generator does NOT emit Repeat
    // options to agents (engine gap); tests and ad-hoc invocations can
    // set this directly. `total_energy_spent` is the sum of base energy
    // cost plus any Repeat-energy increments — used by Forgotten Library
    // (CR-text "if you spent [4] or more") and Virtuoso for cost-gating.
    int repeats_paid = 0;
    int total_energy_spent = 0;

    // Phase 6q+ — snapshot of the TRIGGERING spell's total_energy_spent
    // when this trigger was added to the chain. Used by WhenYouPlaySpell
    // triggers (Virtuoso, Forgotten Library) so they don't read the
    // stale/clobbered PlayerState::last_spell_energy_spent at resolution
    // time. CR-correct semantics: the triggered ability evaluates
    // "if you spent [N] or more" against the SPECIFIC spell that
    // triggered it, not the most-recently-played-anywhere spell.
    // 0 means "not a play-triggered ability" (or pre-snapshot trigger).
    int triggering_spell_energy_spent = 0;

    // Snapshot of the object that a "when you <verb> a friendly unit" trigger
    // acted on (the buffed/readied/moved unit), so the firing card can do "... to
    // it". kInvalidId when the trigger has no relevant subject.
    GameObjectId triggering_subject = kInvalidId;

    // Set for an Equipment's "When I ..." ability (its effect text, speaking
    // for the unit it is attached to): the unit the event happened to. Such an
    // item is a triggered ability whose source is the gear; it resolves
    // through Card::onEquippedTrigger(ctx, equipped_unit, ...) instead of
    // Card::onTrigger. kInvalidId for every other item.
    GameObjectId equipped_unit = kInvalidId;
};

struct ChainState {
    std::vector<ChainItem> items;
    ChainItemId next_id = 1;

    // Phase C-1 commit 6 — the chain item currently being resolved.
    // Populated by ChainManager::stepResolve immediately after popping the
    // top item from `items`; cleared after disposal. Holds the resolving
    // ChainItem while the resumable resolution loop is calling onResolve.
    // Cards that yield mid-resolve (call EffectExecutor::requestChoice +
    // return) read/write `resuming->resume_point` to track their saved
    // progress across re-entry. Counter spells continue to use
    // `items.back()` to find their target — `resuming` is a separate slot.
    std::optional<ChainItem> resuming;

    bool exists() const { return !items.empty(); }

    ChainItemId allocateId() { return next_id++; }

    /// Get the newest (top) item on the chain (last added = resolves first).
    ChainItem* newest() {
        return items.empty() ? nullptr : &items.back();
    }

    const ChainItem* newest() const {
        return items.empty() ? nullptr : &items.back();
    }

    /// Check if any items are still pending.
    bool hasPending() const {
        for (auto& item : items)
            if (item.status == ChainItemStatus::Pending) return true;
        return false;
    }
};

// ─── Delayed Abilities (CR 389-392) ─────────────────────────────────────────

struct DelayedAbility {
    GameObjectId source = kInvalidId;     // card that created this
    CardDefId card_def_id = kInvalidId;   // for looking up the Card object
    PlayerId controller = PlayerId::None;
    TriggerType trigger = TriggerType::None; // what event fires it
    int expires_on_turn = -1;             // -1 = end of current turn
    bool fired = false;                   // one-shot: fire once, then remove

    // Optional filter for events that carry an object id (e.g.
    // UnitDiedEvent::object). Set to a specific GameObjectId to narrow
    // the delayed ability to fire only when THAT object is the event
    // subject (Deadly Flourish: "when IT dies"). kInvalidId = no filter,
    // fires on any matching trigger.
    GameObjectId target_filter = kInvalidId;

    // Fires every time its event happens until it expires, instead of once
    // ("When ANY unit takes damage this turn, kill it" — Imperial Decree).
    bool every_time = false;
};

// ─── Complete Game State ─────────────────────────────────────────────────────

struct GameState {
    // Configuration
    ModeOfPlay mode;

    // Players (indexed by playerIndex())
    PlayerState players[2];

    // Board
    std::vector<BattlefieldState> battlefields;

    // All game objects (the object pool)
    std::unordered_map<GameObjectId, GameObject> objects;
    GameObjectId next_object_id = 1;

    // Turn
    TurnState turn;

    // Chain
    ChainState chain;

    // What each player has learned about hidden cards. Written only through
    // engine/knowledge_tracker.h, read only through KnowledgeView.
    KnowledgeState knowledge;

    // Delayed abilities (Phase 5, CR 389-392)
    std::vector<DelayedAbility> delayed_abilities;

    // Game result
    bool game_over = false;
    PlayerId winner = PlayerId::None;
    std::string game_over_reason;

    // Decision counter (for training data indexing)
    int decision_index = 0;

    // One history token per decision applied to this game, in order
    // (openspiel/action_vocab.h: the vocab slot plus a fingerprint that
    // identifies the chosen Intent). Written only by
    // GameEngine::recordAppliedIntent, in runGame and step-machine games
    // alike. Replaying the tokens from the same decks and seed rebuilds the
    // game exactly; lives here so a state copy carries its own history.
    std::vector<int64_t> action_history;

    // ── Cost payment cursor (Phase C-1 commit 8) ──
    // Saves the rune-pick progress across step-machine yields.
    // Populated when payCardCost has yielded a NeedDecision and
    // is waiting for the agent's rune choice. Cleared when cost
    // is fully paid (or aborted). Memcpy-clone safe (POD).
    struct CostPaymentCursor {
        GameObjectId card        = kInvalidId;
        PlayerId     player      = PlayerId::None;
        int          energy_remaining = 0;
        int          power_remaining  = 0;
        // Domains the card can recycle for power. Snapshotted at begin
        // time so we don't have to re-read CardDef each advance.
        std::vector<Domain> power_domains;
        enum class Phase : uint8_t { PayingEnergy, PayingPower, Done };
        Phase phase = Phase::Done;
    };
    std::optional<CostPaymentCursor> cost_cursor;

    // ── Object management ──

    GameObjectId createObject() {
        GameObjectId id = next_object_id++;
        objects[id] = GameObject{};
        objects[id].id = id;
        return id;
    }

    // Checked in every build type: an unknown id throws std::logic_error
    // instead of dereferencing end() when NDEBUG removes the assert.
    GameObject& getObject(GameObjectId id) {
        auto it = objects.find(id);
        if (it == objects.end()) [[unlikely]] throwInvalidObjectId(id);
        return it->second;
    }

    const GameObject& getObject(GameObjectId id) const {
        auto it = objects.find(id);
        if (it == objects.end()) [[unlikely]] throwInvalidObjectId(id);
        return it->second;
    }

    [[noreturn]] static void throwInvalidObjectId(GameObjectId id);

    bool objectExists(GameObjectId id) const {
        return objects.find(id) != objects.end();
    }

    // ── Player access ──

    PlayerState& player(PlayerId id) {
        return players[playerIndex(id)];
    }

    const PlayerState& player(PlayerId id) const {
        return players[playerIndex(id)];
    }

    // ── Observation tracking ──────────────────────────────────────────
    //
    // Record a CardRevealedEvent into the observers' observed_cards
    // memory banks. Shared between GameEngine's subscriber and unit
    // tests (which don't construct an engine but still need to verify
    // memory-bank updates).
    //
    // Definition is inline in the header so tests + engine can share
    // it without a translation-unit dance. Forward-declared event type
    // would be cleaner; we accept the include here.
    template <class Event>
    void recordReveal(const Event& e) {
        if (e.card_def_id == kInvalidId) return;
        auto bump = [&](PlayerId obs) {
            if (obs == PlayerId::None) return;
            ++player(obs).observed_cards[e.card_def_id];
        };
        if (e.revealed_to_all) {
            bump(PlayerId::Player1);
            bump(PlayerId::Player2);
        } else {
            bump(e.revealed_to);
        }
    }

    // ── Board queries ──

    /// Find all game objects at a specific location.
    std::vector<GameObjectId> objectsAt(LocationId loc) const;

    /// Find all units at a location controlled by a player.
    std::vector<GameObjectId> unitsAt(LocationId loc, PlayerId controller) const;

    /// Find all units on the board controlled by a player (base + battlefields).
    std::vector<GameObjectId> allUnitsControlledBy(PlayerId controller) const;

    /// Find all objects in a player's base.
    std::vector<GameObjectId> objectsInBase(PlayerId player) const;

    /// Find all runes in a player's base (channeled runes).
    std::vector<GameObjectId> runesInBase(PlayerId player) const;

    /// Get the number of battlefields currently on the board.
    size_t totalBattlefields() const { return battlefields.size(); }
};

} // namespace riftbound
