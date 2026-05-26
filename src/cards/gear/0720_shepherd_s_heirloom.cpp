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

class ShepherdSHeirloom : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 1;
        ctx.events.logTrace("SHEPHERD'S HEIRLOOM: play -> gain 1 XP");
    }

    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.xp < 1) return false;  // can't pay the cost
        if (!ctx.state.objectExists(unit)) return false;
        ps.xp -= 1;  // Spend 1 XP
        ctx.events.logTrace("  EQUIP_COST: spent 1 XP");

        auto& gear = ctx.state.getObject(ctx.source);
        auto& u = ctx.state.getObject(unit);
        gear.attached_to = unit;
        u.attachments.push_back(ctx.source);
        gear.location = u.location;
        gear.zone = u.zone;
        u.attachment_might_bonus += gear.might_bonus;
        u.recomputeMight();
        ctx.events.logTrace("EQUIP: " + gear.name + " -> " + u.name);
        ctx.events.emit(ObjectStateChangedEvent{ctx.source, "attached"});
        ctx.events.emit(ObjectStateChangedEvent{unit, "equipped"});
        return true;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 720;
        d.def_id = R"RB(unl-158-219)RB";
        d.name = R"RB(Shepherd's Heirloom)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-158/219)RB";
        d.collector_number = 158;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 2;
        d.might_bonus = 2;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB(When you play this, gain 1 XP.
[Equip] — Spend 1 XP (Pay the cost: Attach this to a unit you control.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fadd95487f164f60c9ad08a457f11170c6afa420-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_720(CardRegistry& r) {
    r.registerCard(720, std::make_unique<ShepherdSHeirloom>());
}

} // namespace riftbound
