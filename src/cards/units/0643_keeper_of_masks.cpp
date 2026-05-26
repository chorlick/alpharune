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

class KeeperOfMasks : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        // "here" = Keeper's current location (its battlefield, or base).
        LocationId loc = self.location.value_or(LocationId{BaseLocation{ctx.controller}});

        for (int i = 0; i < 2; ++i) {
            GameObjectId tok = ctx.executor.createToken(
                ctx.controller, CardType::Unit, "Reflection", /*might=*/0,
                /*tags=*/{}, /*kw=*/KeywordSet{}, loc, /*enter_ready=*/false);
            if (tok == kInvalidId || !ctx.state.objectExists(tok)) continue;
            // Become a copy of me (base traits — copyUnit inherits
            // card_def_id so death triggers still fire, per Mirror Image).
            ctx.executor.copyUnit(tok, ctx.source);
            ctx.events.logTrace("KEEPER OF MASKS: Reflection token copies me");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 643;
        d.def_id = R"RB(unl-081-219)RB";
        d.name = R"RB(Keeper of Masks)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-081/219)RB";
        d.collector_number = 81;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 2;
        d.might = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Hidden);
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
[Temporary] (Kill me at the start of my controller's Beginning Phase, before scoring.)
When you play me, play two Reflection unit tokens here. Then do this: They become copies of me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/67606d98bd3d8816e686dacacb91a56627d4b5f5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_643(CardRegistry& r) {
    r.registerCard(643, std::make_unique<KeeperOfMasks>());
}

} // namespace riftbound
