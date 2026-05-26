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

// "Choose an equipped friendly unit. It deals damage equal to its Might to an
//  enemy unit. Then detach an Equipment from it."

class StrikeDown : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }

    static bool isEquippedFriendly(const GameObject& u, PlayerId controller,
                                   const GameState& state) {
        if (!u.isUnit() || u.controller != controller || !u.location.has_value())
            return false;
        for (auto gid : u.attachments)
            if (state.objectExists(gid) && isEquipment(state.getObject(gid)))
                return true;
        return false;
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects)
            if (isEquippedFriendly(obj, controller, state)) return true;
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> equipped;
        for (auto& [id, obj] : ctx.state.objects)
            if (isEquippedFriendly(obj, ctx.controller, ctx.state)) equipped.push_back(id);
        auto enemy_fn = [&](GameObjectId /*a*/) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller == ctx.controller) continue;
                if (!obj.location.has_value() || obj.untargetable_by_enemy) continue;
                out.push_back(id);
            }
            return out;
        };
        auto [unit, enemy] = pickTargetPair(ctx, "Strike Down", equipped, enemy_fn);
        bool suspending = (unit == kInvalidId || enemy == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        if (unit == kInvalidId || !ctx.state.objectExists(unit)) return;
        // It deals damage equal to its Might to the enemy unit.
        if (enemy != kInvalidId && ctx.state.objectExists(enemy)) {
            int dmg = ctx.state.getObject(unit).current_might;
            ctx.executor.dealDamage(enemy, dmg, unit);
            if (ctx.state.objectExists(enemy) &&
                ctx.state.getObject(enemy).hasLethalDamage())
                ctx.executor.killObject(enemy);
        }
        // Then detach an Equipment from it.
        if (ctx.state.objectExists(unit)) {
            for (auto gid : ctx.state.getObject(unit).attachments) {
                if (ctx.state.objectExists(gid) && isEquipment(ctx.state.getObject(gid))) {
                    ctx.executor.unattachGear(gid);
                    break;
                }
            }
        }
        ctx.events.logTrace("STRIKE DOWN: equipped unit dealt Might damage, detached an Equipment");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 429;
        d.def_id = R"RB(sfd-107-221)RB";
        d.name = R"RB(Strike Down)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-107/221)RB";
        d.collector_number = 107;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Choose an equipped friendly unit. It deals damage equal to its Might to an enemy unit. Then detach an Equipment from it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3570810e867d132fd5a60cc3462f53f42b245ad9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_429(CardRegistry& r) {
    r.registerCard(429, std::make_unique<StrikeDown>());
}

} // namespace riftbound
