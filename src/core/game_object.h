#pragma once
/// @file game_object.h
/// Runtime representation of a card/token on the board or in a zone.
///
/// CardDef is the static template (authored in the card's class, Card::def()).
/// GameObject is the mutable instance that tracks current state
/// (damage, exhaustion, temporary mods, location, attachments, etc).

#include "tracked_field.h"
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

    // ── The fields read most ──
    // Kept together at the front of the object on purpose. The engine still
    // has walks over every object of a game (the cleanup's change checksum,
    // the reset of statuses on cards in any zone, the per-turn expirations),
    // and each object is its own heap node: with these fields spread over the
    // ~700 bytes of a GameObject such a walk touched four or five cache lines
    // per object; here it touches one or two (plans/09 item 9.KK3). Each is
    // documented where it used to be declared, further down.
    CardType card_type = CardType::Unit;
    SuperType super_type = SuperType::None;
    bool is_exhausted = false;
    bool is_stunned = false;
    bool is_hidden = false;             // facedown at a battlefield
    CombatDesignation combat_designation = CombatDesignation::None;
    bool untargetable_by_enemy = false;
    bool untargetable_by_enemy_this_turn = false;
    bool control_reverts_eot = false;
    bool control_reverts_on_source_leave = false;
    // Both read as the plain value (a ZoneType, an optional LocationId). They
    // are tracked fields because the board index of the object table is built
    // from them: assigning either tells the table (core/tracked_field.h,
    // core/object_table.h). A location is replaced, never edited in place.
    TrackedField<ZoneType> zone{ZoneType::MainDeck};
    TrackedLocation location;  // only meaningful when on board
    // Unit stats.
    int base_might = 0;
    int current_might = 0;  // after layers (assault, shield, buffs, auras)
    int damage_marked = 0;
    int aura_might_bonus = 0;   // cached sum of aura might bonuses
    // The "becomes [Mighty]" edge: the number of the cleanup that last saw
    // this unit on the board with 5+ [M] (GameState::mighty_checks then), 0
    // for never. The unit "was Mighty" at a cleanup if this is the number of
    // the one before. A unit off the board is not looked at and its number
    // goes stale by itself, so the edge is forgotten there without a walk
    // over every object (GameEngine::cleanup).
    uint32_t mighty_seen = 0;

    // ── Identity (copied from CardDef, may be modified by layers) ──
    std::string name;
    std::vector<std::string> tags;
    std::vector<Domain> domains;

    // ── State ── (is_exhausted, is_stunned, is_hidden and the unit stats are
    // declared with the other often-read fields at the top)
    BattlefieldId hidden_at = kInvalidId; // which BF it's hidden at
    int hidden_on_turn = -1;            // turn it was hidden (gains Reaction next turn)

    // ── Empowered (Vendetta, CR 442/443/828) ──
    // Number of Empowered statuses. 0 = not Empowered. Normally binary (0/1);
    // objects allowed to be Empowered multiple times (Kayle, Justified) stack
    // up to Card::maxEmpower(). Cleared when the object leaves the board.
    // (empowered: declared in the aura block below)
    // Deferred end-of-turn flip set by "Empower X. Disempower it at end of
    // turn" (-1) and "Disempower X. Empower it at end of turn" (+1). Applied
    // and cleared by the expiration step.
    // (empower_flip_eot: declared in the aura block below)
    // Aura-computed reduction to THIS object's [Empower] energy cost (Risen
    // Altar). Reset each recalculateAuras.
    // (empower_cost_reduction: declared in the aura block below)
    bool isEmpowered() const { return empowered > 0; }

    // ── Flow / "then banish this" (Vendetta) ──
    // When set on a spell, it is banished instead of going to trash when it
    // leaves the chain. Set when a spell is played for its Flow cost.
    bool banish_on_leave_chain = false;

    // ── Per-turn damage tracking ──
    // Total damage dealt to this object this turn, even if since healed
    // (Affectionate Poro: "if I haven't been dealt damage this turn").
    int damage_taken_this_turn = 0;
    // Recomputed each recalculateAuras: prevent all damage that ENEMY spells
    // and abilities would deal to me (Esteemed Hierophant).
    // (prevent_enemy_spell_ability_damage: declared in the aura block below)
    // Recomputed each recalculateAuras: enemy spells and abilities can't move
    // me (Jagged Cutlass).
    // (cant_be_moved_by_enemy: declared in the aura block below)
    // "It can't be chosen by enemy spells and abilities this turn" (Twilight
    // Shroud). ORed into untargetable_by_enemy by the aura pass; cleared at
    // end of turn.
    // (untargetable_by_enemy_this_turn: declared at the top)

    // ── Counted damage prevention (Ki Barrier: "prevent the next 7 damage") ──
    // Remaining damage to prevent this turn. dealDamage consumes it; expires at
    // end of turn.
    int prevent_damage_remaining = 0;

    // ── Intrinsic token-ness (CR 185.1) ──
    bool is_token = false;

    // ── Last-known information (CR 383.2.c.3.b / 429.1.a.1.b) ──
    // Snapshot taken just before this object leaves the board by dying, so
    // "When I die" abilities read the state from before the death. The kill
    // paths fill it; it stays valid while the death trigger resolves.
    struct LastKnown {
        bool valid = false;
        // Who controlled it when it left. Off the board a card is its owner's
        // again (forgetBoardState), so "its controller" of a unit that was
        // taken and then died, was returned or was banished is read here
        // (CR 359.3.e.13: an effect that moves something "can look back at
        // its characteristics before it changes zones").
        PlayerId controller = PlayerId::None;
        // It left by dying (a kill, lethal damage). False for a return to
        // hand, a banish or a recycle: `killed_by` says nothing then.
        bool died = false;
        int might = 0;
        int empowered = 0;
        int buff_count = 0;
        CombatDesignation combat_designation = CombatDesignation::None;
        bool in_combat = false;                 // at a battlefield with combat in progress
        std::vector<GameObjectId> attachments;  // gear attached at death
        KeywordSet keywords;                    // printed + aura + "this turn"
        bool stunned = false;
        bool exhausted = false;
        // Where it was, and the other units its controller had there. For
        // units that die at the same time (EffectExecutor::beginDeathBatch)
        // this is taken for all of them before the first one leaves, so each
        // sees the others ("if I didn't die alone").
        std::optional<LocationId> location;
        std::vector<GameObjectId> other_friendly_units_here;
        // Who killed it: the controller of the spell or ability that did,
        // else the player whose damage was lethal. None if nobody did.
        PlayerId killed_by = PlayerId::None;
        // "kill a unit with a spell": the spell that is resolving killed it,
        // or it died of lethal damage that `killed_by`'s spell dealt this turn.
        bool killed_with_spell = false;
        // Equipment on it whose effect text was a copy of its text
        // (Card::copiesBearerText): its "When I die" triggers that many more times.
        int text_copies = 0;
        // Set by beginDeathBatch: the kill that follows keeps this snapshot
        // instead of taking a new one. Cleared by endDeathBatch.
        bool batch_pending = false;
        // Set by beginDeathBatch on a listed unit whose death was replaced
        // there: the kill that follows does nothing. Cleared by endDeathBatch.
        bool batch_replaced = false;
        // What the unit looked like when that snapshot was taken. A unit that
        // was listed in a batch, survived it and has changed since is
        // snapshotted afresh when it does die (EffectExecutor::snapshotLastKnown).
        uint64_t batch_stamp = 0;
    };
    LastKnown last_known;

    // ── Who marked the damage (CR: "your damage" = damage you marked) ──
    // damage_marked split by the player whose effect / units dealt it. Index
    // by playerIndex(). Cleared whenever damage_marked is healed to 0.
    int damage_marked_by[2] = {0, 0};

    // ── Position ──
    // (zone and location: declared at the top)
    // Last on-board location before the unit died / left play. Populated
    // by EffectExecutor::killObject just before clearing `location`.
    // Used by Deathknell triggers that need to know "where did I die"
    // (e.g. Loyal Poro: "didn't die alone" — check other friendlies at
    // this location). Stays valid across the trigger's chain item.
    std::optional<LocationId> last_location;

    // ── Combat ── (combat_designation: declared at the top)

    // ── Buffs ──
    int buff_count = 0;          // +1 might per buff
    int temp_buff_count = 0;     // temporary buffs applied "this turn" (expire at expiration step)
    int temp_assault_value = 0;  // temporary assault applied "this turn"
    int temp_shield_value = 0;   // temporary shield applied "this turn"
    int temp_deflect_value = 0;  // [Deflect N] given "this turn" (EffectExecutor::
                                 // giveTemporaryKeyword); added to deflect_value by
                                 // deflectCostFor, zeroed at the expiration step
    int temp_might_bonus = 0;    // temporary +N might applied "this turn"
    int moves_this_turn = 0;     // times this unit moved this turn (Kayn, Unleashed)
    // immune_to_damage (declared in the aura block below): recomputed each recalculateAuras; set by a
                                   // card's applyPassiveAura when its condition
                                   // holds (Kayn, Unleashed: "if I have moved twice
                                   // this turn, I don't take damage"). dealDamage
                                   // no-ops against it.
    int spell_bonus_damage = 0;    // per-spell Bonus Damage rider bound to THIS
                                   // spell object at play time (Ravenborn Tome:
                                   // "the next spell you play deals N Bonus
                                   // Damage" — applies to every instance the spell
                                   // deals). Set from PlayerState::next_spell_bonus_damage.
    // spells_targeting_me_cost_reduction (declared in the aura block below): Irelia, Graceful (462): "your
                                   // spells that choose me cost [1]/[A] less."
                                   // Recomputed each recalculateAuras; the spell
                                   // cost path reduces by this when I'm a target.

    // Aura-granted activated abilities (Forge of the Fluft 522 / Gardens of
    // Becoming 769 / Heimerdinger 111). A granting card's applyPassiveAura
    // appends a ref naming WHICH card (def id) and WHICH of its
    // activatedAbilities() this object may now use; the action generator emits
    // an activation per ref and dispatch routes to that card's onActivate with
    // THIS object as the source. Reset each recalculateAuras.
    struct GrantedAbilityRef {
        CardDefId source_def_id = 0;  // 0 = none; else the granting card
        int ability_index = 0;
        // Granted only if that ability's cost includes [E] (Heimerdinger:
        // "I have all [E] abilities of ...").
        bool exhaust_cost_only = false;
    };
    // (granted_abilities: declared in the aura block below)
    // Abilities granted "this turn" by a resolved effect (Dominus: `give it
    // "[A][A]: Ready me."`). Merged into granted_abilities every aura pass and
    // cleared at end of turn.
    // (temp_granted_abilities: declared in the aura block below)

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
    // (untargetable_by_enemy: declared at the top)

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

    // A death replacement an effect put on THIS unit ("Choose a friendly
    // unit. The next time it would die this turn, you may pay [C] to ...
    // instead" — Unlicensed Armory). It belongs to the unit, so it applies
    // whether or not the card that made it is still in play: both kill paths
    // ask `card`'s Card::applyReplacement, for `controller`, before any other
    // replacement (EffectExecutor::applyDeathReplacements). One-shot;
    // ignored once `turn` is over.
    struct PendingDeathReplacement {
        CardDefId card = kInvalidId;
        PlayerId controller = PlayerId::None;
        GameObjectId source = kInvalidId;
        int turn = -1;
    };
    std::vector<PendingDeathReplacement> pending_death_replacements;

    // "If it would die this turn, banish it instead." (Smite 568) Both kill
    // paths consult this before the death happens: the unit is banished, so it
    // does not die (no death triggers, no trash). Lasts until end of turn.
    bool banish_if_would_die_this_turn = false;

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
    // The continuous form ("Units can't move to base." — Minotaur Reckoner):
    // cleared at the start of every aura pass and re-asserted by the source's
    // Card::applyPassiveAura, so it ends when the source leaves the board.
    // (aura_cant_move_to_base: declared in the aura block below)
    // "It can't be moved this turn" / "They can't move it this turn" (Vex,
    // Apathetic): no Standard Move and no move by an effect of its
    // controller's until the turn ends (cleared with the other "this turn"
    // statuses in the Expiration Step). Read by the Standard Move generator.
    bool cant_move_this_turn = false;

    // ── Take-control reversion (CR 416, Phase 6q+) ──
    // EffectExecutor::takeControl with `until_end_of_turn=true` snapshots
    // the original controller into `original_controller_eot` and flips
    // this flag. Expiration step (end-of-turn) restores `controller` from
    // the snapshot and clears the flag. Permanent control changes do NOT
    // set the flag.
    // (control_reverts_eot: declared at the top)
    PlayerId original_controller_eot = PlayerId::None;

    // ── Take-control reversion "until I leave the board" (Akshan 431) ──
    // When an effect takes control of this object only "until <source> leaves
    // the board", these record the controlling source and where control
    // returns. GameEngine::revertLapsedControl (run in cleanup) restores
    // `controller` and clears these once the source object is no longer in
    // play. Independent of the end-of-turn variant above.
    // (control_reverts_on_source_leave: declared at the top)
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
        int bonus_damage_taken = 0;        // +N to spell/ability damage dealt to me (Void Gate)
        // "... have [Keyword] if they didn't already": dropped by
        // GameEngine::recalculateAuras, once every hook of the pass has run,
        // from an object that has `keyword` from any other source (see
        // auraKeywordIfAbsent in card_helpers.h).
        bool only_if_absent = false;
    };
    // ── What an aura pass resets ──
    // GameEngine::recalculateAuras clears these on every object of the game at
    // the start of every pass (a continuous effect that has ended must end
    // wherever its card is now). They are declared together so that the clear
    // writes two cache lines of an object instead of eight (plans/09 item
    // 9.KK3); each is documented where it used to be declared, above.
    int empowered = 0;
    int empower_flip_eot = 0;
    int empower_cost_reduction = 0;
    bool prevent_enemy_spell_ability_damage = false;
    bool cant_be_moved_by_enemy = false;
    bool immune_to_damage = false;
    int spells_targeting_me_cost_reduction = 0;
    std::vector<GrantedAbilityRef> granted_abilities;
    std::vector<GrantedAbilityRef> temp_granted_abilities;
    bool aura_cant_move_to_base = false;
    std::vector<AuraEffect> aura_effects;  // active auras affecting this object
    // (aura_might_bonus, the cached sum of aura might bonuses: declared at the top)
    KeywordSet aura_keywords;              // cached union of aura-granted keywords
    bool aura_no_combat_damage = false;    // cached: any aura suppresses combat dmg
    int aura_bonus_damage_taken = 0;       // cached: +N spell/ability damage taken (Void Gate)

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
    // Keywords an effect GAVE this object for as long as it stays what it is
    // ("Give a gear [Temporary]" — not a "this turn" grant, those are
    // temp_keywords). Set beside `keywords` by the card that gives one, so a
    // rule that treats printed text differently can tell a given keyword from
    // a printed one even when the card prints it too (CR 718.2: only the
    // PRINTED rules text of an attached card is Inactive).
    KeywordSet granted_keywords;

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
    // CR 185.1: "Token" is an intrinsic category of game object, not a
    // supertype: a token stays a token however it is copied or altered, and a
    // card can never become one. is_token is that intrinsic bit; the supertype
    // check keeps token card defs (Gold, Recruit, ...) working.
    bool isToken() const { return is_token || super_type == SuperType::Token; }

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
