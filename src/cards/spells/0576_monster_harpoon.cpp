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

class MonsterHarpoon : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // "Deal 2 to a unit at a battlefield. If you control a facedown card,
        // deal 4 to it instead."
        if (!targets.empty()) {
            bool has_facedown = false;
            for (auto& [id, obj] : ctx.state.objects) {
                if (obj.controller == ctx.controller && obj.is_hidden) {
                    has_facedown = true;
                    break;
                }
            }
            int dmg = has_facedown ? 4 : 2;
            ctx.executor.dealDamage(targets[0], dmg, ctx.source);
            if (ctx.state.objectExists(targets[0]) &&
                ctx.state.getObject(targets[0]).hasLethalDamage()) {
                ctx.executor.killObject(targets[0]);
            }
        }
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_at_battlefield = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 576;
        d.def_id = R"RB(unl-014-219)RB";
        d.name = R"RB(Monster Harpoon)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-014/219)RB";
        d.collector_number = 14;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Deal 2 to a unit at a battlefield. If you control a facedown card, deal 4 to it instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ed51ed36ad0938048ee50894d75d299c29d17759-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_576(CardRegistry& r) {
    r.registerCard(576, std::make_unique<MonsterHarpoon>());
}

} // namespace riftbound
