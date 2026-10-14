#pragma once
/// @file game_state.h
/// Complete game state container.
///
/// GameState holds ALL mutable state for a single game in progress.
/// It is the single source of truth — every subsystem reads from and
/// writes to this structure.

#include "effects/effect_types.h"
#include "game_object.h"
#include "object_table.h"
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

/// What a payment is for. Restricted resources ("Use only to play spells")
/// can pay only for the matching purpose.
enum class SpendFor : uint8_t {
    Other,   // anything else (a legend's or battlefield's ability, a Hide cost, ...)
    Unit,    // playing a unit, or an activated ability of a unit
    Spell,   // playing a spell
    Gear,    // playing a gear, or an activated ability of a gear
};

struct RunePool {
    int energy = 0;
    // Energy that may be spent "only to play units or activated abilities of
    // units" (Butcher of the Sands). Counted and spent first when the engine
    // pays for one of those; invisible to every other payment.
    int unit_energy = 0;
    // The same for the other restrictions cards print. Every payment goes
    // through engine/resource_payment.h, which spends restricted resources
    // before unrestricted ones.
    int spell_energy = 0;      // "Use only to play spells." (Lux, Crownguard)
    int showdown_energy = 0;   // "Spend this Energy only during showdowns." (Scorn of the Moon)
    int power[static_cast<int>(Domain::Count)] = {};  // per-domain
    int universal_power = 0;  // [A] — any domain
    int spell_power = 0;       // [A], "Use only to play spells." (Daughter of the Void)
    int gear_power = 0;        // [A], "Use only to play gear or use gear abilities."

    void clear() {
        energy = 0;
        unit_energy = 0;
        spell_energy = 0;
        showdown_energy = 0;
        universal_power = 0;
        spell_power = 0;
        gear_power = 0;
        for (int i = 0; i < static_cast<int>(Domain::Count); ++i)
            power[i] = 0;
    }

    int totalPower() const {
        int sum = universal_power + spell_power + gear_power;
        for (int i = 0; i < static_cast<int>(Domain::Count); ++i)
            sum += power[i];
        return sum;
    }

