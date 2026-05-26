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

class FoxFire : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Kill any number of units at a battlefield with total Might 4 or less."
    // Anchor on a chosen unit (identifies the battlefield), then kill units
    // there greedily (lowest Might first) while the running total stays <= 4.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId anchor;
        if (!targets.empty()) {
            anchor = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            anchor = pickTarget(ctx, "Fox-Fire (a unit at a battlefield)", legal);
        }
        if (anchor == kInvalidId || !ctx.state.objectExists(anchor)) return;
        auto bf = ctx.state.getObject(anchor).battlefieldId();
        if (!bf) return;

        // Gather all units at that battlefield, sorted by Might ascending.
        std::vector<std::pair<int, GameObjectId>> here;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit()) continue;
            if (obj.battlefieldId() != bf) continue;
            here.emplace_back(obj.current_might, id);
        }
        std::sort(here.begin(), here.end());

        int budget = 4;
        std::vector<GameObjectId> to_kill;
        for (auto& [might, id] : here) {
            if (might <= budget) {
                to_kill.push_back(id);
                budget -= might;
            }
        }
        for (auto id : to_kill)
            if (ctx.state.objectExists(id)) ctx.executor.killObject(id);
        ctx.events.logTrace("FOX-FIRE: killed " + std::to_string(to_kill.size()) +
                            " units (total Might <= 4) at one BF");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_at_battlefield = true, .max_might = 4};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 253;
        d.def_id = R"RB(ogn-256-298)RB";
        d.name = R"RB(Fox-Fire)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-256/298)RB";
        d.collector_number = 256;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Mind};
        d.tags = {R"RB(Ahri)RB"};
        d.energy_cost = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
Kill any number of units at a battlefield with total Might 4 or less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7ad9d6a46a1c54080d54950a0044da3a82e32b45-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_253(CardRegistry& r) {
    r.registerCard(253, std::make_unique<FoxFire>());
}

} // namespace riftbound
