// Manual equip card implementations — these override the generated stubs.
// Each card handles its own equip cost payment and effect text via Card virtuals.

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace riftbound {

// ─── Helper: standard equip (pay domain power, attach) ─────────────────────

static bool standardEquip(CardContext& ctx, GameObjectId gear_id, GameObjectId unit_id,
                           int energy_cost, Domain domain) {
    auto& state = ctx.state;
    auto player = ctx.controller;
    auto& ps = state.player(player);
    auto base_loc = BaseLocation{player};

    // Pay power cost: recycle a matching-domain rune
    if (energy_cost > 0) {
        // Energy portion: exhaust ready runes
        int e_remaining = energy_cost;
        for (auto& [id, obj] : state.objects) {
            if (e_remaining <= 0) break;
            if (!obj.isRune() || obj.controller != player || obj.is_exhausted) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            obj.is_exhausted = true;
            e_remaining--;
        }
    }

    // Recycle a rune for domain power
    for (auto& [id, obj] : state.objects) {
        if (!obj.isRune() || obj.controller != player) continue;
        if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
        bool match = false;
        for (auto d : obj.domains) {
            if (d == domain) { match = true; break; }
        }
        if (!match) continue;
        ctx.events.logTrace("  EQUIP_COST: recycled " + obj.name + " for power");
        obj.location = std::nullopt;
        obj.zone = ZoneType::RuneDeck;
        ps.rune_deck.insert(ps.rune_deck.begin(), id);
        break;
    }

    // Attach
    auto& gear = state.getObject(gear_id);
    auto& unit = state.getObject(unit_id);
    ctx.events.logTrace("EQUIP: " + gear.name + " -> " + unit.name +
                         " (might_bonus=" + std::to_string(gear.might_bonus) + ")");

    gear.attached_to = unit_id;
    unit.attachments.push_back(gear_id);
    gear.location = unit.location;
    gear.zone = unit.zone;
    unit.attachment_might_bonus += gear.might_bonus;
    unit.recomputeMight();

    ctx.events.emit(ObjectStateChangedEvent{gear_id, "attached"});
    ctx.events.emit(ObjectStateChangedEvent{unit_id, "equipped"});
    return true;
}

// ─── Simple equip gear (domain power only, keywords, triggers) ──────────────

/// Template for simple equip gear: pay [DOMAIN] power, grant keywords/triggers
class SimpleEquipGear : public GearCard {
public:
    SimpleEquipGear(CardDefId id, Domain equip_domain, int energy_cost = 0)
        : GearCard(id), equip_domain_(equip_domain), energy_cost_(energy_cost) {}

    bool hasEquipAbility() const override { return true; }

    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        return standardEquip(ctx, ctx.source, unit, energy_cost_, equip_domain_);
    }

protected:
    Domain equip_domain_;
    int energy_cost_;
};

/// Equip with [A] (any domain — recycle any rune)
class UniversalEquipGear : public GearCard {
public:
    UniversalEquipGear(CardDefId id, int energy_cost = 0)
        : GearCard(id), energy_cost_(energy_cost) {}

    bool hasEquipAbility() const override { return true; }

    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        auto& state = ctx.state;
        auto player = ctx.controller;
        auto& ps = state.player(player);
        auto base_loc = BaseLocation{player};

        // Pay energy
        for (int i = 0; i < energy_cost_; ++i) {
            for (auto& [id, obj] : state.objects) {
                if (!obj.isRune() || obj.controller != player || obj.is_exhausted) continue;
                if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
                obj.is_exhausted = true;
                break;
            }
        }

        // Recycle any rune for [A]
        for (auto& [id, obj] : state.objects) {
            if (!obj.isRune() || obj.controller != player) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            ctx.events.logTrace("  EQUIP_COST: recycled " + obj.name + " for [A]");
            obj.location = std::nullopt;
            obj.zone = ZoneType::RuneDeck;
            ps.rune_deck.insert(ps.rune_deck.begin(), id);
            break;
        }

        auto& gear = state.getObject(ctx.source);
        auto& unit_obj = state.getObject(unit);
        ctx.events.logTrace("EQUIP: " + gear.name + " -> " + unit_obj.name);