    /// Restricted Energy usable for a payment of kind `f` right now.
    int restrictedEnergy(SpendFor f, bool in_showdown) const {
        return (f == SpendFor::Unit ? unit_energy : 0) +
               (f == SpendFor::Spell ? spell_energy : 0) +
               (in_showdown ? showdown_energy : 0);
    }
    /// Restricted [A] usable for a payment of kind `f`.
    int restrictedPower(SpendFor f) const {
        return (f == SpendFor::Spell ? spell_power : 0) +
               (f == SpendFor::Gear ? gear_power : 0);
    }
    /// Spend up to `need` restricted Energy / Power usable for `f`; `need` is
    /// reduced by what was spent.
    void takeRestrictedEnergy(SpendFor f, bool in_showdown, int& need) {
        auto take = [&need](int& from) {
            int n = std::min(from, std::max(0, need));
            from -= n;
            need -= n;
        };
        if (f == SpendFor::Unit) take(unit_energy);
        if (f == SpendFor::Spell) take(spell_energy);
        if (in_showdown) take(showdown_energy);
    }
    void takeRestrictedPower(SpendFor f, int& need) {
        auto take = [&need](int& from) {
            int n = std::min(from, std::max(0, need));
            from -= n;
            need -= n;
        };
        if (f == SpendFor::Spell) take(spell_power);
        if (f == SpendFor::Gear) take(gear_power);
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
    // card name in the legal-action UI; Intent::battlefield_pool_index
    // is an index into this vector.
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
    int draws_before_last_draw = 0;  // draws_this_turn before the latest draw event (it may draw several)
    // A friendly unit died during THIS player's Beginning Phase this turn (Shadow Watcher).
    bool unit_died_in_beginning_this_turn = false;
    // Turn-scoped effects of a resolved spell (reset in resetTurnTracking):
    bool units_enter_ready_this_turn = false;   // "Units you play this turn enter ready" (Confront, Bushwhack)
    bool unit_died_this_turn = false;           // a unit this player controlled died this turn (Spoils of War)
    // How many units this player controlled died this turn, and how many of
    // those during their own Beginning Phase: "the first time a friendly unit
    // dies (during your Beginning Phase) each turn" counts the turn's deaths,
    // not the card's own triggers (CR 383.1; Wraith of Echoes, Shard of
    // Undoing). Counted before the death is announced.
    int unit_deaths_this_turn = 0;
    int unit_deaths_in_beginning_this_turn = 0;
    int  buff_might_bonus_this_turn = 0;        // "Buffs give an additional +N [M] to friendly units this turn" (Stand United)
    bool hide_ignoring_costs_this_turn = false; // "You can hide cards ignoring costs this turn" (Guerilla Warfare)
    // Continuous-effect values (reset + recomputed during aura recompute; set by
    // cards' applyPassiveAura — default = no effect):
    bool cannot_gain_points = false; // Tianna Crownguard ("opponents can't gain points")
    int bonus_damage_dealt = 0;      // Annie, Fiery ("your spells/abilities deal +N")
    bool grant_friendly_units_open_bf = false; // Miss Fortune, Buccaneer ("friendly units may be played to open battlefields")
    // "Friendly units can be played to an occupied battlefield if an enemy unit
    // is alone there" (Arachnoid Horror): read by the unit-play generator for
    // every unit this player plays.
    bool grant_units_lone_enemy_bf = false;
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
    // "Once each turn, if you would play a token unit ..., you may play that
    // token and an additional copy of it instead." How many cards in play give
    // this player that right now (rebuilt every aura pass by each such card's
    // applyPassiveAura: one per instance whose condition holds), and how many
    // of them have been used on turn `additional_token_copies_turn`. Each is
    // "once each turn" on its own. Read by EffectExecutor::createToken.
    int additional_token_copy_sources = 0;
    // Turn-scoped riders (NOT aura-derived — reset in resetTurnTracking):
    int max_spell_spent_this_turn = 0;         // most-expensive single spell paid for this turn (Jhin, Meticulous Killer: "spent [4]+ to play a spell")
    int next_spell_bonus_damage = 0;           // Ravenborn Tome: "the next spell you play this turn deals N Bonus Damage" (consumed when that spell deals damage)
    bool grant_repeat_base_to_next_spell = false; // The Academy (772): the next spell without a printed [Repeat] gets a Repeat cost equal to its base cost (Energy and Power)
    // "You can look at their facedown cards this turn." (Scuttle Crab): the
    // turn this player may look at the other player's facedown cards, cards
    // hidden later that turn included (TriggerManager::onCardHidden shows
    // them). Turn-stamped, so it needs no reset.
    int may_look_at_opp_facedown_turn = -1;
    int additional_token_copies_used = 0;
    int additional_token_copies_turn = -1;
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
        // Applies only to a card whose printed Energy cost is at most this
        // (-1 = any): "a gear with Energy cost no more than [7]" (Jayce, Man
        // of Progress).
        int max_printed_energy_cost = -1;
        bool units_only = false;            // applies only to unit plays (Vaults of Helia)
        std::string required_tag;           // applies only to cards with this tag (Herald of Scales: "Dragon")
        // Pushed by a Card::applyPassiveAura hook, i.e. a continuous effect.
        // Set by GameEngine::recalculateAuras (cards do not set it), which
        // drops and rebuilds these on every aura pass.
        bool from_aura = false;
        bool first_gear_per_turn = false;   // applies only to the FIRST gear played this
                                            // turn (consult gears_played_this_turn == 0)
        // Applies only to a card played from its owner's hand ("you may play
        // a gear ... from hand this turn, ignoring its Energy cost" — Jayce,
        // Man of Progress): not to a play from the trash, the Champion Zone
        // or by an effect from anywhere else, and such a play does not use a
        // one-shot modifier up.
        bool from_hand_only = false;
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
        // A permission the card itself keeps while it is in the trash ([Flow],
        // "You may play me from your trash for ..."): rebuilt on every aura
        // pass (GameEngine::refreshFlowGrants) instead of being pushed once.
        bool standing = false;
    };
    std::vector<TrashReplayGrant> trash_replay_grants;

    // Karthus, Eternal (and similar): "Your Deathknell effects trigger
    // an additional time." Counted from board via aura recalculation —
    // bumped per controlled Karthus. TriggerManager::onUnitDied adds
    // (1 + deathknell_double_count) chain items for any friendly card
    // with a Deathknell trigger. 0 = normal (single fire).
    int deathknell_double_count = 0;

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

