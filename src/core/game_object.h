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

    // ── Aura effects (Phase 5b — tagged effects) ──
    struct AuraEffect {
        GameObjectId source = kInvalidId;  // aura source object
        int might_bonus = 0;               // +/- M from this aura
        int might_minimum = 0;             // minimum M (0 = no min)
        Keyword keyword = Keyword::Count;  // granted keyword (Count = none)
        int keyword_value = 0;             // e.g. Assault 1, Shield 2
    };
    std::vector<AuraEffect> aura_effects;  // active auras affecting this object
    int aura_might_bonus = 0;              // cached sum of aura might bonuses
    KeywordSet aura_keywords;              // cached union of aura-granted keywords

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
        current_might = base_might + buff_count + attachment_might_bonus + aura_might_bonus;
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
