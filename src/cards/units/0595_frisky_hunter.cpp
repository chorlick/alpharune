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

class FriskyHunter : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        if (!self.location.has_value()) return;
        KeywordSet kw; kw.set(Keyword::Deflect);
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Bird",
                                  /*might=*/1, /*tags=*/{"Bird"}, kw,
                                  *self.location,
                                  /*enter_ready=*/false);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 595;
        d.def_id = R"RB(unl-033-219)RB";
        d.name = R"RB(Frisky Hunter)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-033/219)RB";
        d.collector_number = 33;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Cat)RB", R"RB(Zaun)RB"};
        d.energy_cost = 4;
        d.might = 3;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB(When you play me, play a 1 [M] Bird unit token with [Deflect] here. (Opponents must pay [A] to choose it with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6bf8978ca42fa50a60067e74364b3caf78b3e928-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_595(CardRegistry& r) {
    r.registerCard(595, std::make_unique<FriskyHunter>());
}

} // namespace riftbound
