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

// "[Deflect] [2][R]: Double my Might this turn." ([Deflect] engine-handled.)

class ViHotheaded : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::Activated; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = false, .energy = 2,
                              .power = 1, .power_domain = Domain::Fury};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        int cur = ctx.state.getObject(ctx.source).current_might;
        if (cur <= 0) return;
        // Double = add current Might as a "this turn" bonus.
        ctx.executor.giveTemporaryMight(ctx.source, cur);
        ctx.events.logTrace("VI HOTHEADED: doubled Might this turn (+" +
                            std::to_string(cur) + ")");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 592;
        d.def_id = R"RB(unl-030-219)RB";
        d.name = R"RB(Vi, Hotheaded)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-030/219)RB";
        d.collector_number = 30;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Vi)RB", R"RB(Zaun)RB"};
        d.energy_cost = 4;
        d.might = 3;
        d.rarity = Rarity::Epic;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
[2][R]: Double my Might this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/03a55e8d494d7efe20da792365161f5c43c20779-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_592(CardRegistry& r) {
    r.registerCard(592, std::make_unique<ViHotheaded>());
}

} // namespace riftbound
