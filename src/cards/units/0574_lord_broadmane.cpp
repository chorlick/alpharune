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

// "When you play me, give your other units here [Assault] this turn."

class LordBroadmane : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source) continue;  // "other units"
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            auto bf = obj.battlefieldId();
            if (bf && *bf == *my_bf)
                ctx.executor.giveTemporaryKeyword(id, Keyword::Assault, 1);
        }
        ctx.events.logTrace("LORD BROADMANE: gave other friendly units here [Assault]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 574;
        d.def_id = R"RB(unl-012-219)RB";
        d.name = R"RB(Lord Broadmane)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-012/219)RB";
        d.collector_number = 12;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.assault_value = 1;
        d.keywords.set(Keyword::Ambush);
        d.keywords.set(Keyword::Assault);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Ambush] (You may play me as a [Reaction] to a battlefield where you have units.)
When you play me, give your other units here [Assault] this turn. (+1 [M] while they're attackers.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f564001228383a27372c0ddc4d8c0ed4e4843dc7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_574(CardRegistry& r) {
    r.registerCard(574, std::make_unique<LordBroadmane>());
}

} // namespace riftbound
