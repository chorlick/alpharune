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

class BulletTime : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
    // The battlefield is named by a unit there, chosen as the spell is
    // played; X is asked as it resolves. Nothing is paid or dealt before both
    // are known: the callback is re-entered from the top with each answer.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId target_unit;
        if (!targets.empty()) {
            target_unit = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            target_unit = pickTarget(ctx, "Bullet Time", legal);
        }
        if (target_unit == kInvalidId) return;
        if (!ctx.state.objectExists(target_unit)) return;
        auto& tgt = ctx.state.getObject(target_unit);
        if (!tgt.isAtBattlefield()) return;
        auto bf_id_opt = tgt.battlefieldId();
        if (!bf_id_opt.has_value()) return;
        auto bf_id = *bf_id_opt;

        // "Pay any amount of [A]": up to all the Power the controller can
        // pay — floating Power plus every rune in base, ready or exhausted
        // (CR 164.2.b).
        const int max_x = ctx.executor.payableAnyPower(ctx.controller);
        int x = pickXAmount(ctx, "Bullet Time: X power", 0, max_x);
        if (x < 0) return;  // pending choice
        if (x == 0) {
            ctx.events.logTrace("BULLET TIME: X=0, no damage");
            return;
        }

        if (!ctx.executor.payAnyPower(ctx.controller, x)) {
            ctx.events.logTrace("BULLET TIME: X can no longer be paid, no damage");
            return;
        }
        ctx.events.logTrace("BULLET TIME: paid " + std::to_string(x) +
                             " power → dealing " + std::to_string(x) +
                             " damage to all enemies at BF#" +
                             std::to_string(bf_id));

        // Deal X damage to all enemy units at the chosen battlefield.
        // Snapshot first so kills don't invalidate iteration.
        std::vector<GameObjectId> victims;
        PlayerId enemy = opponent(ctx.controller);
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit()) continue;
            if (obj.controller != enemy) continue;
            if (!obj.isAtBattlefield()) continue;
            auto vbf = obj.battlefieldId();
            if (vbf && *vbf == bf_id) victims.push_back(id);
        }
        for (auto v : victims) ctx.executor.dealDamage(v, x, ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 263;
        d.def_id = R"RB(ogn-268-298)RB";
        d.name = R"RB(Bullet Time)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-268/298)RB";
        d.collector_number = 268;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Body, Domain::Chaos};
        d.tags = {R"RB(Miss Fortune)RB"};
        d.energy_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Pay any amount of [A] to deal that much damage to all enemy units at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/96ed7f6b121a7f46003534393350838efe2776f0-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_263(CardRegistry& r) {
    r.registerCard(263, std::make_unique<BulletTime>());
}

} // namespace riftbound
