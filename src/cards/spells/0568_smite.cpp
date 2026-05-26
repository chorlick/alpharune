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

class Smite : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // "Deal 3 to a unit at a battlefield. If it would die this turn,
        // banish it instead." The lingering turn-long death-replacement has
        // no engine hook; we apply the banish on the immediate lethal damage
        // (the common case), banishing rather than killing to trash.
        if (!targets.empty()) {
            ctx.executor.dealDamage(targets[0], 3, ctx.source);
            if (ctx.state.objectExists(targets[0]) &&
                ctx.state.getObject(targets[0]).hasLethalDamage()) {
                ctx.executor.banishObject(targets[0]);
                ctx.events.logTrace("SMITE: lethal -> banish instead of kill "
                                    "(lingering turn-long replacement not engine-wired)");
            }
        }
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_at_battlefield = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 568;
        d.def_id = R"RB(unl-007-219)RB";
        d.name = R"RB(Smite)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-007/219)RB";
        d.collector_number = 7;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Deal 3 to a unit at a battlefield. If it would die this turn, banish it instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f46252e8c65a2eb3f476ea51f214af7651d0622d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_568(CardRegistry& r) {
    r.registerCard(568, std::make_unique<Smite>());
}

} // namespace riftbound
