#pragma once
/// @file game_object.h
/// Runtime representation of a card/token on the board or in a zone.
///
/// CardDef is the static template (from registry.json).
/// GameObject is the mutable instance that tracks current state
/// (damage, exhaustion, temporary mods, location, attachments, etc).

#include "types.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace riftbound {

struct GameObject {
    GameObjectId id = kInvalidId;
    CardDefId card_def_id = kInvalidId;  // link to static card data
    PlayerId owner = PlayerId::None;
    PlayerId controller = PlayerId::None;

    // ── Identity (copied from CardDef, may be modified by layers) ──
    std::string name;
    CardType card_type = CardType::Unit;
    SuperType super_type = SuperType::None;
    std::vector<std::string> tags;
    std::vector<Domain> domains;

    // ── Unit stats ──
    int base_might = 0;
    int current_might = 0;  // after layers (assault, shield, buffs, auras)
    int damage_marked = 0;

    // ── State ──
    bool is_exhausted = false;
    bool is_stunned = false;
    bool is_hidden = false;             // facedown at a battlefield
    BattlefieldId hidden_at = kInvalidId; // which BF it's hidden at
    int hidden_on_turn = -1;            // turn it was hidden (gains Reaction next turn)

    // ── Position ──
    ZoneType zone = ZoneType::MainDeck;
    std::optional<LocationId> location;  // only meaningful when on board
    // Last on-board location before the unit died / left play. Populated
    // by EffectExecutor::killObject just before clearing `location`.
    // Used by Deathknell triggers that need to know "where did I die"
    // (e.g. Loyal Poro: "didn't die alone" — check other friendlies at
    // this location). Stays valid across the trigger's chain item.
    std::optional<LocationId> last_location;

    // ── Combat ──
    CombatDesignation combat_designation = CombatDesignation::None;

    // ── Buffs ──
    int buff_count = 0;          // +1 might per buff
    int temp_buff_count = 0;     // temporary buffs applied "this turn" (expire at expiration step)
    int temp_assault_value = 0;  // temporary assault applied "this turn"
    int temp_shield_value = 0;   // temporary shield applied "this turn"
    int temp_might_bonus = 0;    // temporary +N might applied "this turn"

    // ── Keywords (computed via layers, cached) ──
    KeywordSet keywords;

    // ── Parameterized keyword values ──
    int assault_value = 0;
    int shield_value = 0;
    int deflect_value = 0;

    // ── Attachment ──
    std::optional<GameObjectId> attached_to;          // card I'm attached to
    std::vector<GameObjectId> attachments;             // cards attached to me
    int might_bonus = 0;                               // this gear's might bonus (from CardDef)
    int attachment_might_bonus = 0;                    // total might bonus from attached gear
    bool is_rules_text_inactive = false;              // gear: rules text inactive while attached (CR 718.2)

    // ── Targeting protection (CR 700.x — "can't be chosen by ...") ──
    // Set by the engine during cleanup / aura recalc by consulting the
    // Card's canBeChosenByEnemy() method. Card::enumerateLegalTargets
    // reads this flag to filter out untargetable enemies. Baron Nashor
    // ([709]) is the canonical example.
    bool untargetable_by_enemy = false;

    // ── Per-card persistent counters (legend state, etc.) ──
    // Generic int counters keyed by name so multiple card classes can have
    // their own state without ballooning this struct. Used by Virtuoso
    // (spells_banished_with_me). Survives across turns; only reset on
    // explicit Card subclass reset.
    std::unordered_map<std::string, int> card_counters;

    // String-valued per-card state, keyed by purpose. For cards that "name"
    // something at play time and act on the named value later (The List 700:
    // "name a tag" → string_state["named_tag"]). Parallel to card_counters
    // (which only holds ints). Survives across turns; reset by the owning
    // Card subclass if needed.
    std::unordered_map<std::string, std::string> string_state;

    // ── Damage-modifying turn-scoped flags ──
    // Lotus Trap and similar cards set `damage_doubled_this_turn` on a
    // target unit; EffectExecutor::dealDamage consults the flag and
    // doubles the assignment. Cleared at expiration (end of turn).
    bool damage_doubled_this_turn = false;

    // ── Deferred one-shot death replacement (Tactical Retreat 737) ──
    // "The next time it would die this turn, heal it, exhaust it, and recall
    // it to base instead." killUnit consults this BEFORE the normal death
    // path: when set it heals + exhausts + recalls the unit to base and clears
    // the flag (one-shot). Expires at end of turn if unused.
    bool death_replacement_recall_pending = false;

    // ── One-shot damage prevention (Counter Strike 510) ──
    // "The next time that unit would be dealt damage this turn, prevent it."
    // EffectExecutor::dealDamage consults this BEFORE applying damage: when set
    // it prevents the next instance of damage to this unit and clears the flag
    // (one-shot). Expires unused at end of turn.
    bool prevent_next_damage_this_turn = false;

    // ── Per-unit movement restriction (Determined Sentry 673) ──
    // "I can't move to base." Set by the card on play; consulted by the
    // main-phase move generator (parallel to BattlefieldState::blocks_move_to_base,
    // which is the per-battlefield variant).
    bool cant_move_to_base = false;

