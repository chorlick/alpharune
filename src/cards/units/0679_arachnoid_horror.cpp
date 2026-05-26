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

class ArachnoidHorror : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.state.player(ctx.controller).xp += 2;
    }
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 679;
        d.def_id = R"RB(unl-117-219)RB";
        d.name = R"RB(Arachnoid Horror)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-117/219)RB";
        d.collector_number = 117;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Shadow Isles)RB", R"RB(Spider)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Hunt);
        d.ability_text = R"RB([Hunt 2] (When I conquer or hold, gain 2 XP.)
I can be played to an occupied battlefield if an enemy unit is alone there.
Friendly units can be played to an occupied battlefield if an enemy unit is alone there.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ae2ec829a6714ac75e8b203631af08c0c1e1565f-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_679(CardRegistry& r) {
    r.registerCard(679, std::make_unique<ArachnoidHorror>());
}

} // namespace riftbound
