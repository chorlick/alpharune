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

class RogueAssassin : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Empower] [3][A]" + "[Action][>] [E]: If it's your turn, move a friendly
    // unit in a showdown to base and if I'm [Empowered], ready it."
    EmpowerSpec empowerSpec() const override {
        return empowerCost(3, 1, Domain::Fury, /*any_domain=*/true);
    }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {
            {.cost = {.exhaust = true},
             .targets = TargetRequirements{.count = 1, .must_be_unit = true,
                                           .must_be_friendly = true,
                                           .must_be_at_battlefield = true},
             .is_action = true,
             .needs_activation_time_target = true},
            empowerAbility(),
        };
    }
    // A unit "in a showdown" is one at a battlefield where a showdown/combat is live.
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller, int ability_index) const override {
        if (ability_index != 0) return {};
        if (state.turn.turn_player != controller) return {};   // "If it's your turn"
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller != controller) continue;
            auto bf = obj.battlefieldId();
            if (!bf) continue;
            for (const auto& b : state.battlefields)
                if (b.id == *bf && (b.combat_in_progress || b.showdown_in_progress))
                    out.push_back(id);
        }
        return out;
    }
    void onActivate(CardContext& ctx, int ability_index,
                    const std::vector<GameObjectId>& targets) override {
        if (ability_index != 0) return;
        GameObjectId picked = !targets.empty() ? targets[0]
            : pickTarget(ctx, "Rogue Assassin",
                         enumerateLegalTargets(ctx.state, ctx.controller, 0));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.moveToBase(picked);
        if (ctx.state.objectExists(ctx.source) &&
            ctx.state.getObject(ctx.source).isEmpowered())
            ctx.executor.readyObject(picked);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 926;
        d.def_id = R"RB(ven-139-166)RB";
        d.name = R"RB(Rogue Assassin)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-139/166)RB";
        d.collector_number = 139;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Fury, Domain::Calm};
        d.tags = {R"RB(Akali)RB"};
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Empower] [3][A] ([3][A]: Empower this. Use only if not Empowered.)
[Action][>] [E]: If it's your turn, move a friendly unit in a showdown to base and if I'm [Empowered], ready it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0d53b477ed43fb9bbed84858443a606b2b51a2b5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_926(CardRegistry& r) {
    r.registerCard(926, std::make_unique<RogueAssassin>());
}

}  // namespace riftbound
