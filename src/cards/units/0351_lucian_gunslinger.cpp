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

class LucianGunslinger : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When I attack, deal damage equal to my [Assault] to an enemy unit
    // here." Damage scales with effective Assault (base + temp + aura-granted),
    // not a hardcoded 1.
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        int assault = self.assault_value + self.temp_assault_value;
        for (const auto& ae : self.aura_effects)
            if (ae.keyword == Keyword::Assault)
                assault += (ae.keyword_value > 0 ? ae.keyword_value : 1);
        if (assault <= 0) return;

        auto self_bf = self.battlefieldId();
        if (!self_bf) return;
        std::vector<GameObjectId> enemies;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            auto bf = obj.battlefieldId();
            if (!bf || *bf != *self_bf) continue;
            if (obj.untargetable_by_enemy) continue;
            enemies.push_back(id);
        }
        GameObjectId tgt = pickTarget(ctx,
            "Lucian: deal " + std::to_string(assault) + " to an enemy unit here",
            enemies);
        if (tgt == kInvalidId) return;  // suspend or no targets
        ctx.executor.dealDamage(tgt, assault, ctx.source);
        if (ctx.state.objectExists(tgt) &&
            ctx.state.getObject(tgt).hasLethalDamage()) {
            ctx.executor.killObject(tgt);
        }
        ctx.events.logTrace("LUCIAN GUNSLINGER: dealt " + std::to_string(assault) +
                             " (= Assault) to an enemy unit here");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 351;
        d.def_id = R"RB(sfd-028-221)RB";
        d.name = R"RB(Lucian, Gunslinger)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-028/221)RB";
        d.collector_number = 28;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Lucian)RB", R"RB(Demacia)RB", R"RB(Sentinel)RB"};
        d.energy_cost = 3;
        d.might = 2;
        d.rarity = Rarity::Epic;
        d.assault_value = 1;
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB([Assault] (+1 [M] while I'm an attacker.)
When I attack, deal damage equal to my [Assault] to an enemy unit here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2dfb106d7c85235436d25261195a638e2ab15092-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_351(CardRegistry& r) {
    r.registerCard(351, std::make_unique<LucianGunslinger>());
}

} // namespace riftbound
