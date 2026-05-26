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

class JannaSavior : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When you play me, heal your units here, then move up to one enemy unit
    // from here to its base." "here" = the battlefield I was played to.
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto here = ctx.state.getObject(ctx.source).battlefieldId();
        if (!here) return;  // not at a battlefield -> nothing happens

        // Heal ALL friendly units here.
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            auto bf = obj.battlefieldId();
            if (!bf || *bf != *here) continue;
            ctx.executor.healObject(id);
        }

        // Then move up to one enemy unit from here to its base.
        std::vector<GameObjectId> enemies;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            auto bf = obj.battlefieldId();
            if (!bf || *bf != *here) continue;
            if (obj.untargetable_by_enemy) continue;
            enemies.push_back(id);
        }
        GameObjectId enemy = pickTarget(ctx,
            "Janna: move up to one enemy unit from here to base", enemies);
        if (enemy == kInvalidId) return;  // suspend, none, or declined
        if (!ctx.state.objectExists(enemy)) return;
        ctx.executor.moveToBase(enemy);
        ctx.events.logTrace("JANNA SAVIOR: healed friendly units here + moved an "
                            "enemy unit to base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 376;
        d.def_id = R"RB(sfd-053-221)RB";
        d.name = R"RB(Janna, Savior)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-053/221)RB";
        d.collector_number = 53;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Janna)RB", R"RB(Zaun)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve, including to a battlefield you control.)
When you play me, heal your units here, then move up to one enemy unit from here to its base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/191f04059bbfd69dfe63d36010586597bb3549b2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_376(CardRegistry& r) {
    r.registerCard(376, std::make_unique<JannaSavior>());
}

} // namespace riftbound
