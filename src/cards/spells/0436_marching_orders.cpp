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

// "[Repeat] [3] ... Choose a friendly unit anywhere and an enemy unit at a
//  battlefield. They deal damage equal to their Mights to each other."
// ([Repeat] is engine-handled — onResolve re-runs per paid repeat.)

class MarchingOrders : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        bool friendly = false, enemy = false;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            if (obj.controller == controller) friendly = true;
            else if (obj.isAtBattlefield() && !obj.untargetable_by_enemy) enemy = true;
        }
        return friendly && enemy;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> friendly;
        for (auto& [id, obj] : ctx.state.objects)
            if (obj.isUnit() && obj.controller == ctx.controller && obj.location.has_value())
                friendly.push_back(id);
        auto enemy_fn = [&](GameObjectId /*a*/) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller == ctx.controller) continue;
                if (!obj.isAtBattlefield() || obj.untargetable_by_enemy) continue;
                out.push_back(id);
            }
            return out;
        };
        auto [a, b] = pickTargetPair(ctx, "Marching Orders", friendly, enemy_fn);
        bool suspending = (a == kInvalidId || b == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        if (a == kInvalidId || b == kInvalidId ||
            !ctx.state.objectExists(a) || !ctx.state.objectExists(b)) return;
        // Snapshot both Mights BEFORE dealing damage (mutual, simultaneous).
        int might_a = ctx.state.getObject(a).current_might;
        int might_b = ctx.state.getObject(b).current_might;
        ctx.executor.dealDamage(b, might_a, a);
        ctx.executor.dealDamage(a, might_b, b);
        // Collect-then-kill (AoE invariant).
        for (auto id : {a, b})
            if (ctx.state.objectExists(id) && ctx.state.getObject(id).hasLethalDamage())
                ctx.executor.killObject(id);
        ctx.events.logTrace("MARCHING ORDERS: mutual Might damage between friendly + enemy");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 436;
        d.def_id = R"RB(sfd-114-221)RB";
        d.name = R"RB(Marching Orders)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-114/221)RB";
        d.collector_number = 114;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
[Repeat] [3] (You may pay the additional cost to repeat this spell's effect.)
Choose a friendly unit anywhere and an enemy unit at a battlefield. They deal damage equal to their Mights to each other.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b553f8e16e0ff1aaa57f74121e68954fbe45a07b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_436(CardRegistry& r) {
    r.registerCard(436, std::make_unique<MarchingOrders>());
}

} // namespace riftbound
