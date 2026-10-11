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

class RenektonRageFueled : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Accelerate] is engine-handled (keyword).
    // "When I attack, if you control 4 or fewer runes, deal 2 to all enemy units here."
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (runesControlled(ctx.state, ctx.controller) > 4) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return;
        // Collect first, then damage (no mutation while iterating objects).
        std::vector<GameObjectId> enemies;
        for (const auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            auto bf = obj.battlefieldId();
            if (bf && *bf == *my_bf) enemies.push_back(id);
        }
        for (auto id : enemies) ctx.executor.dealDamage(id, 2, ctx.source);
        RIFT_TRACE(ctx.events, "RENEKTON: attack with 4 or fewer runes -> 2 to " +
                            std::to_string(enemies.size()) + " enemy unit(s) here");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 806;
        d.def_id = R"RB(ven-019-166)RB";
        d.name = R"RB(Renekton, Rage Fueled)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-019/166)RB";
        d.collector_number = 19;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Shurima)RB", R"RB(Renekton)RB"};
        d.energy_cost = 6;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Accelerate);
        d.ability_text = R"RB([Accelerate] (You may pay [1][R] as an additional cost to have me enter ready.)
When I attack, if you control 4 or fewer runes, deal 2 to all enemy units here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2f6eb3e8328048ad52617c8639ecbe2c287efa36-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_806(CardRegistry& r) {
    r.registerCard(806, std::make_unique<RenektonRageFueled>());
}

}  // namespace riftbound