        gear.attached_to = unit;
        unit_obj.attachments.push_back(ctx.source);
        gear.location = unit_obj.location;
        gear.zone = unit_obj.zone;
        unit_obj.attachment_might_bonus += gear.might_bonus;
        unit_obj.recomputeMight();

        ctx.events.emit(ObjectStateChangedEvent{ctx.source, "attached"});
        ctx.events.emit(ObjectStateChangedEvent{unit, "equipped"});
        return true;
    }

protected:
    int energy_cost_;
};

// ─── Specific equip cards ───────────────────────────────────────────────────

// [332] Serrated Dirk — [Equip] [R], effect: [Assault 2]
class MESerrated : public SimpleEquipGear {
public:
    MESerrated() : SimpleEquipGear(332, Domain::Fury) {}
    int equippedAssault() const override { return 2; }
    KeywordSet equippedKeywords() const override { KeywordSet k; k.set(Keyword::Assault); return k; }
};

// [339] Recurve Bow — [Equip] [R], effect: When I attack or defend, deal 2 to enemy unit here
class MERecurveBow : public SimpleEquipGear {
public:
    MERecurveBow() : SimpleEquipGear(339, Domain::Fury) {}
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIAttackOrDefend; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        // Deal 2 to an enemy unit here
        auto bf = ctx.state.getObject(unit).battlefieldId();
        if (!bf) return;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            auto obf = obj.battlefieldId();
            if (obf && *obf == *bf) {
                ctx.executor.dealDamage(id, 2, ctx.source);
                break;
            }
        }
    }
};

// [345] Long Sword — [Equip] [R], might_bonus=2, no effect text
class MELongSword : public SimpleEquipGear {
public:
    MELongSword() : SimpleEquipGear(345, Domain::Fury) {}
};

// [356] Doran's Shield — [Equip] [G], might_bonus=1, effect: [Tank]
class MEDoransShield : public SimpleEquipGear {
public:
    MEDoransShield() : SimpleEquipGear(356, Domain::Calm) {}
    KeywordSet equippedKeywords() const override { KeywordSet k; k.set(Keyword::Tank); return k; }
};

// [387] Cloth Armor — [Equip] [B], effect: [Shield 2]
class MEClothArmor : public SimpleEquipGear {
public:
    MEClothArmor() : SimpleEquipGear(387, Domain::Mind) {}
    int equippedShield() const override { return 2; }
    KeywordSet equippedKeywords() const override { KeywordSet k; k.set(Keyword::Shield); return k; }
};

// [417] Doran's Blade — [Equip] [O], might_bonus=2
class MEDoransBlade : public SimpleEquipGear {
public:
    MEDoransBlade() : SimpleEquipGear(417, Domain::Body) {}
};

// [424] Hexdrinker — [Equip] [O], might_bonus=1, effect: [Deflect]
class MEHexdrinker : public SimpleEquipGear {
public:
    MEHexdrinker() : SimpleEquipGear(424, Domain::Body) {}
    KeywordSet equippedKeywords() const override { KeywordSet k; k.set(Keyword::Deflect); return k; }
};

// [430] Warmog's Armor — [Equip] [O], might_bonus=1, effect: When I conquer, buff me
class MEWarmogsArmor : public SimpleEquipGear {
public:
    MEWarmogsArmor() : SimpleEquipGear(430, Domain::Body) {}
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIConquer; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        ctx.executor.buffUnit(unit);
    }
};

// [437] Trinity Force — [Equip] [O], might_bonus=2, effect: When I hold, score 1 point
class METrinityForce : public SimpleEquipGear {
public:
    METrinityForce() : SimpleEquipGear(437, Domain::Body) {}
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIHold; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        auto& ps = ctx.state.player(ctx.controller);
        ps.score++;
        ctx.events.logTrace("EQUIP_TRIGGER: Trinity Force scores 1 point -> " +
                            std::to_string(ps.score));
    }
};

// [439] Boneshiver — [Equip] [1][O], might_bonus=2, effect: When I conquer, channel 1 rune exhausted
class MEBoneshiver : public SimpleEquipGear {
public:
    MEBoneshiver() : SimpleEquipGear(439, Domain::Body, 1) {}
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIConquer; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        ctx.executor.channelRunes(ctx.controller, 1, true);
    }
};

