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

class ShenScourgeOfShadows : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When I hold, if there is exactly one other unit you control here, draw 1."
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        if (!self.isAtBattlefield()) return;
        if (otherFriendlyUnitsHere(ctx.state, self) != 1) return;
        ctx.executor.drawCards(ctx.controller, 1);
        RIFT_TRACE(ctx.events, "SHEN: held with exactly one other friendly unit -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 829;
        d.def_id = R"RB(ven-042-166)RB";
        d.name = R"RB(Shen, Scourge of Shadows)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-042/166)RB";
        d.collector_number = 42;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Shen)RB", R"RB(Ionia)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I hold, if there is exactly one other unit you control here, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bbe849918c7f4189379ae8957dd427a3f272a1a6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_829(CardRegistry& r) {
    r.registerCard(829, std::make_unique<ShenScourgeOfShadows>());
}

}  // namespace riftbound
