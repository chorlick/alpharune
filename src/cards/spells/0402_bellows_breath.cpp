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

class BellowsBreath : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 3, .must_be_unit = true, .optional = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty()) return;
        // "at the same location" — anchor on the first chosen target's
        // location; only damage targets sharing it. Collect first, kill
        // lethal after (AoE iterator-safety per CLAUDE.md).
        std::optional<LocationId> anchor;
        if (ctx.state.objectExists(targets[0]))
            anchor = ctx.state.getObject(targets[0]).location;
        if (!anchor.has_value()) return;

        std::vector<GameObjectId> hit;
        for (auto t : targets) {
            if (!ctx.state.objectExists(t)) continue;
            auto& obj = ctx.state.getObject(t);
            if (obj.location != anchor) continue;
            // de-dup (action gen may repeat in degenerate cases)
            if (std::find(hit.begin(), hit.end(), t) != hit.end()) continue;
            hit.push_back(t);
        }
        for (auto t : hit) {
            if (ctx.state.objectExists(t)) ctx.executor.dealDamage(t, 1, ctx.source);
        }
        for (auto t : hit) {
            if (ctx.state.objectExists(t) &&
                ctx.state.getObject(t).hasLethalDamage()) {
                ctx.executor.killObject(t);
            }
        }
        ctx.events.logTrace("BELLOWS BREATH: dealt 1 to " +
                             std::to_string(hit.size()) + " units at same location");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 402;
        d.def_id = R"RB(sfd-080-221)RB";
        d.name = R"RB(Bellows Breath)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-080/221)RB";
        d.collector_number = 80;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
[Repeat] [1][B] (You may pay the additional cost to repeat this spell's effect.)
Deal 1 to up to three units at the same location.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f0ae1c4bb788f6fadc251c9d9b36f3f92eef4c56-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_402(CardRegistry& r) {
    r.registerCard(402, std::make_unique<BellowsBreath>());
}

} // namespace riftbound