    // Transient: source of the play currently being paid for. Set by
    // GameEngine::executePlayCard / executePlaySpell around the payment and
    // put back to Hand right after it; set by EffectExecutor's effect plays
    // for a card played from anywhere but a hand, and restored when they are
    // done. It is Hand at every other moment, so nothing resets it at end of
    // turn. Lets the cost path know the source without threading it through
    // every signature (Card::grantsAccelerateTo, CostModifier::from_hand_only).
    Intent::PlaySource current_play_source = Intent::PlaySource::Hand;

    // ── Vendetta ─────────────────────────────────────────────────────────
    int cards_burned_this_turn = 0;       // cards this player Burned this turn (CR 441)
    int cards_banished_this_turn = 0;     // owned cards banished this turn
    bool chose_enemy_unit_this_turn = false;   // Hungry Wolf
    // How many times this turn this player chose an enemy unit or gear with a
    // spell or a unit's ability (Prodigal Explorer). Counted with the flag
    // above by GameState::noteChosen.
    int enemy_choices_this_turn = 0;

    // This player's most recent combat this turn: where it was, whether they
    // attacked, and how much combat damage they assigned beyond what was
    // needed for lethal ("if you assigned N or more excess damage").
    // last_combat_bf is kInvalidId when they have not been in a combat this turn.
    BattlefieldId last_combat_bf = kInvalidId;
    bool last_combat_attacker = false;
    int excess_combat_damage = 0;
    bool played_nontoken_unit_this_turn = false;  // Swain, Visionary
    bool played_nontoken_gear_this_turn = false;
    int nontoken_gear_plays_this_turn = 0;   // counted as each play is announced (Jayce, Brilliant Inventor)
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
        enemy_choices_this_turn = 0;
        last_combat_bf = kInvalidId;
        last_combat_attacker = false;
        excess_combat_damage = 0;
        played_nontoken_unit_this_turn = false;
        played_nontoken_gear_this_turn = false;
        nontoken_gear_plays_this_turn = 0;
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
        draws_before_last_draw = 0;
        unit_died_in_beginning_this_turn = false;
        units_enter_ready_this_turn = false;
        unit_died_this_turn = false;
        unit_deaths_this_turn = 0;
        unit_deaths_in_beginning_this_turn = 0;
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
    // Who controlled the battlefield just before that conquer; nullopt = it
    // was uncontrolled ("When I conquer a battlefield that was uncontrolled").
    // Valid together with conquered_on_turn / conquered_by_player.
    std::optional<PlayerId> conquered_from;

    // Aura-derived per-BF flags (reset + recomputed each recalculateAuras by the
    // BF/unit cards' applyPassiveAura):
    // Mageseeker Investigator: "Opponents must pay [A] for each unit beyond
    // the first to move multiple units to my battlefield at the same time."
    // [p] = the [A] player p owes per unit beyond the first of one Standard
    // Move to this battlefield (CR 144.3).
    int multi_move_tax[2] = {0, 0};
    // Noxus Saboteur (018): "Your opponents' [Hidden] cards can't be revealed
    // here." [p] is set for the player whose facedown cards here can't be
    // turned face up, i.e. can't be played from facedown (CR 811).
    bool opp_hidden_unrevealable[2] = {false, false};

    /// Get all unit IDs at this battlefield belonging to a player.
    std::vector<GameObjectId> unitsControlledBy(
        PlayerId player, const ObjectTable& objects) const;