// [445] Doran's Ring — [Equip] [P], might_bonus=1, effect: When I conquer, discard 1, draw 1
class MEDoransRing : public SimpleEquipGear {
public:
    MEDoransRing() : SimpleEquipGear(445, Domain::Chaos) {}
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIConquer; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        ctx.executor.discardCards(ctx.controller, 1);
        ctx.executor.drawCards(ctx.controller, 1);
    }
};

// [454] Boots of Swiftness — [Equip] [P], might_bonus=2, effect: [Ganking]
class MEBootsOfSwiftness : public SimpleEquipGear {
public:
    MEBootsOfSwiftness() : SimpleEquipGear(454, Domain::Chaos) {}
    KeywordSet equippedKeywords() const override { KeywordSet k; k.set(Keyword::Ganking); return k; }
};

// [455] Cull — [Equip] [P], might_bonus=1, effect: When I conquer, play a Gold gear token
class MECull : public SimpleEquipGear {
public:
    MECull() : SimpleEquipGear(455, Domain::Chaos) {}
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIConquer; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        auto loc = ctx.state.getObject(unit).location.value_or(BaseLocation{ctx.controller});
        ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold", 0, {}, {}, loc);
    }
};

// [471] Last Rites — [Equip] [P] + Recycle 2 from trash, might_bonus=2
// effect: When I conquer or hold, play a unit from trash (pay costs)
class MELastRites : public SimpleEquipGear {
public:
    MELastRites() : SimpleEquipGear(471, Domain::Chaos) {}

    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        // Additional cost: recycle 2 cards from trash
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.trash.size() < 2) return false; // can't pay

        // Recycle 2 from trash
        for (int i = 0; i < 2 && !ps.trash.empty(); ++i) {
            auto card_id = ps.trash.back();
            ps.trash.pop_back();
            ctx.events.logTrace("  EQUIP_COST: recycled " +
                                 ctx.state.getObject(card_id).name + " from trash");
            ctx.state.getObject(card_id).zone = ZoneType::MainDeck;
            ps.main_deck.insert(ps.main_deck.begin(), card_id);
        }

        // Standard domain power cost
        return standardEquip(ctx, ctx.source, unit, 0, Domain::Chaos);
    }

    TriggerType equippedTriggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        // Play a unit from trash (simplified — random agent picks from back)
        auto& ps = ctx.state.player(ctx.controller);
        for (int i = static_cast<int>(ps.trash.size()) - 1; i >= 0; --i) {
            auto card_id = ps.trash[i];
            if (!ctx.state.objectExists(card_id)) continue;
            auto& obj = ctx.state.getObject(card_id);
            if (!obj.isUnit()) continue;

            ctx.events.logTrace("EQUIP_TRIGGER: Last Rites plays " + obj.name + " from trash");
            ctx.executor.playIgnoringCost(ctx.controller, card_id);
            ps.trash.erase(ps.trash.begin() + i);
            break;
        }
    }
};

// [474] Eye of the Herald — [Equip] [Y], effect: When I move, play a 1M Recruit token here
class MEEyeOfHerald : public SimpleEquipGear {
public:
    MEEyeOfHerald() : SimpleEquipGear(474, Domain::Order) {}
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIMove; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        auto loc = ctx.state.getObject(unit).location.value_or(BaseLocation{ctx.controller});
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit", 1,
                                  {"Recruit"}, {}, loc, true);
    }
};

// [482] B.F. Sword — [Equip] [Y], might_bonus=3, no effect
class MEBFSword : public SimpleEquipGear {
public:
    MEBFSword() : SimpleEquipGear(482, Domain::Order) {}
};

// [493] Sacred Shears — [Equip] [Y], might_bonus=1, effect: [Deathknell] — Draw 1
class MESacredShears : public SimpleEquipGear {
public:
    MESacredShears() : SimpleEquipGear(493, Domain::Order) {}
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIDie; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        ctx.executor.drawCards(ctx.controller, 1);
    }
};

