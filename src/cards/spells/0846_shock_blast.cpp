#include "cards/card.h"
#include "cards/card_helpers.h"
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

class ShockBlast : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "This costs [2] less if you control something that's [Empowered].
    //  Deal 4 to a unit at a battlefield."
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        for (const auto& [id, obj] : state.objects.inPlay()) {
            if (obj.controller != player || !obj.isEmpowered()) continue;
            if (obj.location.has_value() || obj.zone == ZoneType::LegendZone) return 2;
        }
        return 0;
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_at_battlefield = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Shock Blast", legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.dealDamage(picked, 4, ctx.source);
        if (ctx.state.objectExists(picked) && ctx.state.getObject(picked).hasLethalDamage())
            ctx.executor.killObject(picked);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 846;
        d.def_id = R"RB(ven-059-166)RB";
        d.name = R"RB(Shock Blast)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-059/166)RB";
        d.collector_number = 59;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
This costs [2] less if you control something that's [Empowered].
Deal 4 to a unit at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a8b976725e4722dcbf327cde9f92625cb516150e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_846(CardRegistry& r) {
    r.registerCard(846, std::make_unique<ShockBlast>());
}

}  // namespace riftbound
