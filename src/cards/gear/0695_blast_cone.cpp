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

class BlastCone : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true, .optional = true};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto find_enemy = [&]() -> GameObjectId {
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller == ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                if (obj.untargetable_by_enemy) continue;
                return id;
            }
            return kInvalidId;
        };
        auto move_legal = [&]() {
            if (!targets.empty())
                return ctx.state.objectExists(targets[0]) &&
                       ctx.state.getObject(targets[0]).location.has_value();
            return find_enemy() != kInvalidId;
        };
        if (!move_legal()) return;
        int conf = confirmOptional(ctx, "Blast Cone: move an enemy unit?", move_legal);
        if (conf != 1) return;

        GameObjectId moved = kInvalidId;
        if (!targets.empty() && ctx.state.objectExists(targets[0]) &&
            ctx.state.getObject(targets[0]).controller != ctx.controller) {
            moved = targets[0];
        } else {
            moved = find_enemy();
        }
        if (moved == kInvalidId) return;
        ctx.executor.moveToBase(moved);
        ctx.events.logTrace("BLAST CONE: moved enemy on play");

        // Second clause: having moved an enemy unit, you may exhaust this to
        // stun it.
        auto stun_legal = [&]() {
            return ctx.state.objectExists(ctx.source) &&
                   !ctx.state.getObject(ctx.source).is_exhausted &&
                   ctx.state.objectExists(moved) &&
                   !ctx.state.getObject(moved).is_stunned;
        };
        if (!stun_legal()) return;
        int conf2 = confirmOptional(ctx,
            "Blast Cone: exhaust me to stun the moved enemy?", stun_legal);
        if (conf2 != 1) return;
        ctx.executor.exhaustObject(ctx.source);
        ctx.executor.stunUnitBy(moved, ctx.source);
        ctx.events.logTrace("BLAST CONE: exhausted to stun moved enemy");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 695;
        d.def_id = R"RB(unl-133-219)RB";
        d.name = R"RB(Blast Cone)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-133/219)RB";
        d.collector_number = 133;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play this, you may move an enemy unit.
When you move an enemy unit, you may exhaust this to [Stun] it. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/093799f1ad34084f05011f3b326d4d32d96849e5-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_695(CardRegistry& r) {
    r.registerCard(695, std::make_unique<BlastCone>());
}

} // namespace riftbound
