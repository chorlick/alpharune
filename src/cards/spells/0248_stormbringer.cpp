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

class Stormbringer : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose a friendly unit in your base. Deal damage equal to its Might to
    //  all enemy units at a battlefield, then move your unit there."
    // The unit and the battlefield (a target: "all enemy units at a
    // battlefield", CR 355.10.b) are chosen as it is played (testing phase D3).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object,
                                  [](const GameObject& u) { return u.isAtBase(); });
    }
    static std::vector<GameObjectId> baseUnits(const GameState& state, PlayerId controller) {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller != controller) continue;
            if (!obj.isAtBase()) continue;
            out.push_back(id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
    // CR 355.8: playable only with a friendly unit in base to choose.
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !baseUnits(state, controller).empty() && !state.battlefields.empty();
    }
    void onResolve(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        const GameObjectId mover = pickTarget(ctx, "Stormbringer: friendly unit in base",
                                              baseUnits(ctx.state, ctx.controller));
        if (mover == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;                               // waiting for the agent
        int dest = -2;
        if (mover != kInvalidId && ctx.state.getObject(mover).isAtBase()) {
            dest = pickMoveDest(*this, ctx, "Stormbringer: battlefield",
                                ctx.state.getObject(mover), /*battlefields_only=*/true);
            if (dest == -1) return;               // waiting for the battlefield
        }
        if (finalizingChoices(ctx)) return;       // choices end here (CR 355)
        if (mover == kInvalidId || dest < 1) return;
        const auto& unit = ctx.state.getObject(mover);
        if (!unit.isAtBase()) return;
        const BattlefieldId bf = BattlefieldId::fromRaw(dest - 1);
        const int dmg = unit.current_might;
        // Deal <dmg> to all enemy units there (collect, damage, then kill).
        std::vector<GameObjectId> hit;
        for (auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            if (obj.battlefieldId() != bf) continue;
            hit.push_back(id);
        }
        for (auto id : hit) ctx.executor.dealDamage(id, dmg, ctx.source);
        {   // the batch is for these kills only
            ctx.executor.killLethallyDamagedTogether(hit);   // they die at the same time
        }
        // Then move your unit there.
        ctx.executor.moveToBattlefield(mover, bf);
        RIFT_TRACE(ctx.events, "STORMBRINGER: dealt " + std::to_string(dmg) +
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
