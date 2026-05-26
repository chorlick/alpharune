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

class StormclawUrsine : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.executor.channelRunes(ctx.controller, 1, true);
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 137;
        d.def_id = R"RB(ogn-137-298)RB";
        d.name = R"RB(Stormclaw Ursine)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-137/298)RB";
        d.collector_number = 137;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Freljord)RB"};
        d.energy_cost = 7;
        d.might = 6;
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Tank] (I must be assigned combat damage first.)
When you play me, channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/137e5a1faec1c8b26a5a0abc7802f6461cc725c8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_137(CardRegistry& r) {
    r.registerCard(137, std::make_unique<StormclawUrsine>());
}

} // namespace riftbound
