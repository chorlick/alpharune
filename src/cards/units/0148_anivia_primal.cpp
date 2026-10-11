#include "cards/card.h"
#include "cards/card_helpers.h"
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

class AniviaPrimal : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "Deal 3 to all enemy units here": the battlefield I'm attacking,
        // not every battlefield. Damage everything first, then kill.
        const auto here = ctx.state.getObject(ctx.source).battlefieldId();
        if (!here) return;   // no longer at a battlefield
        std::vector<GameObjectId> to_damage;
        for (auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            if (obj.battlefieldId() != here) continue;
            to_damage.push_back(id);
        }
        std::sort(to_damage.begin(), to_damage.end());
        for (auto id : to_damage)
            ctx.executor.dealDamage(id, 3, ctx.source);
        {   // the batch is for these kills only
            DeathBatch together(ctx.executor, to_damage, DeathBatch::LethalOnly);   // they die at the same time
            for (auto id : to_damage) {
                const auto& o = ctx.state.getObject(id);
                if (o.location.has_value() && o.hasLethalDamage())
                    ctx.executor.killObject(id);
            }
        }
    }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 148;
        d.def_id = R"RB(ogn-148-298)RB";
        d.name = R"RB(Anivia, Primal)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-148/298)RB";
        d.collector_number = 148;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Bird)RB", R"RB(Anivia)RB", R"RB(Freljord)RB"};
        d.energy_cost = 7;
        d.power_cost = 2;
        d.might = 8;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I attack, deal 3 to all enemy units here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ca1a56035333e31b3a04d67ee9131bb4d533e5db-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_148(CardRegistry& r) {
    r.registerCard(148, std::make_unique<AniviaPrimal>());
}

} // namespace riftbound
