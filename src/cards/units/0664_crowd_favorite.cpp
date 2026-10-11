#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class CrowdFavorite : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Hunt] — gain 1 XP on conquer/hold.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        gainXp(ctx, ctx.controller, 1);
    }
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    // "Spend 2 XP: [Buff] me. (Give me a +1 [M] buff if I don't have one.)"
    // The XP is the activation cost: the engine checks and charges it as the
    // ability is activated (ActivationCost::xp_cost).
    // Activating it while I already have a buff is LEGAL and is offered: the
    // text has no "use only if", and CR 427.1.c says a unit with a Buff "can
    // still be chosen for actions that Buff units, but will not be Buffed as
    // part of the execution" (427.1.b.1). The 2 XP is spent for no effect; the
    // engine does not hide a legal action because it is a poor one.
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost cost;
        cost.xp_cost = 2;
        return cost;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // buffUnit places a Buff only on a unit on the board without one.
        if (ctx.executor.buffUnit(ctx.source))
            RIFT_TRACE(ctx.events, "CROWD FAVORITE: spent 2 XP -> [Buff] me");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 664;
        d.def_id = R"RB(unl-102-219)RB";
        d.name = R"RB(Crowd Favorite)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-102/219)RB";
        d.collector_number = 102;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Hunt);
        d.ability_text = R"RB([Hunt] (When I conquer or hold, gain 1 XP.)
Spend 2 XP: [Buff] me. (Give me a +1 [M] buff if I don't have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0031569ff123a8317e1be753f0bd895501bf838b-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_664(CardRegistry& r) {
    r.registerCard(664, std::make_unique<CrowdFavorite>());
}

} // namespace riftbound
