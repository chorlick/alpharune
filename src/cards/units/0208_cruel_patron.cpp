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

class CruelPatron : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "As an additional cost to play me, kill a friendly unit." A mandatory
    // additional cost (CR 356.2.a): without a friendly unit to kill I cannot
    // be played, and the unit — the player's choice — dies as I am played,
    // before my cost is paid and before I enter the board.
    static std::vector<GameObjectId> friendlyUnits(const GameState& state, PlayerId player) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects)
            if (obj.isUnit() && obj.controller == player && obj.location.has_value())
                out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
    }
    bool canBePlayed(const GameState& state, PlayerId player) const override {
        return !friendlyUnits(state, player).empty();
    }
    AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB& db,
                                          PlayerId player, GameObjectId self) const override {
        (void)db; (void)self;
        AdditionalPlayCost c;
        c.exists = true;
        c.mandatory = true;
        c.payable = !friendlyUnits(state, player).empty();
        return c;
    }
    PlayCostAdjust payAdditionalPlayCost(
        CardContext& ctx,
        const std::function<bool(const PlayCostAdjust&)>& affordable) override {
        (void)affordable;
        const GameObjectId unit = ctx.executor.chooseObjectNow(
            ctx.controller, friendlyUnits(ctx.state, ctx.controller));
        if (unit != kInvalidId) {
            ctx.executor.killObject(unit);
            RIFT_TRACE(ctx.events, "CRUEL PATRON: killed a friendly unit (additional cost)");
        }
        return {};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 208;
        d.def_id = R"RB(ogn-208-298)RB";
        d.name = R"RB(Cruel Patron)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-208/298)RB";
        d.collector_number = 208;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 4;
        d.might = 6;
        d.ability_text = R"RB(As an additional cost to play me, kill a friendly unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c1c2c08fa0032a245c1ffee6ba29fff2826bd468-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_208(CardRegistry& r) {
    r.registerCard(208, std::make_unique<CruelPatron>());
}

} // namespace riftbound
