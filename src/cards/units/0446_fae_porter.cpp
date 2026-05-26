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

// "When I move to a battlefield, you may pay [P] to move a unit you control to
//  the same battlefield." ([P] = Chaos power.)

class FaePorter : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIMoveToFB; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return;
        auto movable = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source) continue;
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                if (obj.battlefieldId() == my_bf) continue;  // already here
                out.push_back(id);
            }
            return out;
        };
        // "you may pay [P]" — gate on having a Chaos power source + a movable unit.
        auto still_legal = [&]() {
            return !movable().empty();
        };
        int conf = confirmOptional(ctx, "Fae Porter: pay [P] to move a unit here?", still_legal);
        if (conf == -1) return;  // waiting on agent
        if (conf == 0) return;   // declined / not legal
        GameObjectId tgt = pickTarget(ctx, "Fae Porter: move which unit here?", movable());
        if (tgt == kInvalidId) return;  // suspend or no target
        if (!ctx.state.objectExists(tgt)) return;
        if (!payOnePower(ctx, ctx.controller, Domain::Chaos)) return;
        ctx.executor.moveToBattlefield(tgt, *my_bf);
        ctx.events.logTrace("FAE PORTER: paid [P] -> moved a unit to my battlefield");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 446;
        d.def_id = R"RB(sfd-125-221)RB";
        d.name = R"RB(Fae Porter)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-125/221)RB";
        d.collector_number = 125;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Fae)RB", R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.ability_text = R"RB(When I move to a battlefield, you may pay [P] to move a unit you control to the same battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4d233b11d4efb2652d7ede556dd9f730903cdba9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_446(CardRegistry& r) {
    r.registerCard(446, std::make_unique<FaePorter>());
}

} // namespace riftbound
