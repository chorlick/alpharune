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

class DancingGrenade : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Deal 2 to a unit. Its controller may play this spell again for [A]. If
    //  they do, this deals 1 additional Bonus Damage for each time this spell
    //  has dealt damage this turn."
    // The recast-for-[A] loop has no engine "may play this again + charge [A]"
    // replay hook; it is not wired. The escalating Bonus Damage IS modeled:
    // each time this specific spell deals damage in a turn, the count is
    // tracked on the source object so a re-play (by any means) escalates.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty()) return;
        int prior_hits = 0;
        if (ctx.state.objectExists(ctx.source)) {
            auto& src = ctx.state.getObject(ctx.source);
            prior_hits = src.card_counters["__dancing_grenade_hits"];
            src.card_counters["__dancing_grenade_hits"] = prior_hits + 1;
        }
        int dmg = 2 + prior_hits;  // +1 Bonus Damage per prior hit this turn
        ctx.executor.dealDamage(targets[0], dmg, ctx.source);
        ctx.events.logTrace("DANCING GRENADE: deal " + std::to_string(dmg) +
                            " (recast-for-[A] loop not engine-wired)");
        if (ctx.state.objectExists(targets[0]) &&
            ctx.state.getObject(targets[0]).hasLethalDamage()) {
            ctx.executor.killObject(targets[0]);
        }
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 582;
        d.def_id = R"RB(unl-020-219)RB";
        d.name = R"RB(Dancing Grenade)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-020/219)RB";
        d.collector_number = 20;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Deal 2 to a unit. Its controller may play this spell again for [A]. If they do, this deals 1 additional Bonus Damage for each time this spell has dealt damage this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8afc30b1bc7cc1841dfe11cfa29adbcc02257845-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_582(CardRegistry& r) {
    r.registerCard(582, std::make_unique<DancingGrenade>());
}

} // namespace riftbound
