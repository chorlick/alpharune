// Manual card implementations for cards appearing in test decks.
// These override the generated stubs for cards that do nothing.

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <algorithm>
#include <memory>

namespace riftbound {

// ═══════════════════════════════════════════════════════════════════════════════
// Champions
// ═══════════════════════════════════════════════════════════════════════════════

// [734] LeBlanc, Fragmented — [Assault], [Deathknell][>] Draw 1 (or 2 during Beginning)
class MLeBlanc : public UnitCard {
public:
    MLeBlanc() : UnitCard(734) {}
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        int draw_count = (ctx.state.turn.phase == TurnPhase::BeginningStep) ? 2 : 1;
        ctx.executor.drawCards(ctx.controller, draw_count);
    }
};

// [712] Vex, Apathetic — When opponent plays unit while I'm at BF, stun it
// Complex trigger — needs non-standard event. For now: keyword-only (Deflect handled by base)
class MVexApathetic : public UnitCard {
public:
    MVexApathetic() : UnitCard(712) {}
};

// [682] Rengar, Trophy Hunter — [Ambush] to BF with enemy units
class MRengarTrophy : public UnitCard {
public:
    MRengarTrophy() : UnitCard(682) {}
};

// [676] Nidalee, Cat Form — [Ambush], when I win combat draw 1
class MNidaleeCat : public UnitCard {
public:
    MNidaleeCat() : UnitCard(676) {}
};

// [66] Ahri, Alluring — When I hold, score 1 point
class MAhriAlluring : public UnitCard {
public:
    MAhriAlluring() : UnitCard(66) {}
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& ps = ctx.state.player(ctx.controller);
        ps.score++;
        ctx.events.logTrace("TRIGGER: Ahri scores 1 point -> " + std::to_string(ps.score));
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Legends
// ═══════════════════════════════════════════════════════════════════════════════

// [785] Gloomist — When you or ally hold, exhaust me to draw 1
class MGloomist : public LegendCard {
public:
    MGloomist() : LegendCard(785) {}
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& legend = ctx.state.getObject(ctx.source);
        if (legend.is_exhausted) return;
        legend.is_exhausted = true;
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("TRIGGER: Gloomist exhausts to draw 1");
    }
};

// [756] Deceiver — When conquer/hold, may discard 1 + exhaust to play Reflection token
class MDeceiver : public LegendCard {
public:
    MDeceiver() : LegendCard(756) {}
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& legend = ctx.state.getObject(ctx.source);
        if (legend.is_exhausted) return;
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.hand.empty()) return;
        // Exhaust and discard to create Reflection
        legend.is_exhausted = true;
        ctx.executor.discardCards(ctx.controller, 1);
        // Create a 0M Reflection token
        // For now, place at base (should be at conquered BF)
        auto loc = BaseLocation{ctx.controller};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Reflection", 0,
                                  {}, {}, loc, true);
        ctx.events.logTrace("TRIGGER: Deceiver creates Reflection token");
    }
};

// [506] Fire Below the Mountain — [E]: [Reaction] — [Add] [A] for gear
class MFireBelowMtn : public LegendCard {
public:
    MFireBelowMtn() : LegendCard(506) {}
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // Add [A] to rune pool (universal power)
        ctx.state.player(ctx.controller).rune_pool.universal_power++;
        ctx.events.logTrace("ACTIVATE: Fire Below the Mountain adds [A]");
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Deathknell units
// ═══════════════════════════════════════════════════════════════════════════════

// [629] Ruined Rex — [Deathknell][>] Deal 4 to enemy unit
class MRuinedRex : public UnitCard {
public:
    MRuinedRex() : UnitCard(629) {}
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // Deal 4 to an enemy unit at a battlefield
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            if (!obj.isAtBattlefield()) continue;
            ctx.executor.dealDamage(id, 4, ctx.source);
            if (ctx.state.objectExists(id) && ctx.state.getObject(id).hasLethalDamage())
                ctx.executor.killObject(id);
            break;
        }
    }
};

// [714] Black Rose Dignitary — [Assault], [Deathknell][>] Channel 1 rune exhausted
class MBlackRoseDig : public UnitCard {
public:
    MBlackRoseDig() : UnitCard(714) {}
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.executor.channelRunes(ctx.controller, 1, true);
    }
};

