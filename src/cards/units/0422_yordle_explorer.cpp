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

class YordleExplorer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play a card with Power cost [A][A] or more, draw 1." ([A][A] = 2 Power)
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayAUnit,
                TriggerType::WhenYouPlayASpell,
                TriggerType::WhenYouPlayAGear};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Find the most-recently played card on the chain (skip the trigger
        // ability items pushed on top of it).
        CardDefId played = kInvalidId;
        for (auto it = ctx.state.chain.items.rbegin(); it != ctx.state.chain.items.rend(); ++it) {
            if (it->is_spell || it->is_permanent) { played = it->card_def_id; break; }
        }
        if (played == kInvalidId) return;
        const auto& def = ctx.executor.cardDB().get(played);
        if (def.power_cost < 2) return;
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("YORDLE EXPLORER: played [A][A]+ card -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 422;
        d.def_id = R"RB(sfd-100-221)RB";
        d.name = R"RB(Yordle Explorer)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-100/221)RB";
        d.collector_number = 100;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Yordle)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.ability_text = R"RB(When you play a card with Power cost [A][A] or more, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/556b20c8c605545f13a8990dca36586a287ebb29-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_422(CardRegistry& r) {
    r.registerCard(422, std::make_unique<YordleExplorer>());
}

} // namespace riftbound
