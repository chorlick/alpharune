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

class Undertitan : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, give your other units +2 [M] this turn.
    //  As I'm revealed from your deck, [Add] [2]." ([2] = 2 Energy)
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIRevealedFromTop};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.firing_trigger == TriggerType::WhenIRevealedFromTop) {
            ctx.executor.addFloatingEnergy(ctx.controller, 2);
            ctx.events.logTrace("UNDERTITAN: revealed from deck -> Add [2] energy");
            return;
        }
        // WhenYouPlayMe: buff all OTHER friendly units +2 [M] this turn.
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source) continue;
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            ctx.executor.giveTemporaryMight(id, 2);
        }
        ctx.events.logTrace("UNDERTITAN: play -> other friendly units +2 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 495;
        d.def_id = R"RB(sfd-175-221)RB";
        d.name = R"RB(Undertitan)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-175/221)RB";
        d.collector_number = 175;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(The Void)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, give your other units +2 [M] this turn.
As I'm revealed from your deck, [Add] [2].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/aa3bd795a1fa015b42bf64a2d588f527342c2a15-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_495(CardRegistry& r) {
    r.registerCard(495, std::make_unique<Undertitan>());
}

} // namespace riftbound
