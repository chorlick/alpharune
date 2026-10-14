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

// [vendetta-E] local helpers
std::vector<GameObjectId> executioners(const GameState& state, PlayerId player) {
    std::vector<GameObjectId> out;
    for (const auto& [id, obj] : state.objects.onBoard())
        if (obj.isUnit() && obj.controller == player && obj.location.has_value())
            out.push_back(id);
    std::sort(out.begin(), out.end());
    return out;
}
// Enemy units with strictly less Might than the friendly unit `chosen`.
std::vector<GameObjectId> weakerEnemies(const GameState& state, PlayerId player,
                                        GameObjectId chosen) {
    if (!state.objectExists(chosen)) return {};
    const int might = state.getObject(chosen).current_might;
    auto out = chooseableUnits(state, player, [&](const GameObject& o) {
        return o.controller != player && o.current_might < might;
    });
    std::sort(out.begin(), out.end());
    return out;
}

class PublicExecution : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // A friendly unit with an enemy unit of less Might to choose (CR 355.16).
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object, [&](const GameObject& u) {
            return !weakerEnemies(state, controller, u.id).empty();
        });
    }
    // [vendetta-E]
    // "Choose a friendly unit. Kill an enemy unit with less Might than it."
    bool needsPlayTimeTargetPair() const override { return true; }
    // Both targets (CR 355.8): a friendly unit with an enemy unit of less
    // Might than it to choose (testing phase G20: whether there is one
    // depends on which friendly unit, so the pairs are tested).
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        for (auto f : executioners(state, controller))
            if (!weakerEnemies(state, controller, f).empty()) return true;
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto [mine, victim] = pickTargetPair(
            ctx, "Public Execution", executioners(ctx.state, ctx.controller),
            [&](GameObjectId a) { return weakerEnemies(ctx.state, ctx.controller, a); });
            if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (mine == kInvalidId || victim == kInvalidId) return;   // suspended or fizzled
        if (!ctx.state.objectExists(victim)) return;
        ctx.executor.killObject(victim);
    }
    // "[Flow] [5][A][A]"
    FlowCost flowCost() const override { return flow(5, 2, Domain::Fury, /*any_domain=*/true); }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 941;
        d.def_id = R"RB(ven-154-166)RB";
        d.name = R"RB(Public Execution)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-154/166)RB";
        d.collector_number = 154;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Body, Domain::Order};
        d.tags = {R"RB(Ambessa)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Flow);
        d.ability_text = R"RB(Choose a friendly unit. Kill an enemy unit with less Might than it.
[Flow] [5][A][A] (You may play this from your trash for its Flow cost. Then banish it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c8348e2aef6bad5a38d4f71b1b1c277598670286-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_941(CardRegistry& r) {
    r.registerCard(941, std::make_unique<PublicExecution>());
}

}  // namespace riftbound