    /// Check if any player has units here.
    bool hasUnitsFrom(
        PlayerId player, const ObjectTable& objects) const;
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
    /// Additional Turns queued after this one, the next first (CR 735-738;
    /// GameState::queueAdditionalTurn).
    std::vector<PlayerId> additional_turn_queue;
    /// Whose regular turn was the last one begun: the Turn Order resumes from
    /// it after Additional Turns (CR 737). Written by the turn transition.
    PlayerId regular_turn_player = PlayerId::Player1;

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
    // The incarnation (GameObject::zone.incarnation()) of each object this
    // item chose, as it was chosen (GameState::stampChosen). An object that
    // has left for a non-board zone since — and maybe come back — is a new
    // object and no longer this item's legal target (CR 359.3.e.2, 359.3.e.4;
    // testing phase G14).
    struct ChosenStamp {
        GameObjectId id = kInvalidId;
        uint32_t incarnation = 0;
    };
    std::vector<ChosenStamp> chosen_stamps;
    // A finalized spell or ability on the chain that this item chose ("Counter
    // a spell", "Choose a spell": CR 355.9.a.2), by Card::pickChainItem.
    // kInvalidId: none.
    ChainItemId chosen_item = kInvalidId;
    // A cost this item still owes after its choices are made ("you may pay
    // [N] to <choose a unit>", an activated ability's cost: CR 403.2 -> 405.1).
    // While it is set, a choice is offered only if its Deflect and this cost
    // can be paid together, and a Deflect is paid ahead of it (testing phase
    // F5): Card::owesAfterChoices.
    int owed_energy = 0;
    int owed_power = 0;
    uint32_t owed_power_mask = 0;   // resource::DomainMask of owed_power
    // An [Equip] activation (CR 818): `targets` holds the unit; it attaches
    // the source gear as it resolves.
    bool is_equip = false;

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
    // kInvalidId = the source's own ability. Copied from Intent::granted_ability_def when
    // the activation is enqueued.
    CardDefId granted_ability_def = kInvalidId;

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
    // The state digest taken when Card::costDueNow first said "due" in the
    // current finalize-pass run (Card::costStateFingerprint).
    uint64_t cost_due_fingerprint = 0;
    // This spell's choices are being made before it is on the chain (and
    // before its cost is paid): "chosen" events are held until it is.
    bool pre_play_choices = false;

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
    // The choices of each additional execution (CR 820.2: "choices must be
    // made at the usual time during the Make Relevant Choices step"; 820.2.a:
    // they need not be the same as the first execution's). Made by the engine
    // as the item is finalized (GameEngine::chooseTargetsAtFinalize), one
    // entry per Repeat paid; ChainManager::stepResolve loads entry r before
    // extra execution r. Empty for an item nobody finalized (built by hand):
    // such an execution asks as it runs.
    struct ExecutionChoices {
        std::vector<GameObjectId> targets;
        std::vector<GameObjectId> chosen_objects;
        std::vector<ChosenStamp> chosen_stamps;
        ChainItemId chosen_item = kInvalidId;
        int resume_point = 0;
        std::vector<int32_t> resume_data;
        int8_t may_answer = -1;
        bool choices_finalized = false;
        bool finalize_costs_paid = false;
    };
    std::vector<ExecutionChoices> repeat_choices;

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
    // That subject as it left the board, when the event was its leaving (it
    // died, was returned, banished, recycled): the object's own `last_known`
    // copied as the ability triggered. The object keeps ONE snapshot, so a
    // card that dies, is played again and dies again before its first death
    // ability resolves would otherwise have both abilities read the second
    // death. Cards read it with asItLeft(ctx, id) (cards/card_helpers.h).
    std::optional<GameObject::LastKnown> subject_as_it_left;

    // Set for an Equipment's "When I ..." ability (its effect text, speaking
    // for the unit it is attached to): the unit the event happened to. Such an
    // item is a triggered ability whose source is the gear; it resolves
    // through Card::onEquippedTrigger(ctx, equipped_unit, ...) instead of
    // Card::onTrigger. kInvalidId for every other item.
    GameObjectId equipped_unit = kInvalidId;

    // Set for an ability triggered by a unit moving ("When I move", "When a
    // unit moves from here", ...): where that move started and ended. It
    // belongs to the trigger, not to the unit — a unit that moves again
    // before this resolves gives its second trigger a different one. Cards
    // read it with triggeringMove() (card_helpers.h).
    struct Move { LocationId from; LocationId to; };
    std::optional<Move> triggering_move;

    // Set for a reflexive trigger (CR 387-389: "Do this: ...", "Do one of the
    // following ..."): an ability created while another one resolves, when
    // the first part of its text has been done. Which of the card's reflexive
    // triggers it is, and what that first part produced for it (the type of
    // the card discarded, the token just played). It is a chain item like any
    // other — finalized with its own choices, and players get priority before
    // it resolves — and resolves through Card::onReflexiveTrigger.
    // -1 = not a reflexive trigger.
    int reflexive_index = -1;
    std::vector<int32_t> reflexive_data;

