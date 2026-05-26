#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "When I attack or defend, deal damage equal to my Might to an enemy unit here.
//  I don't deal combat damage.
//  [B]: [Action] — Move me to your base." ([B] = Mind power.)

class EzrealDashing : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // ── Trigger: deal damage = my Might to an enemy unit here ──
    TriggerType triggerType() const override { return TriggerType::WhenIAttackOrDefend; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return;
        std::vector<GameObjectId> here;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            if (obj.battlefieldId() != my_bf || obj.untargetable_by_enemy) continue;
            here.push_back(id);
        }
        GameObjectId tgt = pickTarget(ctx, "Ezreal: deal Might damage to an enemy here", here);
        if (tgt == kInvalidId || !ctx.state.objectExists(tgt)) return;
        int dmg = ctx.state.getObject(ctx.source).current_might;
        ctx.executor.dealDamage(tgt, dmg, ctx.source);
        if (ctx.state.objectExists(tgt) && ctx.state.getObject(tgt).hasLethalDamage())
            ctx.executor.killObject(tgt);
        ctx.events.logTrace("EZREAL DASHING: dealt " + std::to_string(dmg) +
                            " (=Might) to an enemy here");
    }

    // ── "I don't deal combat damage." ──
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId() || obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            GameObject::AuraEffect ae;
            ae.source = id;
            ae.suppress_combat_damage = true;
            obj.aura_effects.push_back(ae);
        }
    }

    // ── "[B]: [Action] — Move me to your base." ──
    bool hasActivatedAbility() const override { return true; }
    bool isActionAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.power = 1, .power_domain = Domain::Mind};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.moveToBase(ctx.source);
        ctx.events.logTrace("EZREAL DASHING: [B] -> move me to base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 404;
        d.def_id = R"RB(sfd-082-221)RB";
        d.name = R"RB(Ezreal, Dashing)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-082/221)RB";
        d.collector_number = 82;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Ezreal)RB", R"RB(Piltover)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB(When I attack or defend, deal damage equal to my Might to an enemy unit here.
I don't deal combat damage.
[B]: [Action] — Move me to your base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3e040f7b7ca16009a2558674a911b8cc93122ba3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_404(CardRegistry& r) {
    r.registerCard(404, std::make_unique<EzrealDashing>());
}

} // namespace riftbound