    // ── Take-control reversion (CR 416, Phase 6q+) ──
    // EffectExecutor::takeControl with `until_end_of_turn=true` snapshots
    // the original controller into `original_controller_eot` and flips
    // this flag. Expiration step (end-of-turn) restores `controller` from
    // the snapshot and clears the flag. Permanent control changes do NOT
    // set the flag.
    bool control_reverts_eot = false;
    PlayerId original_controller_eot = PlayerId::None;

    // ── Take-control reversion "until I leave the board" (Akshan 431) ──
    // When an effect takes control of this object only "until <source> leaves
    // the board", these record the controlling source and where control
    // returns. GameEngine::revertLapsedControl (run in cleanup) restores
    // `controller` and clears these once the source object is no longer in
    // play. Independent of the end-of-turn variant above.
    bool control_reverts_on_source_leave = false;
    GameObjectId control_source_obj = kInvalidId;
    PlayerId control_revert_to = PlayerId::None;

    // ── Card-specific tracked objects ──
    // Virtuoso [782] uses this to remember WHICH banished spells were
    // banished by ITS ability ("four spells banished with me"). When the
    // 4-stack payoff fires, only THESE return to trash — banished spells
    // from other sources stay in banishment. Generic vector so future
    // similar cards can re-use it.
    std::vector<GameObjectId> tracked_objects;

    // ── Aura effects (Phase 5b — tagged effects) ──
    struct AuraEffect {
        GameObjectId source = kInvalidId;  // aura source object
        int might_bonus = 0;               // +/- M from this aura
        int might_minimum = 0;             // minimum M (0 = no min)
        Keyword keyword = Keyword::Count;  // granted keyword (Count = none)
        int keyword_value = 0;             // e.g. Assault 1, Shield 2
        bool suppress_combat_damage = false; // Vilemaw: "doesn't deal combat damage"
    };
    std::vector<AuraEffect> aura_effects;  // active auras affecting this object
    int aura_might_bonus = 0;              // cached sum of aura might bonuses
    KeywordSet aura_keywords;              // cached union of aura-granted keywords
    bool aura_no_combat_damage = false;    // cached: any aura suppresses combat dmg

    /// This unit deals no combat damage (stunned, or an aura suppresses it —
    /// Vilemaw). Consulted by the combat-damage step.
    bool dealsNoCombatDamage() const { return is_stunned || aura_no_combat_damage; }

    // Keywords granted by "this turn" effects (Bounty Hunter's
    // `[E]: Give a unit [Ganking] this turn.`, Catalyst of Aeons-style
    // temp grants, etc.). Mirror-set into `keywords` when granted so
    // hasKeyword() short-circuits cheaply; the expiration step (CR
    // 317.2.c) walks this set and clears each bit from `keywords` if
    // the CardDef doesn't print it AND no active aura grants it,
    // then clears this field. Without this tracking, a "this turn"
    // grant becomes a permanent keyword — Bounty Hunter's Ganking
    // would stick forever, contradicting the card text.
    KeywordSet temp_keywords;

    // ── Convenience ──
    bool isReady() const { return !is_exhausted; }
    bool isUnit() const { return card_type == CardType::Unit; }
    bool isGear() const { return card_type == CardType::Gear; }
    bool isSpell() const { return card_type == CardType::Spell; }
    bool isRune() const { return card_type == CardType::Rune; }
    bool isBattlefield() const { return card_type == CardType::Battlefield; }
    bool isLegend() const { return card_type == CardType::Legend; }
    bool isPermanent() const { return isUnit() || isGear(); }
    bool isChampion() const { return super_type == SuperType::Champion; }
    bool isToken() const { return super_type == SuperType::Token; }

    bool isAlive() const { return damage_marked < current_might; }
    bool hasLethalDamage() const {
        return damage_marked > 0 && damage_marked >= current_might;
    }

    bool isAtBase() const {
        return location.has_value() &&
               std::holds_alternative<BaseLocation>(*location);
    }

    bool isAtBattlefield() const {
        return location.has_value() &&
               std::holds_alternative<BattlefieldLocation>(*location);
    }

    std::optional<BattlefieldId> battlefieldId() const {
        if (location && std::holds_alternative<BattlefieldLocation>(*location)) {
            return std::get<BattlefieldLocation>(*location).id;
        }
        return std::nullopt;
    }

    /// Compute current might from base + buffs + assault/shield (Phase 1 simple version).
    /// Full layer system will replace this later.
    void recomputeMight() {
        // buff_count = permanent +1 buff counters (CR); temp_might_bonus =
        // this-turn temporary might (expires at the expiration step). Tracked
        // separately so "spend a buff" / "if it has a buff" gates read only the
        // permanent counters, not transient temp might.
        current_might = base_might + buff_count + temp_might_bonus +
                        attachment_might_bonus + aura_might_bonus;
        // Assault applies only while attacking (base + aura-granted)
        if (combat_designation == CombatDesignation::Attacker) {
            current_might += assault_value;
        }
        // Shield applies only while defending
        if (combat_designation == CombatDesignation::Defender) {
            current_might += shield_value;
        }
        // Apply aura minimums (e.g., Leona: -8M to minimum 1)
        for (auto& ae : aura_effects) {
            if (ae.might_minimum > 0 && current_might < ae.might_minimum) {
                current_might = ae.might_minimum;
            }
        }
        if (current_might < 0) current_might = 0;
    }

    /// Check if this object has a keyword (base + aura-granted).
    bool hasKeyword(Keyword kw) const {
        return keywords.has(kw) || aura_keywords.has(kw);
    }
};

} // namespace riftbound