    // Set for a keyword's own triggered ability ([Vision]: "When this is
    // played, [Predict]", CR 817; [Weaponmaster], CR 821). The keyword may
    // come from a continuous effect and the object may be a token with no
    // card class, so the ability is the keyword's (engine/keyword_abilities.h)
    // rather than the source card's: `card_def_id` is not consulted.
    std::optional<Keyword> keyword_ability;

    // ── Simultaneous triggers (CR 383.3.d) ──
    // Triggered abilities that triggered at the same time share a batch
    // number (ChainState::current_trigger_batch as TriggerManager queued
    // them); 0 = not part of any batch (spells, activated abilities,
    // reflexive triggers — CR 389.2 creates those in order). Their controller
    // orders a batch as its first item is about to be finalized
    // (GameEngine::orderPendingTriggers); `batch_ordered` says that is done.
    // `batch_first` is the player whose abilities go on the chain first when
    // that is not the turn player (the attacker of a combat, CR 466.2.d.1).
    uint32_t trigger_batch = 0;
    bool batch_ordered = false;
    PlayerId batch_first = PlayerId::None;

    // The source left the board after this ability was put on the chain
    // (EffectExecutor::leftBoard). Wherever the card is now — back on the
    // board included — it is a new object (CR 124.1): an ability that says
    // "this" ([Temporary]: "kill this") no longer has anything to act on.
    bool source_left_board = false;

    // Set by the finalize pass of a triggered ability that is not performed:
    // its leading "may" was declined or its cost not paid (CR 403.1.a, 405.2)
    // or a required choice had no legal option (CR 403.4). The item leaves
    // the chain as it would have been finalized: it "ceases to be a Pending
    // Item but never becomes a Finalized Chain Item" — nobody gets priority.
    bool remove_at_finalize = false;
    // A choice helper found no legal option for a REQUIRED choice during the
    // finalize pass (Card::requiredChoiceMissing): CR 403.4.
    bool required_choice_missing = false;
    // The card said its "may" is not the first part of its effect
    // (Card::mayIsNotTheWholeAbility): declining it does not remove the ability.
    bool may_is_partial = false;
};

struct ChainState {
    // ── Loops (Tournament Rules 505) ──
    // How often each triggered ability's leading "may" has been accepted
    // since the chain was last empty. A loop that players maintain by their
    // decisions runs for the number of iterations they choose, and then the
    // player makes a different decision (TR 505.4, 505.7, 505.10): past
    // kMayLoopIterations acceptances of one ability in one unbroken chain its
    // "may" is not offered again (Card::confirmOptional). Cleared when the
    // chain empties (ChainManager).
    struct MayCount {
        GameObjectId source = kInvalidId;
        TriggerType trigger = TriggerType::None;
        int accepted = 0;
    };
    std::vector<MayCount> may_loop_counts;
    static constexpr int kMayLoopIterations = 100;

    // CR 383.3.d: the batch number given to triggered abilities queued now
    // (see ChainItem::trigger_batch). TriggerManager opens a new batch for
    // each event it handles; the engine holds one open across events the
    // rules call simultaneous (units that die together, the units one cleanup
    // finds newly Mighty, a combat's start with its showdown's start).
    uint32_t trigger_batch_counter = 0;
    uint32_t current_trigger_batch = 0;
    int trigger_batch_depth = 0;     // nested event handlers share the batch
    int trigger_batch_holds = 0;     // > 0: an engine scope holds the batch open
    PlayerId batch_first_player = PlayerId::None;

    std::vector<ChainItem> items;
    ChainItemId next_id = ChainItemId::fromRaw(1);  // ids count up from 1

    // Phase C-1 commit 6 — the chain item currently being resolved.
    // Populated by ChainManager::stepResolve immediately after popping the
    // top item from `items`; cleared after disposal. Holds the resolving
    // ChainItem while the resumable resolution loop is calling onResolve.
    // Cards that yield mid-resolve (call EffectExecutor::requestChoice +
    // return) read/write `resuming->resume_point` to track their saved
    // progress across re-entry. Counter spells continue to use
    // `items.back()` to find their target — `resuming` is a separate slot.
    std::optional<ChainItem> resuming;

