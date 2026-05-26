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

class IronBallista : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "This enters exhausted."
    void onPlay(CardContext& ctx) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.exhaustObject(ctx.source);
    }

    // "[E]: Deal 2 to a unit at a battlefield."
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true},
            .targets = TargetRequirements{.count = 1, .must_be_unit = true,
                                           .must_be_at_battlefield = true},
            .is_action = false, .is_reaction = false,
            .needs_activation_time_target = true,
        }};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId /*controller*/,
        int /*ability_index*/) const override {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || !obj.isAtBattlefield()) continue;
            out.push_back(id);
        }
        return out;
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) picked = targets[0];
        else picked = pickTarget(ctx, "Iron Ballista: deal 2 to a unit",
                                 enumerateLegalTargets(ctx.state, ctx.controller, 0));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.dealDamage(picked, 2, ctx.source);
        if (ctx.state.objectExists(picked) &&
            ctx.state.getObject(picked).hasLethalDamage())
            ctx.executor.killObject(picked);
        ctx.events.logTrace("IRON BALLISTA: deal 2 to a unit at a battlefield");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 17;
        d.def_id = R"RB(ogn-017-298)RB";
        d.name = R"RB(Iron Ballista)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-017/298)RB";
        d.collector_number = 17;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Fury};
        d.energy_cost = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(This enters exhausted.
[E]: Deal 2 to a unit at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9b5cfe757b6067994e17e284eba46433880a1461-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_17(CardRegistry& r) {
    r.registerCard(17, std::make_unique<IronBallista>());
}

} // namespace riftbound
