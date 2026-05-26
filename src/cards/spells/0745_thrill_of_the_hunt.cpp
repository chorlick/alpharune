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

class ThrillOfTheHunt : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool needsPlayTimeTarget() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId unit_id = pickTarget(ctx, "Thrill of the Hunt", legal);
        if (unit_id == kInvalidId || !ctx.state.objectExists(unit_id)) return;
        auto owner = ctx.state.getObject(unit_id).owner;
        std::string uname = ctx.state.getObject(unit_id).name;

        // Step 1: banish the friendly unit.
        ctx.executor.banishObject(unit_id);
        // banishObject pushes to banishment; playIgnoringCost re-zones it, so
        // drop the duplicate banishment entry.
        auto& bz = ctx.state.player(owner).banishment;
        bz.erase(std::remove(bz.begin(), bz.end(), unit_id), bz.end());

        // Step 2: owner plays it to ANY battlefield, ignoring cost. Use
        // pickMode so the agent records the battlefield choice (one option
        // per battlefield on the board, not just controlled ones).
        std::vector<LocationId> locations;
        std::vector<std::string> labels;
        for (const auto& bf : ctx.state.battlefields) {
            locations.push_back(LocationId{BattlefieldLocation{bf.id}});
            std::string bf_name = "BF#" + std::to_string(static_cast<int>(bf.id));
            if (ctx.state.objectExists(bf.card_object_id)) {
                bf_name = ctx.state.getObject(bf.card_object_id).name;
            }
            labels.push_back(bf_name);
        }
        if (locations.empty()) {
            // No battlefields — fall back to owner's base.
            ctx.executor.playIgnoringCost(owner, unit_id,
                                           LocationId{BaseLocation{owner}});
            ctx.events.logTrace("THRILL: " + uname + " (no BF) played to base");
            return;
        }
        uint32_t legal_mask = (locations.size() >= 32)
            ? 0xFFFFFFFFu
            : ((1u << locations.size()) - 1);
        int chosen = pickMode(ctx, "Thrill: which battlefield",
                              static_cast<int>(locations.size()), labels, legal_mask);
        if (chosen == -1) return;  // yielded for agent input
        if (chosen < 0 || static_cast<size_t>(chosen) >= locations.size()) chosen = 0;
        ctx.executor.playIgnoringCost(owner, unit_id, locations[chosen]);
        ctx.events.logTrace("THRILL: " + uname + " played to " + labels[chosen] +
                             ", ignoring cost");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 745;
        d.def_id = R"RB(unl-184-219)RB";
        d.name = R"RB(Thrill of the Hunt)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-184/219)RB";
        d.collector_number = 184;
        d.artist = R"RB(华锐)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Body};
        d.tags = {R"RB(Rengar)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Banish a friendly unit, then its owner plays it to any battlefield, ignoring its cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2753c2b3ea4fd55e5225a4451a29736b5c3434a8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_745(CardRegistry& r) {
    r.registerCard(745, std::make_unique<ThrillOfTheHunt>());
}

} // namespace riftbound