// [486] Glasc Mixologist — [Deathknell] play unit cost<=3 from trash
class MGlascMixologist : public UnitCard {
public:
    MGlascMixologist() : UnitCard(486) {}
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& ps = ctx.state.player(ctx.controller);
        for (int i = static_cast<int>(ps.trash.size()) - 1; i >= 0; --i) {
            auto cid = ps.trash[i];
            if (!ctx.state.objectExists(cid)) continue;
            auto& obj = ctx.state.getObject(cid);
            if (!obj.isUnit()) continue;
            if (obj.card_def_id == kInvalidId) continue;
            // Check cost <= 3 (simplified — use base might as proxy, or check CardDB)
            ctx.executor.playIgnoringCost(ctx.controller, cid);
            ps.trash.erase(ps.trash.begin() + i);
            break;
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Score/Phase trigger units
// ═══════════════════════════════════════════════════════════════════════════════

// [73] Sona, Harmonious — At end of turn, if at BF, ready up to 4 friendly runes
class MSona : public UnitCard {
public:
    MSona() : UnitCard(73) {}
    TriggerType triggerType() const override { return TriggerType::AtEndOfTurn; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (!self.isAtBattlefield()) return;
        int readied = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (readied >= 4) break;
            if (!obj.isRune() || obj.controller != ctx.controller) continue;
            if (!obj.is_exhausted) continue;
            if (!obj.location.has_value()) continue;
            ctx.executor.readyObject(id);
            readied++;
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Spells
// ═══════════════════════════════════════════════════════════════════════════════

// [209] Cull the Weak — Each player kills one of their units
class MCullTheWeak : public SpellCard {
public:
    MCullTheWeak() : SpellCard(209) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        for (auto pid : {PlayerId::Player1, PlayerId::Player2}) {
            auto units = ctx.state.allUnitsControlledBy(pid);
            if (!units.empty()) {
                // Kill last unit (agent choice in future)
                ctx.executor.killObject(units.back());
            }
        }
    }
};

// [45] Defy — Counter a spell, draw 1
class MDefy : public SpellCard {
public:
    MDefy() : SpellCard(45) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // Peek-and-pop counter
        if (!ctx.state.chain.items.empty()) {
            auto& top = ctx.state.chain.items.back();
            if (top.is_spell) {
                auto countered = top.source;
                ctx.state.chain.items.pop_back();
                if (ctx.state.objectExists(countered)) {
                    auto& obj = ctx.state.getObject(countered);
                    ctx.events.logTrace("COUNTER: " + obj.name + " countered by Defy -> trash");
                    obj.zone = ZoneType::Trash;
                    obj.location = std::nullopt;
                    ctx.state.player(obj.owner).trash.push_back(countered);
                }
            }
        }
        ctx.executor.drawCards(ctx.controller, 1);
    }
};

// [631] Sprite Burst — Play 2 ready 3M Sprite tokens with [Temporary]
class MSpriteBurst : public SpellCard {
public:
    MSpriteBurst() : SpellCard(631) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        KeywordSet kw; kw.set(Keyword::Temporary);
        auto loc = BaseLocation{ctx.controller};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, loc, true);
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, loc, true);
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Simple units with static effects (keyword-only but marked complex by codegen)
// ═══════════════════════════════════════════════════════════════════════════════

// [560] Inferna — [Ambush] [Assault 2] (keywords only, no trigger)
class MInferna : public UnitCard {
public: MInferna() : UnitCard(560) {} };

// [745] Thrill of the Hunt — Banish friendly unit, play it to any BF ignoring cost
class MThrillOfTheHunt : public SpellCard {
public:
    MThrillOfTheHunt() : SpellCard(745) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty()) return;
        auto unit_id = targets[0];
        if (!ctx.state.objectExists(unit_id)) return;
        auto& unit = ctx.state.getObject(unit_id);

        // Step 1: Banish the friendly unit
        ctx.events.logTrace("THRILL: banishing " + unit.name);
        ctx.executor.banishObject(unit_id);

        // Step 2: Play it from banishment to any battlefield, ignoring cost
        // Remove from banishment
        auto& ps = ctx.state.player(ctx.controller);
        auto it = std::find(ps.banishment.begin(), ps.banishment.end(), unit_id);
        if (it != ps.banishment.end()) ps.banishment.erase(it);

        // Pick a battlefield (agent choice in future, for now: first BF)
        BattlefieldId target_bf = 0;
        if (!ctx.state.battlefields.empty()) {
            target_bf = ctx.state.battlefields[0].id;
        }

        // Place on battlefield ready (ignoring cost)
        unit.zone = ZoneType::BattlefieldZone;
        unit.location = BattlefieldLocation{target_bf};
        unit.controller = ctx.controller;
        unit.is_exhausted = false; // enters ready
        unit.damage_marked = 0;
        unit.recomputeMight();