// [504] Spinning Axe — [Equip] [A], might_bonus=3
class MESpinningAxe : public UniversalEquipGear {
public:
    MESpinningAxe() : UniversalEquipGear(504) {}
};

// [507] Forgefire Cape — [Equip] [A], might_bonus=3, When I attack or defend, deal 2 to all enemy units here
class MEForgefireCape : public UniversalEquipGear {
public:
    MEForgefireCape() : UniversalEquipGear(507) {}
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIAttackOrDefend; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        auto bf = ctx.state.getObject(unit).battlefieldId();
        if (!bf) return;
        std::vector<GameObjectId> enemies;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            auto obf = obj.battlefieldId();
            if (obf && *obf == *bf) enemies.push_back(id);
        }
        for (auto eid : enemies) ctx.executor.dealDamage(eid, 2, ctx.source);
    }
};

// [658] Hunter's Machete — [Equip] [O], might_bonus=2, effect: [Hunt] (conquer/hold = +1 XP)
class MEHuntersMachete : public SimpleEquipGear {
public:
    MEHuntersMachete() : SimpleEquipGear(658, Domain::Body) {}
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        ctx.state.player(ctx.controller).xp += 1;
        ctx.events.logTrace("EQUIP_TRIGGER: Hunter's Machete +1 XP");
    }
};

// ─── Remaining equip gear (simple patterns) ────────────────────────────────

// [353] Skyfall of Areion — [Equip] [1][R], might_bonus=2
class MESkyfallAreion : public SimpleEquipGear {
public: MESkyfallAreion() : SimpleEquipGear(353, Domain::Fury, 1) {} };

// [365] Brutalizer — [Equip] [G], might_bonus=1
class MEBrutalizer : public SimpleEquipGear {
public: MEBrutalizer() : SimpleEquipGear(365, Domain::Calm) {} };

// [374] Guardian Angel — [Equip] [G], might_bonus=1 (replacement effect in effect_text)
class MEGuardianAngel : public SimpleEquipGear {
public: MEGuardianAngel() : SimpleEquipGear(374, Domain::Calm) {} };

// [379] Sterak's Gage — [Equip] [G], might_bonus=3
class MESteraksGage : public SimpleEquipGear {
public: MESteraksGage() : SimpleEquipGear(379, Domain::Calm) {} };

// [382] Svellsongur — [Equip] [1][G], might_bonus=0
class MESvellsongur : public SimpleEquipGear {
public: MESvellsongur() : SimpleEquipGear(382, Domain::Calm, 1) {} };

// [396] Experimental Hexplate — [Equip] [B], might_bonus=1
class MEExperimentalHexplate : public SimpleEquipGear {
public: MEExperimentalHexplate() : SimpleEquipGear(396, Domain::Mind) {} };

// [408] World Atlas — [Equip] [B], might_bonus=2, effect: When I hold, play 2 Gold tokens
class MEWorldAtlas : public SimpleEquipGear {
public:
    MEWorldAtlas() : SimpleEquipGear(408, Domain::Mind) {}
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIHold; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        auto loc = ctx.state.getObject(unit).location.value_or(BaseLocation{ctx.controller});
        ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold", 0, {}, {}, loc);
        ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold", 0, {}, {}, loc);
    }
};

// [412] The Zero Drive — [Equip] [1][B], might_bonus=2
class MEZeroDrive : public SimpleEquipGear {
public: MEZeroDrive() : SimpleEquipGear(412, Domain::Mind, 1) {} };

// [460] Edge of Night — [Equip] [C] (recycle self), might_bonus=2
// Special: [C] cost means recycle the gear itself — but equip attaches it, contradiction
// Per rules: [C] likely means recycle self to activate some OTHER ability, not equip
// Treat as simple equip with [P] domain
class MEEdgeOfNight : public SimpleEquipGear {
public: MEEdgeOfNight() : SimpleEquipGear(460, Domain::Chaos) {} };

// [498] Blade of the Ruined King — [Equip] [Y] + Kill a friendly unit, might_bonus=4
class MEBladeOfRuinedKing : public SimpleEquipGear {
public: MEBladeOfRuinedKing() : SimpleEquipGear(498, Domain::Order) {} };

