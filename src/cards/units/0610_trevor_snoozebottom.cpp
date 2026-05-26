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

class TrevorSnoozebottom : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto bf_id = ctx.state.getObject(ctx.source).battlefieldId();
        if (!bf_id) return;
        KeywordSet kw; kw.set(Keyword::Temporary);
        auto loc = LocationId{BattlefieldLocation{*bf_id}};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, loc, true);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 610;
        d.def_id = R"RB(unl-048-219)RB";
        d.name = R"RB(Trevor Snoozebottom)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-048/219)RB";
        d.collector_number = 48;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Yordle)RB", R"RB(Ionia)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.shield_value = 1;
        d.keywords.set(Keyword::Shield);
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB([Shield] (+1 [M] while I'm a defender.)
When I hold, play a ready 3 [M] Sprite unit token with [Temporary] here. (Kill it at the start of its controller's next Beginning Phase, before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fa843ee80d1a35416d61482bc1602279955a2c7f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_610(CardRegistry& r) {
    r.registerCard(610, std::make_unique<TrevorSnoozebottom>());
}

} // namespace riftbound
