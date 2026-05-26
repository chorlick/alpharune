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

class ClashOfGiants : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose two units. They deal damage equal to their Mights to each other."
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        int units = 0;
        for (auto& [id, obj] : state.objects) {
            if (!obj.location.has_value() || !obj.isUnit()) continue;
            if (obj.controller != controller && obj.untargetable_by_enemy) continue;
            if (++units >= 2) return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = [&](GameObjectId exclude) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == exclude) continue;
                if (!obj.location.has_value() || !obj.isUnit()) continue;
                if (obj.controller != ctx.controller && obj.untargetable_by_enemy) continue;
                out.push_back(id);
            }
            return out;
        };
        auto [a, b] = pickTargetPair(ctx, "Clash of Giants",
                                     legal(kInvalidId),
                                     [&](GameObjectId picked_a) { return legal(picked_a); });
        bool suspending = (a == kInvalidId || b == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        if (a == kInvalidId || b == kInvalidId) return;
        if (!ctx.state.objectExists(a) || !ctx.state.objectExists(b)) return;

        // Snapshot Mights before dealing damage so the exchange is simultaneous.
        int might_a = ctx.state.getObject(a).current_might;
        int might_b = ctx.state.getObject(b).current_might;
        ctx.events.logTrace("CLASH OF GIANTS: " + ctx.state.getObject(a).name +
                             " (" + std::to_string(might_a) + "M) <-> " +
                             ctx.state.getObject(b).name +
                             " (" + std::to_string(might_b) + "M)");
        ctx.executor.dealDamage(b, might_a, ctx.source);
        ctx.executor.dealDamage(a, might_b, ctx.source);
        for (auto tid : {a, b}) {
            if (ctx.state.objectExists(tid) &&
                ctx.state.getObject(tid).hasLethalDamage()) {
                ctx.executor.killObject(tid);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 672;
        d.def_id = R"RB(unl-110-219)RB";
        d.name = R"RB(Clash of Giants)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-110/219)RB";
        d.collector_number = 110;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 6;
        d.power_cost = 2;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Choose two units. They deal damage equal to their Mights to each other.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/afe7cbde874003db673925275a13614d5f35266b-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_672(CardRegistry& r) {
    r.registerCard(672, std::make_unique<ClashOfGiants>());
}

} // namespace riftbound
