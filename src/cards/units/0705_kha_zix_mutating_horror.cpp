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

class KhaZixMutatingHorror : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttackOrDefend; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        auto bf_id = self.battlefieldId();
        if (!bf_id) return;
        // Count enemy units at this BF — "alone" means exactly 1.
        auto opp = opponent(ctx.controller);
        int enemy_count = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (obj.battlefieldId() == bf_id) ++enemy_count;
        }
        if (enemy_count != 1) return;
        ctx.executor.giveTemporaryMight(ctx.source, 2);
        ctx.state.player(ctx.controller).xp += 2;
        ctx.events.logTrace("KHA'ZIX: enemy alone, +2M and +2 XP");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 705;
        d.def_id = R"RB(unl-143-219)RB";
        d.name = R"RB(Kha'Zix, Mutating Horror)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-143/219)RB";
        d.collector_number = 143;
        d.artist = R"RB(蛋费鸡丁)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Kha'Zix)RB", R"RB(The Void)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Ambush);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Ambush] (You may play me as a [Reaction] to a battlefield where you have units.)
When I attack or defend, if an enemy unit is alone here, give me +2 [M] this turn and gain 2 XP.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8306abc5ffce45add8c75c2e215162b2d1aed320-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_705(CardRegistry& r) {
    r.registerCard(705, std::make_unique<KhaZixMutatingHorror>());
}

} // namespace riftbound
