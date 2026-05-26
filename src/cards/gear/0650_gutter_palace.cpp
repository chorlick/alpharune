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

class GutterPalace : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // ── (1) Beginning-phase win check ──
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto& ps = ctx.state.player(ctx.controller);
        int hand = static_cast<int>(ps.hand.size());
        int units_at_bf = 0;
        for (const auto& [id, obj] : ctx.state.objects) {
            if (obj.isUnit() && obj.controller == ctx.controller &&
                obj.battlefieldId().has_value()) {
                ++units_at_bf;
            }
        }
        if (hand == 4 && units_at_bf == 4) {
            ctx.state.game_over = true;
            ctx.state.winner = ctx.controller;
            ctx.state.game_over_reason = "Gutter Palace win condition";
            ctx.events.logTrace("GUTTER PALACE: win condition met (4 hand / 4 units)");
        }
    }

    // ── (2) Activated: discard 1, [E] -> make a 1[M] Bird token w/ [Deflect] ──
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true, .discard = true, .discard_count = 1};
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 0};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        KeywordSet kw; kw.set(Keyword::Deflect);
        auto loc = ctx.state.getObject(ctx.source).location
                       .value_or(LocationId{BaseLocation{ctx.controller}});
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Bird", 1,
                                  {"Bird"}, kw, loc, /*enter_ready=*/false);
        ctx.events.logTrace("GUTTER PALACE: play 1M Bird token with [Deflect]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 650;
        d.def_id = R"RB(unl-088-219)RB";
        d.name = R"RB(Gutter Palace)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-088/219)RB";
        d.collector_number = 88;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 4;
        d.rarity = Rarity::Epic;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB(At the start of your Beginning Phase, if you have exactly 4 cards in hand and exactly 4 units at battlefields, you win the game.
Discard 1, [E]: Play a 1 [M] Bird unit token with [Deflect]. (Opponents must pay [A] to choose it with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f1addf7bb3925871cbcf4615f5a449dc61f762da-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_650(CardRegistry& r) {
    r.registerCard(650, std::make_unique<GutterPalace>());
}

} // namespace riftbound
