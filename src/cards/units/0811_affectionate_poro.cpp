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

class AffectionatePoro : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When a combat that I was in ends, if I haven't been dealt damage this
    //  turn, draw 1." (GameObject::damage_taken_this_turn counts spell, ability
    //  and combat damage and is reset at end of turn.)
    TriggerType triggerType() const override { return TriggerType::WhenACombatIWasInEnds; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (ctx.state.getObject(ctx.source).damage_taken_this_turn > 0) return;
        ctx.executor.drawCards(ctx.controller, 1);
        RIFT_TRACE(ctx.events, "AFFECTIONATE PORO: combat ended undamaged -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 811;
        d.def_id = R"RB(ven-024-166)RB";
        d.name = R"RB(Affectionate Poro)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-024/166)RB";
        d.collector_number = 24;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Poro)RB", R"RB(Noxus)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Common;
        d.ability_text = R"RB(When a combat that I was in ends, if I haven't been dealt damage this turn, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/aa5fdc995322af38f540629fcf695f1080a14afb-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_811(CardRegistry& r) {
    r.registerCard(811, std::make_unique<AffectionatePoro>());
}

}  // namespace riftbound
