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

class PitCrew : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAGear; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.readyObject(ctx.source);
        ctx.events.logTrace("PIT CREW: gear played, ready me");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 91;
        d.def_id = R"RB(ogn-091-298)RB";
        d.name = R"RB(Pit Crew)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-091/298)RB";
        d.collector_number = 91;
        d.artist = R"RB(Chris Kintner)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Bandle City)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.ability_text = R"RB(When you play a gear, ready me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8023223b55adc44bafe1f8c5f305d3dde6f6d114-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_91(CardRegistry& r) {
    r.registerCard(91, std::make_unique<PitCrew>());
}

} // namespace riftbound
