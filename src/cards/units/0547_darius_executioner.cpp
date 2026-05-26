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

class DariusExecutioner : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (ctx.state.player(ctx.controller).cards_played_this_turn < 2) return;
        ctx.executor.readyObject(ctx.source);
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    bool requiresLegion() const override { return true; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 547;
        d.def_id = R"RB(sfd-236-221)RB";
        d.name = R"RB(Darius, Executioner)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-236/221)RB";
        d.collector_number = 236;
        d.artist = R"RB(Shishizaru)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Trifarian)RB", R"RB(Darius)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Showcase;
        d.keywords.set(Keyword::Legion);
        d.ability_text = R"RB([Legion] — When you play me, ready me. (Get the effect if you've played another card this turn)
Other friendly units have +1 [M] here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4f550f321dce06bb09bb47682d5bf2981280ea16-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_547(CardRegistry& r) {
    r.registerCard(547, std::make_unique<DariusExecutioner>());
}

} // namespace riftbound
