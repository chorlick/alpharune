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
// The kill is an additional cost chosen and paid as I am played
// (Card::additionalPlayCost / payAdditionalPlayCost). "Its battlefield, even
// if you don't have other units there" needs nothing more: the unit to be
// killed is still at its battlefield while the Ambush locations are offered,
// so a battlefield where it is the only friendly unit is offered like any
// other.

class StalkingWolf : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // A mandatory additional cost (CR 356.2.a): without a Bird, Cat, Dog or
    // Poro to kill I cannot be played, and the one the player chooses dies as
    // I am played — before my cost is paid and before I enter the board.
    // Paying a cost is not choosing a unit: no Deflect, no "chosen" trigger.
    static bool isFodder(const GameObject& obj, PlayerId player) {
        return obj.isUnit() && obj.controller == player && obj.location.has_value() &&
               (hasTag(obj, "Bird") || hasTag(obj, "Cat") ||
                hasTag(obj, "Dog") || hasTag(obj, "Poro"));
    }
    static std::vector<GameObjectId> fodder(const GameState& state, PlayerId player) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects)
            if (isFodder(obj, player)) out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
    }
    bool canBePlayed(const GameState& state, PlayerId player) const override {
        return !fodder(state, player).empty();
    }
    AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB& db,
                                          PlayerId player, GameObjectId self) const override {
        (void)db; (void)self;
        AdditionalPlayCost c;
        c.exists = true;
        c.mandatory = true;
        c.payable = !fodder(state, player).empty();
        return c;
    }
    PlayCostAdjust payAdditionalPlayCost(
        CardContext& ctx,
        const std::function<bool(const PlayCostAdjust&)>& affordable) override {
        (void)affordable;
        const GameObjectId victim = ctx.executor.chooseObjectNow(
            ctx.controller, fodder(ctx.state, ctx.controller));
        if (victim != kInvalidId) {
            ctx.executor.killObject(victim);
            ctx.events.logTrace("STALKING WOLF: killed a Bird/Cat/Dog/Poro (additional cost)");
        }
        return {};
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
