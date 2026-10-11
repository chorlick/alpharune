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
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // An equipped friendly unit.
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object,
            [&](const GameObject& u) {
                return !u.attachments.empty();
            });
    }
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
    static std::vector<GameObjectId> equipmentOn(const GameState& state, GameObjectId unit) {
        std::vector<GameObjectId> out;
        if (!state.objectExists(unit)) return out;
        for (auto gid : state.getObject(unit).attachments)
            if (state.objectExists(gid) && isEquipment(state.getObject(gid))) out.push_back(gid);
        return out;
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects.onBoard())
            if (isEquippedFriendly(obj, controller, state)) return true;
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> equipped;
        for (auto& [id, obj] : ctx.state.objects.onBoard())
            if (isEquippedFriendly(obj, ctx.controller, ctx.state)) equipped.push_back(id);
        auto enemy_fn = [&](GameObjectId /*a*/) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects.onBoard()) {
                if (!obj.isUnit() || obj.controller == ctx.controller) continue;
                if (!obj.location.has_value() || obj.untargetable_by_enemy) continue;
                out.push_back(id);
            }
            return out;
        };
        auto [unit, enemy] = pickTargetPair(ctx, "Strike Down", equipped, enemy_fn);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        bool suspending = (unit == kInvalidId || enemy == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        if (unit == kInvalidId || !ctx.state.objectExists(unit)) return;
        // "Then detach an Equipment from it": which one is the controller's
        // choice when it carries more than one. Asked before anything happens
        // (this callback is re-entered with the answer); nothing between here
        // and the damage changes what the unit carries.
        const auto carried = equipmentOn(ctx.state, unit);
        GameObjectId detach = carried.empty() ? kInvalidId : carried.front();
        if (carried.size() > 1) {
            detach = pickTarget(ctx, "Strike Down: detach an Equipment", carried);
            if (detach == kInvalidId) return;   // waiting for the agent
        }
        // It deals damage equal to its Might to the enemy unit.
        if (enemy != kInvalidId && ctx.state.objectExists(enemy)) {
            int dmg = ctx.state.getObject(unit).current_might;
            ctx.executor.dealDamage(enemy, dmg, unit);
            if (ctx.state.objectExists(enemy) &&
                ctx.state.getObject(enemy).hasLethalDamage())
                ctx.executor.killObject(enemy);
        }
        // Then detach an Equipment from it.
        if (detach != kInvalidId && ctx.state.objectExists(detach) &&
            ctx.state.getObject(detach).attached_to == std::optional<GameObjectId>(unit))
            ctx.executor.unattachGear(detach);
        RIFT_TRACE(ctx.events, "STRIKE DOWN: equipped unit dealt Might damage, detached an Equipment");
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
