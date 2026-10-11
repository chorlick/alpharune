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

class CrescentStrike : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose a battlefield and an enemy unit there. Deal 4 to that unit and 1
    //  to each other enemy unit there." The battlefield is the one the chosen
    //  unit is at, so the unit must be at a battlefield.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        const GameObjectId main = targets[0];
        const auto bf = ctx.state.getObject(main).battlefieldId();
        if (!bf.has_value()) return;
        // Collect first, damage, then kill (AoE: never kill inside the loop).
        std::vector<GameObjectId> hit = {main};
        for (const auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (id == main || !obj.isUnit() || obj.controller == ctx.controller) continue;
            if (obj.battlefieldId() == bf) hit.push_back(id);
        }
        for (auto id : hit) ctx.executor.dealDamage(id, id == main ? 4 : 1, ctx.source);
        {   // the batch is for these kills only
            ctx.executor.killLethallyDamagedTogether(hit);   // they die at the same time
        }
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_enemy = true,
                                  .must_be_at_battlefield = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 634;
        d.def_id = R"RB(unl-072-219)RB";
        d.name = R"RB(Crescent Strike)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-072/219)RB";
        d.collector_number = 72;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Choose a battlefield and an enemy unit there. Deal 4 to that unit and 1 to each other enemy unit there.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/34ab7eff1eb8833ab8b9978eac18c4fb18b1ac81-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_634(CardRegistry& r) {
    r.registerCard(634, std::make_unique<CrescentStrike>());
}

} // namespace riftbound
