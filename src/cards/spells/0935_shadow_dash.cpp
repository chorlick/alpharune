#include "cards/card.h"
#include "cards/card_helpers.h"
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

// Battlefields where `player` has at least one unit.
std::vector<BattlefieldId> battlefieldsWithUnitsOf(const GameState& state, PlayerId player) {
    std::vector<BattlefieldId> out;
    for (const auto& bf : state.battlefields) {
        for (const auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != player) continue;
            auto at = obj.battlefieldId();
            if (at.has_value() && *at == bf.id) { out.push_back(bf.id); break; }
        }
    }
    return out;
}
// Battlefields the enemy unit `u` could be moved to by Shadow Dash.
std::vector<BattlefieldId> dashDests(const GameState& state, PlayerId player,
                                     const GameObject& u) {
    std::vector<BattlefieldId> out;
    for (auto bf : battlefieldsWithUnitsOf(state, player)) {
        auto at = u.battlefieldId();
        if (at.has_value() && *at == bf) continue;
        out.push_back(bf);
    }
    return out;
}
std::vector<GameObjectId> dashTargets(const GameState& state, PlayerId player) {
    auto out = chooseableUnits(state, player, [&](const GameObject& o) {
        return o.controller != player && !dashDests(state, player, o).empty();
    });
    std::sort(out.begin(), out.end());
    return out;
}

class ShadowDash : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Move an enemy unit to a battlefield where you have units. If you have
    //  exactly two units there, they each get +1 [M] this turn."
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_enemy = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        return dashTargets(state, controller);
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // The enemy unit was chosen as the spell was played
        // (needsPlayTimeTarget): pickTarget returns that choice — Deflect
        // paid, "chosen" bookkeeping done — or nothing if it is no longer a
        // legal target.
        const GameObjectId u = pickTarget(ctx, "Shadow Dash: move an enemy unit",
                                          dashTargets(ctx.state, ctx.controller));
        if (u == kInvalidId) return;   // no target / mistarget / waiting
        // The destination is decided as the spell resolves: a battlefield
        // where I have units now. Coded like moveDests (1 + i = battlefield
        // i; 0, the base, is never a destination here).
        const auto dests = dashDests(ctx.state, ctx.controller, ctx.state.getObject(u));
        if (dests.empty()) return;
        std::vector<std::string> labels{destLabel(0)};
        uint32_t legal = 0;
        for (size_t i = 0; i < ctx.state.battlefields.size() && i < 31; ++i) {
            labels.push_back(destLabel(1 + static_cast<int>(i)));
            if (std::find(dests.begin(), dests.end(), ctx.state.battlefields[i].id) != dests.end())
                legal |= 1u << (i + 1);
        }
        const int m = pickMode(ctx, "Shadow Dash: to which battlefield",
                               static_cast<int>(labels.size()), labels, legal);
        if (m < 1) return;             // waiting for the agent
        const BattlefieldId bf = ctx.state.battlefields[static_cast<size_t>(m - 1)].id;
        if (std::find(dests.begin(), dests.end(), bf) == dests.end()) return;
        ctx.executor.moveToBattlefield(u, bf);
        // "If you have exactly two units there, they each get +1 [M] this turn."
        std::vector<GameObjectId> mine;
        for (const auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            auto at = obj.battlefieldId();
            if (at.has_value() && *at == bf) mine.push_back(id);
        }
        if (mine.size() == 2)
            for (auto id : mine) ctx.executor.giveTemporaryMight(id, 1);
    }
    // "[Flow] [5][A][A]"
    FlowCost flowCost() const override { return flow(5, 2, Domain::Fury, /*any_domain=*/true); }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 935;
        d.def_id = R"RB(ven-148-166)RB";
        d.name = R"RB(Shadow Dash)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-148/166)RB";
        d.collector_number = 148;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Order};
        d.tags = {R"RB(Shen)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Flow);
        d.ability_text = R"RB(Move an enemy unit to a battlefield where you have units. If you have exactly two units there, they each get +1 [M] this turn.
[Flow] [5][A][A] (You may play this from your trash for its Flow cost. Then banish it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c95a4862c3af5f3ec1cee1f1ae7291c8724df7a1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_935(CardRegistry& r) {
    r.registerCard(935, std::make_unique<ShadowDash>());
}

}  // namespace riftbound