    // The move TriggerManager::onUnitMoved is announcing right now: every
    // ability it queues gets it as ChainItem::triggering_move, and a
    // triggerCondition evaluated meanwhile reads it. Empty outside that call.
    std::optional<ChainItem::Move> move_being_announced;

    bool exists() const { return !items.empty(); }

    ChainItemId allocateId() {
        const ChainItemId id = next_id;
        next_id = ChainItemId::fromRaw(id.raw() + 1);
        return id;
    }

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

/// Holds one trigger batch open across several events that happen at the same
/// time (CR 383.3.d): every triggered ability queued while it lives is in the
/// same batch. `first` names the player whose abilities go on the chain first
/// when that is not the turn player (CR 466.2.d.1: the attacker).
struct SimultaneousTriggers {
    ChainState& chain;
    PlayerId saved_first;
    explicit SimultaneousTriggers(ChainState& c, PlayerId first = PlayerId::None)
        : chain(c), saved_first(c.batch_first_player) {
        if (chain.trigger_batch_holds++ == 0)
            chain.current_trigger_batch = ++chain.trigger_batch_counter;
        if (first != PlayerId::None) chain.batch_first_player = first;
    }
    ~SimultaneousTriggers() {
        chain.batch_first_player = saved_first;
        --chain.trigger_batch_holds;
    }
    SimultaneousTriggers(const SimultaneousTriggers&) = delete;
    SimultaneousTriggers& operator=(const SimultaneousTriggers&) = delete;
};

/// One event as TriggerManager handles it: the abilities it triggers are
/// simultaneous. Nested handlers and an engine hold share the outer batch.
struct TriggerEventScope {
    ChainState& chain;
    explicit TriggerEventScope(ChainState& c) : chain(c) {
        if (chain.trigger_batch_depth++ == 0 && chain.trigger_batch_holds == 0)
            chain.current_trigger_batch = ++chain.trigger_batch_counter;
    }
    ~TriggerEventScope() { --chain.trigger_batch_depth; }
    TriggerEventScope(const TriggerEventScope&) = delete;
    TriggerEventScope& operator=(const TriggerEventScope&) = delete;
    /// The batch a trigger queued now belongs to (0 outside any event).
    static uint32_t now(const ChainState& c) {
        return (c.trigger_batch_depth > 0 || c.trigger_batch_holds > 0)
            ? c.current_trigger_batch : 0;
    }
};

// ─── Delayed Abilities (CR 389-392) ─────────────────────────────────────────

struct DelayedAbility {
    GameObjectId source = kInvalidId;     // card that created this
    CardDefId card_def_id = kInvalidId;   // for looking up the Card object
    // Who controls the ability when it goes on the chain: the controller of
    // the effect that created it (CR 391-392).
    PlayerId controller = PlayerId::None;
    // Whose event it waits for, when that is not its controller ("When THEY
    // hold, ..." — Ashe, Focused). None = the controller's own.
    PlayerId watched_player = PlayerId::None;
    PlayerId watches() const {
        return watched_player != PlayerId::None ? watched_player : controller;
    }
    TriggerType trigger = TriggerType::None; // what event fires it
    // When it stops waiting. The expiration step removes it at the end of
    // turn `expires_on_turn` (and of any later turn, should that one have been
    // missed). kThisTurn — the default, so an ability nobody dated cannot
    // outlive its turn — is whichever turn is ending first; kUntilFired never
    // expires ("at the start of your next Main Phase").
    static constexpr int kThisTurn = -1;
    static constexpr int kUntilFired = -2;
    int expires_on_turn = kThisTurn;
    bool expiresAtEndOf(int turn_number) const {
        if (expires_on_turn == kUntilFired) return false;
        return expires_on_turn == kThisTurn || expires_on_turn <= turn_number;
    }
    bool fired = false;                   // one-shot: fire once, then remove
    // Fires for EVERY matching event until it expires, instead of once ("When
    // a friendly unit is played this turn, ..." — Rally the Troops; "When any
    // unit takes damage this turn, kill it" — Imperial Decree).
    bool repeats = false;

