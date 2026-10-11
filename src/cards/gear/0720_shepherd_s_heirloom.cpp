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
        gainXp(ctx, ctx.controller, 1);
        ctx.events.logTrace("SHEPHERD'S HEIRLOOM: play -> gain 1 XP");
    }

    bool hasEquipAbility() const override { return true; }
    // "[Equip] — Spend 1 XP": no Energy or Power; the XP is the
    // non-resource part of the cost (CR 818.1.c.3).
    EquipCost equipCost(const GameState&, PlayerId, GameObjectId, GameObjectId) const override {
        return {.valid = true};
    }
    bool canPayEquipExtraCost(const GameState& state, PlayerId controller, GameObjectId,
                              GameObjectId) const override {
        return state.player(controller).xp >= 1;
    }
    bool payEquipExtraCost(CardContext& ctx, GameObjectId) override {
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.xp < 1) return false;
        ps.xp -= 1;
        ctx.events.logTrace("  EQUIP_COST: spent 1 XP");
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