// [509] Shurelya's Requiem — [Equip] [A], might_bonus=2, effect: units here have [Ganking]
class MEShurelyasRequiem : public UniversalEquipGear {
public: MEShurelyasRequiem() : UniversalEquipGear(509) {} };

// [581] Blighted Battleaxe — [Equip] [1][R], might_bonus=4
class MEBlightedBattleaxe : public SimpleEquipGear {
public: MEBlightedBattleaxe() : SimpleEquipGear(581, Domain::Fury, 1) {} };

// [601] Soul Sword — [Equip] [G], might_bonus=1
class MESoulSword : public SimpleEquipGear {
public: MESoulSword() : SimpleEquipGear(601, Domain::Calm) {} };

// [720] Shepherd's Heirloom — [Equip] cost unknown, might_bonus=2
class MEShepherdsHeirloom : public SimpleEquipGear {
public: MEShepherdsHeirloom() : SimpleEquipGear(720, Domain::Order) {} };

// [748] Hextech Gauntlets — [Equip] [3][A], might_bonus=3
class MEHextechGauntlets : public UniversalEquipGear {
public: MEHextechGauntlets() : UniversalEquipGear(748, 3) {} };

// [508] Rabadon's Deathcrown — [Equip] [A], might_bonus=3
class MERabadonsDeathcrown : public UniversalEquipGear {
public: MERabadonsDeathcrown() : UniversalEquipGear(508) {} };

// ─── Registration ───────────────────────────────────────────────────────────

void registerManualEquipCards(CardRegistry& registry) {
    registry.registerCard(332, std::make_unique<MESerrated>());
    registry.registerCard(339, std::make_unique<MERecurveBow>());
    registry.registerCard(345, std::make_unique<MELongSword>());
    registry.registerCard(356, std::make_unique<MEDoransShield>());
    registry.registerCard(387, std::make_unique<MEClothArmor>());
    registry.registerCard(417, std::make_unique<MEDoransBlade>());
    registry.registerCard(424, std::make_unique<MEHexdrinker>());
    registry.registerCard(430, std::make_unique<MEWarmogsArmor>());
    registry.registerCard(437, std::make_unique<METrinityForce>());
    registry.registerCard(439, std::make_unique<MEBoneshiver>());
    registry.registerCard(445, std::make_unique<MEDoransRing>());
    registry.registerCard(454, std::make_unique<MEBootsOfSwiftness>());
    registry.registerCard(455, std::make_unique<MECull>());
    registry.registerCard(471, std::make_unique<MELastRites>());
    registry.registerCard(474, std::make_unique<MEEyeOfHerald>());
    registry.registerCard(482, std::make_unique<MEBFSword>());
    registry.registerCard(493, std::make_unique<MESacredShears>());
    registry.registerCard(504, std::make_unique<MESpinningAxe>());
    registry.registerCard(507, std::make_unique<MEForgefireCape>());
    registry.registerCard(658, std::make_unique<MEHuntersMachete>());
    // Additional equip cards
    registry.registerCard(353, std::make_unique<MESkyfallAreion>());
    registry.registerCard(365, std::make_unique<MEBrutalizer>());
    registry.registerCard(374, std::make_unique<MEGuardianAngel>());
    registry.registerCard(379, std::make_unique<MESteraksGage>());
    registry.registerCard(382, std::make_unique<MESvellsongur>());
    registry.registerCard(396, std::make_unique<MEExperimentalHexplate>());
    registry.registerCard(408, std::make_unique<MEWorldAtlas>());
    registry.registerCard(412, std::make_unique<MEZeroDrive>());
    registry.registerCard(460, std::make_unique<MEEdgeOfNight>());
    registry.registerCard(498, std::make_unique<MEBladeOfRuinedKing>());
    registry.registerCard(508, std::make_unique<MERabadonsDeathcrown>());
    registry.registerCard(509, std::make_unique<MEShurelyasRequiem>());
    registry.registerCard(581, std::make_unique<MEBlightedBattleaxe>());
    registry.registerCard(601, std::make_unique<MESoulSword>());
    registry.registerCard(720, std::make_unique<MEShepherdsHeirloom>());
    registry.registerCard(748, std::make_unique<MEHextechGauntlets>());
}

} // namespace riftbound
