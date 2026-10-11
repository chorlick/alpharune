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

class ZedWithoutASound : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // ── behaviour (Vendetta batch C) ──
    // "When I conquer, play a 0 [M] Shadow Clone unit token to your base."
    // (The token carries the registered Shadow Clone card, id 955.)
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        {
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Shadow Clone", 0,
                                 // CR 187.11: 0 Might, no tags ("Shadow Clone" is its name).
                                 std::vector<std::string>{}, KeywordSet{},
                                 BaseLocation{ctx.controller}, /*enter_ready=*/false, {},
                                 // the Shadow Clone token card ("When I attack")
                                 /*token_card=*/955);
        }
        RIFT_TRACE(ctx.events, "ZED, WITHOUT A SOUND: conquer -> Shadow Clone token to base");
    }
    // "[Action][>] [1][P]: Move me and a Shadow Clone you control to each
    //  other's locations."
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility ab;
        ab.cost.energy = 1;
        ab.cost.power = 1;
        ab.cost.power_domain = Domain::Chaos;
        ab.targets = TargetRequirements{.count = 1, .must_be_unit = true,
                                        .must_be_friendly = true};
        ab.is_action = true;
        return {ab};
    }
    static bool isClone(const GameObject& obj, PlayerId controller) {
        return obj.isUnit() && obj.controller == controller && obj.location.has_value() &&
               obj.name == "Shadow Clone";
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller, int /*ability_index*/) const override {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects.onBoard())
            if (isClone(obj, controller)) out.push_back(id);
        return out;
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        if (!ctx.state.objectExists(ctx.source)) return;
        const GameObjectId clone = targets[0];
        if (clone == ctx.source || !isClone(ctx.state.getObject(clone), ctx.controller)) return;
        const auto my_loc = ctx.state.getObject(ctx.source).location;
        const auto clone_loc = ctx.state.getObject(clone).location;
        if (!my_loc || !clone_loc || *my_loc == *clone_loc) return;   // nothing to swap
        moveToLocation(ctx.executor, ctx.source, clone_loc);
        moveToLocation(ctx.executor, clone, my_loc);
        RIFT_TRACE(ctx.events, "ZED, WITHOUT A SOUND: swapped places with a Shadow Clone");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 899;
        d.def_id = R"RB(ven-112-166)RB";
        d.name = R"RB(Zed, Without a Sound)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-112/166)RB";
        d.collector_number = 112;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Ionia)RB", R"RB(Zed)RB"};
        d.energy_cost = 5;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I conquer, play a 0 [M] Shadow Clone unit token to your base. (It has "When I attack, you may banish a unit from your trash. If you do, give me [Assault 4] this turn.")
[Action][>] [1][P]: Move me and a Shadow Clone you control to each other's locations.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/be97ca6a33e25b85a04ec1b3a163b31f141776cf-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_899(CardRegistry& r) {
    r.registerCard(899, std::make_unique<ZedWithoutASound>());
}

}  // namespace riftbound
