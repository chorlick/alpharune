#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class LoyalPoro : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    // "I wasn't alone if there were other friendly units here." — judged from
    // the last-known information taken as I died (CR 383.2.c.3.b): for units
    // that die at the same time it is taken for all of them first, so a unit
    // that died with me was still "here".
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& lk = ctx.state.getObject(ctx.source).last_known;
        if (!lk.valid || lk.other_friendly_units_here.empty()) return;
        RIFT_TRACE(ctx.events, "LOYAL PORO: not alone at death — drawing 1");
        ctx.executor.drawCards(ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 718;
        d.def_id = R"RB(unl-156-219)RB";
        d.name = R"RB(Loyal Poro)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-156/219)RB";
        d.collector_number = 156;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Poro)RB", R"RB(Freljord)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell][>] If I didn't die alone, draw 1. (When I die, get the effect. I wasn't alone if there were other friendly units here.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/860e1afc7364a002c6305ece7b6941e2e328d0a4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_718(CardRegistry& r) {
    r.registerCard(718, std::make_unique<LoyalPoro>());
}

} // namespace riftbound
