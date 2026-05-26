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

class SolariChief : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "choose an enemy unit. If it is stunned, kill it. Otherwise, stun it."
        std::vector<GameObjectId> enemies;
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.isUnit() && obj.controller != ctx.controller &&
                obj.location.has_value() && !obj.untargetable_by_enemy)
                enemies.push_back(id);
        }
        if (enemies.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Solari Chief (enemy unit)", enemies);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        if (ctx.state.getObject(picked).is_stunned)
            ctx.executor.killObject(picked);
        else
            ctx.executor.stunUnitBy(picked, ctx.source);
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 225;
        d.def_id = R"RB(ogn-225-298)RB";
        d.name = R"RB(Solari Chief)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-225/298)RB";
        d.collector_number = 225;
        d.artist = R"RB(JiHun Lee)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Mount Targon)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, choose an enemy unit. If it is stunned, kill it. Otherwise, stun it. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/02fd791fdaa7d1c63221655e889fb412de103ca2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_225(CardRegistry& r) {
    r.registerCard(225, std::make_unique<SolariChief>());
}

} // namespace riftbound
