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

class JinxRebel : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you discard one or more cards, ready me and give me +1 [M] this turn."
    TriggerType triggerType() const override { return TriggerType::WhenYouDiscard; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.readyObject(ctx.source);
        ctx.executor.giveTemporaryMight(ctx.source, 1);
        ctx.events.logTrace("JINX REBEL: discard -> ready + +1[M]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 202;
        d.def_id = R"RB(ogn-202-298)RB";
        d.name = R"RB(Jinx, Rebel)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-202/298)RB";
        d.collector_number = 202;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Jinx)RB", R"RB(Zaun)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When you discard one or more cards, ready me and give me +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a7fe105f40df66525be51bd18e25506945a7b027-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_202(CardRegistry& r) {
    r.registerCard(202, std::make_unique<JinxRebel>());
}

} // namespace riftbound
