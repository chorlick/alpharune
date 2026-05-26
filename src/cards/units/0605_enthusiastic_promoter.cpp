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

class EnthusiasticPromoter : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto bf_id = ctx.state.getObject(ctx.source).battlefieldId();
        if (!bf_id) return;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit()) continue;
            if (obj.battlefieldId() != bf_id) continue;
            // "if it doesn't have one" — skip units that already have a buff.
            if (obj.buff_count > 0) continue;
            ctx.executor.buffUnit(id);
        }
        ctx.events.logTrace("ENTHUSIASTIC PROMOTER: +1M buff to unbuffed units here");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 605;
        d.def_id = R"RB(unl-043-219)RB";
        d.name = R"RB(Enthusiastic Promoter)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-043/219)RB";
        d.collector_number = 43;
        d.artist = R"RB(Valentin Gloaguen)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Yordle)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 3;
        d.might = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Backline);
        d.ability_text = R"RB([Backline] (I must be assigned combat damage last.)
When I hold, [Buff] all units here. (Give each a +1 [M] buff if it doesn't have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c03bdc371440cbf6de773b0b39010808bfdecea1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_605(CardRegistry& r) {
    r.registerCard(605, std::make_unique<EnthusiasticPromoter>());
}

} // namespace riftbound
