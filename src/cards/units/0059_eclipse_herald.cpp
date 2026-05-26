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

class EclipseHerald : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you stun an enemy unit, ready me and give me +1 [M] this turn."
    TriggerType triggerType() const override { return TriggerType::WhenYouStun; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.readyObject(ctx.source);
        ctx.executor.giveTemporaryMight(ctx.source, 1);
        ctx.events.logTrace("ECLIPSE HERALD: stun -> ready me + +1 [M]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 59;
        d.def_id = R"RB(ogn-059-298)RB";
        d.name = R"RB(Eclipse Herald)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-059/298)RB";
        d.collector_number = 59;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Bird)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 7;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you stun an enemy unit, ready me and give me +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bbe4fec278b8960681f97da658dc2f06ee46c4bd-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_59(CardRegistry& r) {
    r.registerCard(59, std::make_unique<EclipseHerald>());
}

} // namespace riftbound
