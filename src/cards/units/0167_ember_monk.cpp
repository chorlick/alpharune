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

class EmberMonk : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play a card from [Hidden], give me +2 [M] this turn."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayFromFacedown; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.giveTemporaryMight(ctx.source, 2);
        ctx.events.logTrace("EMBER MONK: played from Hidden -> +2 M this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 167;
        d.def_id = R"RB(ogn-167-298)RB";
        d.name = R"RB(Ember Monk)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-167/298)RB";
        d.collector_number = 167;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Bandle City)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB(When you play a card from [Hidden], give me +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0441f70a7acc377d8cbe08559b7a2fc3139c903b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_167(CardRegistry& r) {
    r.registerCard(167, std::make_unique<EmberMonk>());
}

} // namespace riftbound
