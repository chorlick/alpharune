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

class HextechFormula : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "This enters exhausted."
    bool entersExhausted() const override { return true; }
    // "[E]: Empower another gear. (It becomes Empowered if it's not already.)"
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        return c;
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_gear = true};
    }
    // "Another gear": target enumeration is not told which object is
    // activating, so a controller's own Hextech Formulas are only offered when
    // they control two or more (one can then choose the other); onActivate
    // rejects the activating copy itself.
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        int own_formulas = 0;
        for (const auto& [id, obj] : state.objects.onBoard())
            if (obj.isGear() && obj.location.has_value() && obj.controller == controller &&
                obj.card_def_id == cardDefId()) ++own_formulas;
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects.onBoard()) {
            if (!obj.isGear() || !obj.location.has_value()) continue;
            if (obj.controller != controller && obj.untargetable_by_enemy) continue;
            if (obj.controller == controller && obj.card_def_id == cardDefId() &&
                own_formulas < 2) continue;
            out.push_back(id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || targets[0] == ctx.source) return;
        if (!ctx.state.objectExists(targets[0]) || !ctx.state.getObject(targets[0]).isGear())
            return;
        ctx.executor.empower(targets[0]);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 849;
        d.def_id = R"RB(ven-062-166)RB";
        d.name = R"RB(Hextech Formula)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-062/166)RB";
        d.collector_number = 62;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 2;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(This enters exhausted.
[E]: Empower another gear. (It becomes Empowered if it's not already.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/95f4ee0625dda4213847e6649465397be03c8bd1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_849(CardRegistry& r) {
    r.registerCard(849, std::make_unique<HextechFormula>());
}

}  // namespace riftbound
