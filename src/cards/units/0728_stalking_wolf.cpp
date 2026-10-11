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

// [Ambush] engine-handled.
// "As an additional cost to play me, kill a Bird, Cat, Dog, or Poro you
//  control. You may play me to its battlefield (even if you don't have other
//  units there)."
//
// The engine has no hook for a "kill a unit" additional play cost, so:
//  - canBePlayed gates the play on having a Bird, Cat, Dog or Poro to kill;
//  - the kill is the cost of my WhenYouPlayMe ability, chosen and paid as
//    that ability goes on the chain (CR 383.3), before anyone can respond.
// APPROX: the kill happens just after I enter the board, not before. "Its
// battlefield, even if you don't have other units there" needs nothing more:
// the unit to be killed is still at its battlefield while the Ambush
// locations are offered, so a battlefield where it is the only friendly unit
// is offered like any other.

class StalkingWolf : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    static bool isFodder(const GameObject& obj, PlayerId player) {
        return obj.isUnit() && obj.controller == player && obj.location.has_value() &&
               (hasTag(obj, "Bird") || hasTag(obj, "Cat") ||
                hasTag(obj, "Dog") || hasTag(obj, "Poro"));
    }
    // "You may [Ambush] me to its battlefield, even if you don't have other
    // units there." needs no code of its own: the unit that will be killed is
    // still at its battlefield while the engine offers [Ambush] locations
    // (battlefields where you have units, CR 822.1.b), so its battlefield is
    // always among them, and no battlefield without a unit of yours is.
    bool canBePlayed(const GameState& state, PlayerId player) const override {
        for (const auto& [id, obj] : state.objects)
            if (isFodder(obj, player)) return true;
        return false;
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!costAlreadyPaid(ctx)) {
            std::vector<GameObjectId> legal;
            for (auto& [id, obj] : ctx.state.objects)
                if (id != ctx.source && isFodder(obj, ctx.controller)) legal.push_back(id);
            std::sort(legal.begin(), legal.end());
            // Paying a cost, not choosing a unit: no Deflect, no "chosen" trigger.
            auto victim = selectObject(
                ctx, "Stalking Wolf: kill a Bird/Cat/Dog/Poro you control", legal);
            if (!victim.has_value() || *victim == kInvalidId) return;   // waiting / none
            if (costDueNow(ctx)) {
                ctx.executor.killObject(*victim);
                ctx.events.logTrace("STALKING WOLF: killed a Bird/Cat/Dog/Poro (additional cost)");
            }
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 728;
        d.def_id = R"RB(unl-166-219)RB";
        d.name = R"RB(Stalking Wolf)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-166/219)RB";
        d.collector_number = 166;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Dog)RB", R"RB(Freljord)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Ambush);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Ambush] (You may play me as a [Reaction] to a battlefield where you have units.)
As an additional cost to play me, kill a Bird, Cat, Dog, or Poro you control. You may [Ambush] me to its battlefield, even if you don't have other units there.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5f5f66caad21d59bf966ddc501bb4f9c84a595c6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_728(CardRegistry& r) {
    r.registerCard(728, std::make_unique<StalkingWolf>());
}

} // namespace riftbound
