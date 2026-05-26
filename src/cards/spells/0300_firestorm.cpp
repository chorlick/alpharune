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

class Firestorm : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Deal 3 to all enemy units at A battlefield." (one chosen BF)
    // Choose an enemy unit at a battlefield to identify the target BF, then
    // hit ALL enemy units there.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_enemy = true, .must_be_at_battlefield = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId anchor;
        if (!targets.empty()) {
            anchor = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            anchor = pickTarget(ctx, "Firestorm (enemy unit at a battlefield)", legal);
        }
        if (anchor == kInvalidId || !ctx.state.objectExists(anchor)) return;
        auto bf = ctx.state.getObject(anchor).battlefieldId();
        if (!bf) return;

        // Collect all enemy units at the chosen battlefield, then deal-then-kill.
        std::vector<GameObjectId> to_damage;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            if (obj.battlefieldId() != bf) continue;
            to_damage.push_back(id);
        }
        for (auto id : to_damage)
            ctx.executor.dealDamage(id, 3, ctx.source);
        for (auto id : to_damage) {
            if (ctx.state.objectExists(id) && ctx.state.getObject(id).hasLethalDamage())
                ctx.executor.killObject(id);
        }
        ctx.events.logTrace("FIRESTORM: 3 to all enemy units at one battlefield");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 300;
        d.def_id = R"RB(ogs-002-024)RB";
        d.name = R"RB(Firestorm)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-002/024)RB";
        d.collector_number = 2;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Deal 3 to all enemy units at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a7a34129e64f0296bf2da166c2b06ed156d568db-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_300(CardRegistry& r) {
    r.registerCard(300, std::make_unique<Firestorm>());
}

} // namespace riftbound