    // Optional filter for events that carry an object id (e.g.
    // UnitDiedEvent::object). Set to a specific GameObjectId to narrow
    // the delayed ability to fire only when THAT object is the event
    // subject (Deadly Flourish: "when IT dies"). kInvalidId = no filter,
    // fires on any matching trigger.
    GameObjectId target_filter = kInvalidId;

    /// The object a "do this to IT at <time>" ability is about ("Disempower it
    /// at end of turn": Tornado Warrior, Sanction). Handed to the ability as
    /// its chain item's triggering_subject when it fires. Its window lasts as
    /// long as that object is on the board (CR 391.5.a): EffectExecutor::
    /// leftBoard removes the ability when the object leaves (a card played
    /// again is a new object, CR 124.1).
    GameObjectId subject = kInvalidId;
    /// Which of its card's delayed abilities this is, for a card that makes
    /// more than one kind (Sanction: 0 "disempower it", 1 "empower it");
    /// handed to the ability as its chain item's ability_index.
    int ability_index = 0;
};

// ─── Complete Game State ─────────────────────────────────────────────────────

/// GameState::battlefields: the vector it always was, which a slot's id also
/// indexes (`state.battlefields[bf_id]`; an id is the slot's position).
struct BattlefieldList : std::vector<BattlefieldState> {
    using std::vector<BattlefieldState>::vector;
    using std::vector<BattlefieldState>::operator[];
    BattlefieldState& operator[](BattlefieldId id) {
        return std::vector<BattlefieldState>::operator[](id.raw());
    }
    const BattlefieldState& operator[](BattlefieldId id) const {
        return std::vector<BattlefieldState>::operator[](id.raw());
    }
};

struct GameState {
    // Configuration
    ModeOfPlay mode;

    // Players (indexed by playerIndex())
    PlayerState players[2];

    // Board
    BattlefieldList battlefields;

    // All game objects (the object pool). An ObjectTable: the unordered_map it
    // always was — same iteration order, stable references — plus an index of
    // the objects in play by location, which the board queries below and the
    // engine's hot loops read instead of walking every object
    // (core/object_table.h).
    ObjectTable objects;
    GameObjectId next_object_id = GameObjectId::fromRaw(1);  // ids count up from 1
    // How many times cleanup has looked for units that became [Mighty]
    // (GameObject::mighty_seen is compared with it).
    uint32_t mighty_checks = 0;

    // Turn
    TurnState turn;

    // Chain
    ChainState chain;

    // What each player has learned about hidden cards. Written only through
    // engine/knowledge_tracker.h, read only through KnowledgeView.
    KnowledgeState knowledge;

    // Delayed abilities (Phase 5, CR 389-392)
    std::vector<DelayedAbility> delayed_abilities;

    /// "It gains [Keyword N] this combat": a continuous effect a resolved
    /// ability created (EffectExecutor::giveKeywordThisCombat). It lasts while
    /// the combat at `battlefield` that was going on when it was made goes on
    /// and `unit` stays on the board, whatever becomes of the card that made
    /// it; GameEngine::recalculateAuras applies it and drops it once over.
    struct CombatKeyword {
        GameObjectId unit = kInvalidId;
        Keyword keyword = Keyword::Count;
        int value = 0;
        BattlefieldId battlefield = kInvalidId;
        int turn = 0;
    };
    std::vector<CombatKeyword> combat_keywords;

    /// Cards a replacement took away as they were looked at or revealed from
    /// the top of a deck ("As you look at or reveal me ..., you may banish
    /// me": Nocturne, Horrifying; CR 370.1.c.1). The effect that was looking
    /// at them goes on without them: it does not put them back, recycle,
    /// banish, play or offer them (EffectExecutor::seenOnTopOfDeck). Emptied
    /// when the chain item that looked has resolved.
    std::vector<GameObjectId> left_while_seen;
    bool leftWhileSeen(GameObjectId id) const {
        return std::find(left_while_seen.begin(), left_while_seen.end(), id) !=
               left_while_seen.end();
    }

    // Game result
    bool game_over = false;
    PlayerId winner = PlayerId::None;
    std::string game_over_reason;

    // Decision counter (for training data indexing)
    int decision_index = 0;

