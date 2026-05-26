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

class PoroHerder : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, if you control a Poro, buff me and draw 1."
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        bool controls_poro = false;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (hasTag(obj, "Poro")) { controls_poro = true; break; }
        }
        if (!controls_poro) return;
        ctx.executor.buffUnit(ctx.source);  // +1 [M] buff if it has none
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("PORO HERDER: control a Poro -> buff me + draw 1");
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 61;
        d.def_id = R"RB(ogn-061-298)RB";
        d.name = R"RB(Poro Herder)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-061/298)RB";
        d.collector_number = 61;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Freljord)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, if you control a Poro, buff me and draw 1. (If I don't have a buff, I get a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/166b718a8517fcceac7e7d4f6acbc4fa0bbc2c55-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_61(CardRegistry& r) {
    r.registerCard(61, std::make_unique<PoroHerder>());
}

} // namespace riftbound
