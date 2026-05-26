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

class CaitlynPatrolling : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "I must be assigned combat damage last." = Backline (CR 460.2.c).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller || !obj.location.has_value()) continue;
            GameObject::AuraEffect ae;
            ae.source = id;
            ae.keyword = Keyword::Backline;
            obj.aura_effects.push_back(ae);
        }
    }

    // "[E]: Deal damage equal to my Might to a unit at a battlefield. Use this
    //  ability only while I'm at a battlefield."
    bool canActivateAbility(const GameState& state,
                            PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller) continue;
            if (obj.isAtBattlefield()) return true;
        }
        return false;
    }
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
        if (!ctx.state.objectExists(ctx.source)) return;
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) picked = targets[0];
        else picked = pickTarget(ctx, "Caitlyn: deal Might damage to a unit",
                                 enumerateLegalTargets(ctx.state, ctx.controller, 0));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        int dmg = ctx.state.getObject(ctx.source).current_might;
        ctx.executor.dealDamage(picked, dmg, ctx.source);
        if (ctx.state.objectExists(picked) &&
            ctx.state.getObject(picked).hasLethalDamage())
            ctx.executor.killObject(picked);
        ctx.events.logTrace("CAITLYN: deal " + std::to_string(dmg) +
                            " (=Might) to a unit at a battlefield");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 68;
        d.def_id = R"RB(ogn-068-298)RB";
        d.name = R"RB(Caitlyn, Patrolling)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-068/298)RB";
        d.collector_number = 68;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Caitlyn)RB", R"RB(Piltover)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(I must be assigned combat damage last.
[E]: Deal damage equal to my Might to a unit at a battlefield. Use this ability only while I'm at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/51e6bacf25d19e7c391367ff107efb9e0b9f1ff5-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_68(CardRegistry& r) {
    r.registerCard(68, std::make_unique<CaitlynPatrolling>());
}

} // namespace riftbound
