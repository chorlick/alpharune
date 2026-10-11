#include "cards/card.h"
#include "cards/card_helpers.h"
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

class ShenLeaderOfTheKinkouOrder : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // ── behaviour (Vendetta batch C) ──
    // [Shield] is an engine-handled keyword.
    // "When I hold, if there is exactly one other unit you control here, you
    //  score 1 point."
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        if (!self.isAtBattlefield()) return;
        if (otherFriendlyUnitsHere(ctx.state, self) != 1) return;
        ctx.executor.scorePoints(ctx.controller, 1);   // CR 194.1.c
        RIFT_TRACE(ctx.events, "SHEN, LEADER OF THE KINKOU ORDER: hold with exactly one "
                            "other friendly unit here -> score 1 point");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 925;
        d.def_id = R"RB(ven-138-166)RB";
        d.name = R"RB(Shen, Leader of the Kinkou Order)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-138/166)RB";
        d.collector_number = 138;
        d.artist = R"RB(Pandart Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Shen)RB", R"RB(Ionia)RB"};
        d.energy_cost = 6;
        d.power_cost = 2;
        d.might = 7;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Shield);
        d.shield_value = 1;
        d.ability_text = R"RB([Shield] (+1 [M] while I'm a defender.)
When I hold, if there is exactly one other unit you control here, you score 1 point.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b227d9f9abc6f165beffba4eaf3b5819cf06eab4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_925(CardRegistry& r) {
    r.registerCard(925, std::make_unique<ShenLeaderOfTheKinkouOrder>());
}

}  // namespace riftbound
