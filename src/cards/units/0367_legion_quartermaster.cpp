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

class LegionQuartermaster : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "As an additional cost to play me, return a friendly gear to its
    // owner's hand." Mandatory additional cost. The engine has no structured
    // "return a gear" cost hook, so we (1) gate playability on the presence
    // of a friendly on-board gear via hasLegalTargets, and (2) perform the
    // bounce as a WhenYouPlayMe effect. NOTE: the bounce resolves just after
    // I enter rather than strictly before (no true pre-cost hook).
    static bool hasFriendlyGear(const GameState& state, PlayerId controller) {
        for (auto& [id, obj] : state.objects) {
            if (!obj.isGear() || obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            return true;
        }
        return false;
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return hasFriendlyGear(state, controller);
    }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        std::vector<GameObjectId> gears;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isGear() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            gears.push_back(id);
        }
        GameObjectId gear = pickTarget(ctx,
            "Legion Quartermaster: return a friendly gear to hand", gears);
        if (gear == kInvalidId || !ctx.state.objectExists(gear)) return;
        ctx.executor.bounceToHand(gear);
        ctx.events.logTrace("LEGION QUARTERMASTER: returned a friendly gear to hand "
                            "(additional cost)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 367;
        d.def_id = R"RB(sfd-044-221)RB";
        d.name = R"RB(Legion Quartermaster)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-044/221)RB";
        d.collector_number = 44;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Trifarian)RB", R"RB(Noxus)RB"};
        d.energy_cost = 3;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(As an additional cost to play me, return a friendly gear to its owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c09cc35ffc98b46f919c09e09a93b603d63eb73e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_367(CardRegistry& r) {
    r.registerCard(367, std::make_unique<LegionQuartermaster>());
}

} // namespace riftbound
