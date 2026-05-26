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

class ThousandTailedWatcher : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        PlayerId opp = opponent(ctx.controller);
        std::vector<GameObjectId> enemies;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.location.has_value()) continue;
            enemies.push_back(id);
        }
        for (auto id : enemies) {
            // -3 might this turn, minimum 1.
            ctx.executor.giveTemporaryMight(id, -3, /*minimum=*/1);
        }
        ctx.events.logTrace("THOUSAND-TAILED WATCHER: enemy units -3M (min 1) this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 116;
        d.def_id = R"RB(ogn-116-298)RB";
        d.name = R"RB(Thousand-Tailed Watcher)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-116/298)RB";
        d.collector_number = 116;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 7;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Accelerate);
        d.ability_text = R"RB([Accelerate] (You may pay [1][B] as an additional cost to have me enter ready.)
When you play me, give enemy units -3 [M] this turn, to a minimum of 1 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b20e5644c33924a58e0497dd9f7db19723147003-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_116(CardRegistry& r) {
    r.registerCard(116, std::make_unique<ThousandTailedWatcher>());
}

} // namespace riftbound