    // One history token per decision applied to this game, in order
    // (openspiel/action_vocab.h: the Intent's slot plus a fingerprint of
    // every field, which identifies the chosen Intent). Written only by
    // GameEngine::recordAppliedIntent, in runGame and step-machine games
    // alike. Replaying the tokens from the same decks and seed rebuilds the
    // game exactly; lives here so a state copy carries its own history.
    std::vector<int64_t> action_history;

    // ── Cost payment cursor (Phase C-1 commit 8) ──
    // Saves the rune-pick progress across step-machine yields.
    // Populated when payCardCost has yielded a NeedDecision and
    // is waiting for the agent's rune choice. Cleared when cost
    // is fully paid (or aborted). Copied with the GameState like every other
    // field (it owns a vector, so it is not a POD and must not be memcpy'd).
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

    /// "Take a turn after this one" (CR 735): an Additional Turn for `owner`,
    /// inserted right after the current turn — before any queued earlier
    /// (CR 738, second example).
    void queueAdditionalTurn(PlayerId owner) {
        turn.additional_turn_queue.insert(turn.additional_turn_queue.begin(), owner);
    }
    /// How many Additional Turns of `owner` are queued.
    int additionalTurnsOf(PlayerId owner) const {
        return static_cast<int>(std::count(turn.additional_turn_queue.begin(),
                                           turn.additional_turn_queue.end(), owner));
    }

    // ── Object management ──

    GameObjectId createObject() {
        const GameObjectId id = next_object_id;
        next_object_id = GameObjectId::fromRaw(id.raw() + 1);
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

    /// The points needed to win right now: the mode's Victory Score plus what
    /// battlefields in play add to it (TurnState::victory_score_bonus, rebuilt
    /// every aura pass from Card::victoryScoreModifier). What a card means by
    /// "the Victory Score" — never read mode.victory_score for that.
    int effectiveVictoryScore() const { return mode.victory_score + turn.victory_score_bonus; }

    /// Bookkeeping for `chooser` having chosen `chosen` with the spell or
    /// ability `by` (nullptr when it is on no chain item): "if you've chosen
    /// an enemy unit this turn" and the count of enemy units / gear chosen
    /// with spells or unit abilities. Every site that locks in a choice calls
    /// this once per chosen object. A unit / gear is one on the board.
    void noteChosen(PlayerId chooser, GameObjectId chosen, const ChainItem* by) {
        if (!objectExists(chosen)) return;
        const auto& o = getObject(chosen);
        if (!o.location.has_value() || o.controller == chooser) return;
        if (o.isUnit()) player(chooser).chose_enemy_unit_this_turn = true;
        if (!o.isUnit() && !o.isGear()) return;
        const bool spell_or_unit_ability = by && (by->is_spell ||
            (by->is_ability && objectExists(by->source) && getObject(by->source).isUnit()));
        if (spell_or_unit_ability) player(chooser).enemy_choices_this_turn++;
    }

    /// Stamp `chosen` on `item` as it is now (CR 359.3.e.4: it is that
    /// object only for as long as it does not change zones to or from a
    /// non-board zone). Every site that locks in a choice calls this.
    void stampChosen(ChainItem& item, GameObjectId chosen) const {
        if (!objectExists(chosen)) return;
        const uint32_t inc = getObject(chosen).zone.incarnation();
        for (auto& st : item.chosen_stamps)
            if (st.id == chosen) { st.incarnation = inc; return; }
        item.chosen_stamps.push_back({chosen, inc});
    }
    /// False if `item` chose `chosen` and that object has changed zones to or
    /// from a non-board zone since (a new object, CR 359.3.e.2 / 359.3.e.4):
    /// the choice is then no longer legal. True for an object the item has
    /// not stamped.
    bool stillTheChosenObject(const ChainItem& item, GameObjectId chosen) const {
        if (!objectExists(chosen)) return false;
        for (const auto& st : item.chosen_stamps)
            if (st.id == chosen) return st.incarnation == getObject(chosen).zone.incarnation();
        return true;
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
            // A hand revealed to all players (CR 425.1) shows its owner
            // nothing: they are not an observer of their own hand.
            if (obs == e.owner && e.source_zone == ZoneType::Hand) return;
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
