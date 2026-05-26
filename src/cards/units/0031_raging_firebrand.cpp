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

class RagingFirebrand : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "the next spell you play this turn costs [5] less."
        PlayerState::CostModifier m;
        m.source = ctx.source;
        m.energy_reduction = 5;
        m.this_turn_only = true;
        m.next_spell_only = true;
        m.affects_friendly_only = true;
        ctx.state.player(ctx.controller).cost_modifiers.push_back(m);
        ctx.events.logTrace("RAGING FIREBRAND: next spell this turn costs [5] less");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 31;
        d.def_id = R"RB(ogn-031-298)RB";
        d.name = R"RB(Raging Firebrand)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-031/298)RB";
        d.collector_number = 31;
        d.artist = R"RB(JiHun Lee)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Dragon)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, the next spell you play this turn costs [5] less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9566fc064c098bd7f3540f3074dc6353c7ca5663-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_31(CardRegistry& r) {
    r.registerCard(31, std::make_unique<RagingFirebrand>());
}

} // namespace riftbound
