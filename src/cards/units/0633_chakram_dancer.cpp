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

class ChakramDancer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        if (!self.location.has_value()) return;  // must be "here" at a location
        const auto here = *self.location;
        int granted = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source) continue;                  // "other" units
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value() || *obj.location != here) continue;
            ctx.executor.giveTemporaryKeyword(id, Keyword::Shield, 1);
            ++granted;
        }
        if (granted > 0) {
            ctx.events.logTrace("CHAKRAM DANCER: gave [Shield] this turn to " +
                                std::to_string(granted) + " other unit(s) here");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 633;
        d.def_id = R"RB(unl-071-219)RB";
        d.name = R"RB(Chakram Dancer)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-071/219)RB";
        d.collector_number = 71;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.shield_value = 1;
        d.keywords.set(Keyword::Ambush);
        d.keywords.set(Keyword::Reaction);
        d.keywords.set(Keyword::Shield);
        d.ability_text = R"RB([Ambush] (You may play me as a [Reaction] to a battlefield where you have units.)
When you play me, give your other units here [Shield] this turn. (+1 [M] while they're defenders.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/29abe06f1c16a9eed622a5060330116268b73b8c-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_633(CardRegistry& r) {
    r.registerCard(633, std::make_unique<ChakramDancer>());
}

} // namespace riftbound
