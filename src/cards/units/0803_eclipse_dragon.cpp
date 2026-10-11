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

class EclipseDragon : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Accelerate] is engine-handled (keyword).
    // "When I move, if you control 4 or fewer runes, draw 1."
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (runesControlled(ctx.state, ctx.controller) > 4) return;
        ctx.executor.drawCards(ctx.controller, 1);
        RIFT_TRACE(ctx.events, "ECLIPSE DRAGON: moved with 4 or fewer runes -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 803;
        d.def_id = R"RB(ven-016-166)RB";
        d.name = R"RB(Eclipse Dragon)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-016/166)RB";
        d.collector_number = 16;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Dragon)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 8;
        d.might = 8;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Accelerate);
        d.ability_text = R"RB([Accelerate] (You may pay [1][R] as an additional cost to have me enter ready.)
When I move, if you control 4 or fewer runes, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6b7ea97973ced0e0631554ff07c726fe0a257a3c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_803(CardRegistry& r) {
    r.registerCard(803, std::make_unique<EclipseDragon>());
}

}  // namespace riftbound
