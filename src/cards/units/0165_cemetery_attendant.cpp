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

class CemeteryAttendant : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "When you play me, return a unit from your trash to your hand."
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> trash_units;
        for (auto cid : ps.trash) {
            if (!ctx.state.objectExists(cid)) continue;
            if (ctx.state.getObject(cid).isUnit()) trash_units.push_back(cid);
        }
        if (trash_units.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Cemetery Attendant (unit from trash)",
                                          trash_units);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto it = std::find(ps.trash.begin(), ps.trash.end(), picked);
        if (it == ps.trash.end()) return;
        ps.trash.erase(it);
        auto& obj = ctx.state.getObject(picked);
        obj.zone = ZoneType::Hand;
        obj.location = std::nullopt;
        ps.hand.push_back(picked);
        ctx.events.logTrace("CEMETERY ATTENDANT: returned " + obj.name +
                             " from trash to hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 165;
        d.def_id = R"RB(ogn-165-298)RB";
        d.name = R"RB(Cemetery Attendant)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-165/298)RB";
        d.collector_number = 165;
        d.artist = R"RB(Bubble Cat Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Dog)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.ability_text = R"RB(When you play me, return a unit from your trash to your hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6bbbe6923105c53fc6b2a44430db940e009609b6-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_165(CardRegistry& r) {
    r.registerCard(165, std::make_unique<CemeteryAttendant>());
}

} // namespace riftbound
