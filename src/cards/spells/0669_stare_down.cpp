#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class StareDown : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "Choose a friendly unit and a battlefield. Move all enemy units at that
    //  battlefield with less Might than the chosen unit to their base. Gain 1 XP."
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    bool needsPlayTimeTarget() const override { return true; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        const auto locked = lockedPlayTarget(ctx, targets);
        const GameObjectId chosen = locked ? *locked
            : pickTarget(ctx, "Stare Down: choose a friendly unit",
                         enumerateLegalTargets(ctx.state, ctx.controller));
        if (chosen == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;   // waiting for the agent (the XP below is gained once)
        // "and a battlefield": the controller's choice among all of them.
        const auto& bfs = ctx.state.battlefields;
        const int n = static_cast<int>(std::min<size_t>(bfs.size(), 32));
        std::vector<std::string> labels;
        for (int i = 0; i < n; ++i)
            labels.push_back(ctx.state.objectExists(bfs[i].card_object_id)
                ? ctx.state.getObject(bfs[i].card_object_id).name
                : "Battlefield #" + std::to_string(static_cast<int>(bfs[i].id)));
        const int mode = pickMode(ctx, "Stare Down: battlefield", n, labels);
        if (mode == -1) return;   // waiting for the agent

        if (chosen != kInvalidId && ctx.state.objectExists(chosen) && mode >= 0 && mode < n) {
            const int threshold = ctx.state.getObject(chosen).current_might;
            const BattlefieldId bf = bfs[mode].id;
            std::vector<GameObjectId> weaker;
            for (const auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller == ctx.controller) continue;
                const auto at = obj.battlefieldId();
                if (!at.has_value() || *at != bf) continue;
                if (obj.current_might < threshold) weaker.push_back(id);
            }
            for (auto id : weaker) ctx.executor.moveToBase(id);
            RIFT_TRACE(ctx.events, "STARE DOWN: moved enemy units (< " +
                                 std::to_string(threshold) + "M) at a BF to base");
        }
        // "Gain 1 XP." does not depend on the chosen unit.
        gainXp(ctx, ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 669;
        d.def_id = R"RB(unl-107-219)RB";
        d.name = R"RB(Stare Down)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-107/219)RB";
        d.collector_number = 107;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Choose a friendly unit and a battlefield. Move all enemy units at that battlefield with less Might than the chosen unit to their base. Gain 1 XP.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/58d0353a966bb0ca292855a0a55de5769e28d155-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_669(CardRegistry& r) {
    r.registerCard(669, std::make_unique<StareDown>());
}

} // namespace riftbound
