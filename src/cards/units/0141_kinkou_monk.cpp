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

class KinkouMonk : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    // "Buff up to two OTHER friendly units." Triggers receive empty targets;
    // pick the (up to) two units at resolution. buffUnit is a +1 [M] persistent
    // buff; "each one that doesn't have a buff gets a +1 buff" — buffUnit only
    // bumps unbuffed units in our model? No: apply buff only if buff_count==0.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> others;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source) continue;                 // "other"
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            others.push_back(id);
        }
        auto second_fn = [&](GameObjectId picked_a) {
            std::vector<GameObjectId> out;
            for (auto id : others) if (id != picked_a) out.push_back(id);
            return out;
        };
        auto [a, b] = pickTargetPair(ctx, "Kinkou Monk (buff up to 2)",
                                     others, second_fn);
        bool suspending = (a == kInvalidId || b == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        auto buff_if_unbuffed = [&](GameObjectId id) {
            if (id == kInvalidId || !ctx.state.objectExists(id)) return;
            if (ctx.state.getObject(id).buff_count > 0) return;  // already has a buff
            ctx.executor.buffUnit(id);
        };
        buff_if_unbuffed(a);
        buff_if_unbuffed(b);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 141;
        d.def_id = R"RB(ogn-141-298)RB";
        d.name = R"RB(Kinkou Monk)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-141/298)RB";
        d.collector_number = 141;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, buff up to two other friendly units. (Each one that doesn't have a buff gets a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/648b22c6f419ee55723247bbf4c3ac10f0be9ab5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_141(CardRegistry& r) {
    r.registerCard(141, std::make_unique<KinkouMonk>());
}

} // namespace riftbound
