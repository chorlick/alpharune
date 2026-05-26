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

class ConvergentMutation : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        GameObjectId chosen = targets[0];
        // "increase its Might to the Might of another friendly unit."
        // Pick another friendly unit (not the chosen one).
        std::vector<GameObjectId> others;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == chosen) continue;
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            others.push_back(id);
        }
        GameObjectId other = pickTarget(ctx, "Convergent Mutation (copy Might from)", others);
        if (other == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (other == kInvalidId || !ctx.state.objectExists(other)) return;
        int target_might = ctx.state.getObject(other).current_might;
        int cur = ctx.state.getObject(chosen).current_might;
        int delta = target_might - cur;
        if (delta > 0) ctx.executor.giveTemporaryMight(chosen, delta);
        ctx.events.logTrace("CONVERGENT MUTATION: set Might to " +
                             std::to_string(target_might));
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 108;
        d.def_id = R"RB(ogn-108-298)RB";
        d.name = R"RB(Convergent Mutation)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-108/298)RB";
        d.collector_number = 108;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a friendly unit. This turn, increase its Might to the Might of another friendly unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/61d414fc67ff52b9082e282e628dd0916afd8454-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_108(CardRegistry& r) {
    r.registerCard(108, std::make_unique<ConvergentMutation>());
}

} // namespace riftbound
