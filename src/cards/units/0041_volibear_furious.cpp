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

class VolibearFurious : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "When I attack, deal 5 damage split among any number of enemy
        // units here." Spread 5 round-robin across enemy units at MY
        // battlefield. Collect targets BEFORE killing (iterator-safety).
        if (!ctx.state.objectExists(ctx.source)) return;
        auto my_loc = ctx.state.getObject(ctx.source).location;
        if (!my_loc.has_value() ||
            !std::holds_alternative<BattlefieldLocation>(*my_loc)) return;
        BattlefieldId bf = std::get<BattlefieldLocation>(*my_loc).id;
        PlayerId opp = opponent(ctx.controller);
        std::vector<GameObjectId> enemies;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.isAtBattlefield()) continue;
            if (std::get<BattlefieldLocation>(*obj.location).id != bf) continue;
            enemies.push_back(id);
        }
        std::sort(enemies.begin(), enemies.end());
        if (enemies.empty()) return;
        std::vector<int> dmg(enemies.size(), 0);
        for (int i = 0; i < 5; ++i) dmg[i % enemies.size()] += 1;
        for (size_t i = 0; i < enemies.size(); ++i) {
            if (dmg[i] <= 0 || !ctx.state.objectExists(enemies[i])) continue;
            ctx.executor.dealDamage(enemies[i], dmg[i], ctx.source);
        }
        for (auto id : enemies) {
            if (!ctx.state.objectExists(id)) continue;
            if (ctx.state.getObject(id).hasLethalDamage())
                ctx.executor.killObject(id);
        }
        ctx.events.logTrace("VOLIBEAR: deal 5 split among enemy units here");
    }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 41;
        d.def_id = R"RB(ogn-041-298)RB";
        d.name = R"RB(Volibear, Furious)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-041/298)RB";
        d.collector_number = 41;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Volibear)RB", R"RB(Freljord)RB"};
        d.energy_cost = 10;
        d.power_cost = 2;
        d.might = 9;
        d.rarity = Rarity::Epic;
        d.deflect_value = 2;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect 2] (Opponents must pay [A][A] to choose me with a spell or ability.)
When I attack, deal 5 damage split among any number of enemy units here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c9165d49b8caae9a856433cd5151e8b368eb80b5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_41(CardRegistry& r) {
    r.registerCard(41, std::make_unique<VolibearFurious>());
}

} // namespace riftbound
