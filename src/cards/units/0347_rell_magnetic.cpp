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

class RellMagnetic : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "[Tank]" engine-handled. "When I attack, you may play an Equipment with
    // Energy cost no more than [2], ignoring its cost. If you do, attach it
    // to me."
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto candidates = [&]() {
            std::vector<GameObjectId> out;
            for (auto id : ctx.state.player(ctx.controller).hand) {
                if (!ctx.state.objectExists(id)) continue;
                const auto& obj = ctx.state.getObject(id);
                if (!isEquipment(obj)) continue;
                const auto& def = ctx.executor.cardDB().get(obj.card_def_id);
                if (def.energy_cost > 2) continue;
                out.push_back(id);
            }
            return out;
        };
        int conf = confirmOptional(ctx,
            "Rell: play an Equipment (<= [2]) free and attach to me?",
            [&]() { return !candidates().empty(); });
        if (conf == -1) return;  // waiting for agent
        if (conf == 0) return;   // declined / none available

        GameObjectId eq = pickTarget(ctx, "Rell: choose an Equipment to play",
                                     candidates());
        if (eq == kInvalidId || !ctx.state.objectExists(eq)) return;
        // Play it ignoring cost (to base), then attach it to me.
        ctx.executor.playIgnoringCost(ctx.controller, eq,
                                      LocationId{BaseLocation{ctx.controller}});
        if (ctx.state.objectExists(eq) && ctx.state.objectExists(ctx.source))
            attachFree(ctx, eq, ctx.source);
        ctx.events.logTrace("RELL MAGNETIC: played + attached an Equipment (free)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 347;
        d.def_id = R"RB(sfd-024-221)RB";
        d.name = R"RB(Rell, Magnetic)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-024/221)RB";
        d.collector_number = 24;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Rell)RB", R"RB(Noxus)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Tank] (I must be assigned combat damage first.)
When I attack, you may play an Equipment with Energy cost no more than [2], ignoring its cost. If you do, then do this: Attach it to me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8aa1a9cf0a91a9ab601d5e5d42a438d4217eddd7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_347(CardRegistry& r) {
    r.registerCard(347, std::make_unique<RellMagnetic>());
}

} // namespace riftbound
