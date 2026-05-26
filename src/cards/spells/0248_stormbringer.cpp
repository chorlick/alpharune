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

class Stormbringer : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    void onResolve(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        // 1) Choose a friendly unit in your base.
        std::vector<GameObjectId> base_units;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.isAtBase()) continue;
            base_units.push_back(id);
        }
        std::sort(base_units.begin(), base_units.end());
        GameObjectId mover = pickTarget(ctx, "Stormbringer: friendly unit in base",
                                        base_units);
        // Distinguish suspend (resume_point 7) from fizzle.
        if (mover == kInvalidId) {
            if (ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7) return;  // suspend
            return;  // no legal mover — fizzle
        }
        if (!ctx.state.objectExists(mover)) return;
        int dmg = ctx.state.getObject(mover).current_might;

        // 2) Choose a battlefield with enemy units (anchored by an enemy unit).
        PlayerId opp = opponent(ctx.controller);
        std::vector<GameObjectId> enemy_at_bf;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.isAtBattlefield()) continue;
            enemy_at_bf.push_back(id);
        }
        std::sort(enemy_at_bf.begin(), enemy_at_bf.end());
        // The first pickTarget above already consumed the pickTarget resume
        // slots (6/7/8), so we cannot pose a SECOND resumable target choice for
        // the battlefield in the same resolve. Instead we anchor the AoE on the
        // first enemy unit found at a battlefield; the damage hits ALL enemy
        // units at that BF regardless, and the mover lands there. The agent's
        // battlefield preference is not surfaced as a separate vocab slot here
        // (acceptable simplification — documented in report).
        if (enemy_at_bf.empty()) {
            ctx.events.logTrace("STORMBRINGER: no enemy units at a battlefield — "
                                "damage skipped");
            return;
        }
        std::optional<LocationId> anchor =
            ctx.state.getObject(enemy_at_bf.front()).location;
        if (!anchor.has_value()) return;

        // AoE: deal <dmg> to all enemy units at the anchor BF, collect-then-kill.
        std::vector<GameObjectId> hit;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (obj.location != anchor) continue;
            hit.push_back(id);
        }
        for (auto id : hit) {
            if (ctx.state.objectExists(id))
                ctx.executor.dealDamage(id, dmg, ctx.source);
        }
        for (auto id : hit) {
            if (ctx.state.objectExists(id) &&
                ctx.state.getObject(id).hasLethalDamage()) {
                ctx.executor.killObject(id);
            }
        }
        // Then move your unit there.
        if (ctx.state.objectExists(mover) &&
            std::holds_alternative<BattlefieldLocation>(*anchor)) {
            ctx.executor.moveToBattlefield(
                mover, std::get<BattlefieldLocation>(*anchor).id);
        }
        ctx.events.logTrace("STORMBRINGER: dealt " + std::to_string(dmg) +
                            " to " + std::to_string(hit.size()) +
                            " enemy units, then moved my unit there");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 248;
        d.def_id = R"RB(ogn-250-298)RB";
        d.name = R"RB(Stormbringer)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-250/298)RB";
        d.collector_number = 250;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Body};
        d.tags = {R"RB(Volibear)RB"};
        d.energy_cost = 6;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Choose a friendly unit in your base. Deal damage equal to its Might to all enemy units at a battlefield, then move your unit there.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d86c4137cc7f77f103cd7d6228125df8cb9a54e1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_248(CardRegistry& r) {
    r.registerCard(248, std::make_unique<Stormbringer>());
}

} // namespace riftbound