        ctx.events.logTrace("THRILL: " + unit.name + " played to BF#" +
                             std::to_string(target_bf) + " ready, ignoring cost");
        ctx.events.emit(EnteredBoardEvent{unit_id, ctx.controller, CardType::Unit,
            BattlefieldLocation{target_bf}, true});
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
};

// [176] Sneaky Deckhand — keywords only
class MSneakyDeckhand : public UnitCard {
public: MSneakyDeckhand() : UnitCard(176) {} };

// [395] Dropboarder — keywords only
class MDropboarder : public UnitCard {
public: MDropboarder() : UnitCard(395) {} };

// [106] Sprite Mother — When you play me, play a 3M Sprite token here
class MSpriteMotherUnit : public UnitCard {
public:
    MSpriteMotherUnit() : UnitCard(106) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        KeywordSet kw; kw.set(Keyword::Temporary);
        auto loc = ctx.state.getObject(ctx.source).location.value_or(BaseLocation{ctx.controller});
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, loc, true);
    }
};

// [91] Pit Crew — When you play a gear, ready me (non-standard trigger — skip for now)
class MPitCrew : public UnitCard {
public: MPitCrew() : UnitCard(91) {} };

// [136] Pit Rookie — When you play me, buff another friendly unit
class MPitRookie : public UnitCard {
public:
    MPitRookie() : UnitCard(136) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // Buff another friendly unit
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source) continue;
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            ctx.executor.buffUnit(id);
            break;
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Gear (non-equip)
// ═══════════════════════════════════════════════════════════════════════════════

// [538] Seal of Focus — When you play this, ready a friendly unit
class MSealOfFocus : public GearCard {
public:
    MSealOfFocus() : GearCard(538) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value() || !obj.is_exhausted) continue;
            ctx.executor.readyObject(id);
            break;
        }
    }
};

// [542] Seal of Strength — When you play this, buff a friendly unit
class MSealOfStrength : public GearCard {
public:
    MSealOfStrength() : GearCard(542) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            ctx.executor.buffUnit(id);
            break;
        }
    }
};

// [573] Fresh Beans — When you play a unit during showdown, exhaust to draw 1
// Non-standard trigger — skip for now
class MFreshBeans : public GearCard {
public: MFreshBeans() : GearCard(573) {} };

// [640] Sprite Fountain — When you play this, play a 3M Sprite token
class MSpriteFountain : public GearCard {
public:
    MSpriteFountain() : GearCard(640) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        KeywordSet kw; kw.set(Keyword::Temporary);
        auto loc = BaseLocation{ctx.controller};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, loc, true);
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Registration
// ═══════════════════════════════════════════════════════════════════════════════

void registerManualDeckCards(CardRegistry& registry) {
    // Champions
    registry.registerCard(734, std::make_unique<MLeBlanc>());
    registry.registerCard(712, std::make_unique<MVexApathetic>());
    registry.registerCard(682, std::make_unique<MRengarTrophy>());
    registry.registerCard(676, std::make_unique<MNidaleeCat>());
    registry.registerCard(66, std::make_unique<MAhriAlluring>());
    // Legends
    registry.registerCard(785, std::make_unique<MGloomist>());
    registry.registerCard(756, std::make_unique<MDeceiver>());
    registry.registerCard(506, std::make_unique<MFireBelowMtn>());
    // Deathknell units
    registry.registerCard(629, std::make_unique<MRuinedRex>());
    registry.registerCard(714, std::make_unique<MBlackRoseDig>());
    registry.registerCard(486, std::make_unique<MGlascMixologist>());
    // Score/phase triggers
    registry.registerCard(73, std::make_unique<MSona>());
    // Spells
    registry.registerCard(209, std::make_unique<MCullTheWeak>());
    registry.registerCard(45, std::make_unique<MDefy>());
    registry.registerCard(631, std::make_unique<MSpriteBurst>());
    // Simple units
    registry.registerCard(560, std::make_unique<MInferna>());
    registry.registerCard(176, std::make_unique<MSneakyDeckhand>());
    registry.registerCard(395, std::make_unique<MDropboarder>());
    registry.registerCard(106, std::make_unique<MSpriteMotherUnit>());
    registry.registerCard(91, std::make_unique<MPitCrew>());
    registry.registerCard(136, std::make_unique<MPitRookie>());
    // Gear
    registry.registerCard(538, std::make_unique<MSealOfFocus>());
    registry.registerCard(542, std::make_unique<MSealOfStrength>());
    registry.registerCard(573, std::make_unique<MFreshBeans>());
    registry.registerCard(640, std::make_unique<MSpriteFountain>());
    registry.registerCard(745, std::make_unique<MThrillOfTheHunt>());
}

} // namespace riftbound
