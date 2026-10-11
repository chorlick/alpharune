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

class CommanderLedros : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "As you play me, you may kill any number of friendly units as an
    // additional cost. Reduce my cost by [Y] for each killed this way." An
    // additional cost chosen before payment (CR 356.2.b): one unit at a time,
    // of the player's choice; the player may stop once what is left can be
    // paid. [Deflect] and [Ganking] are engine keywords.
    static std::vector<GameObjectId> friendlyUnits(const GameState& state, PlayerId player) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects)
            if (obj.isUnit() && obj.controller == player && obj.location.has_value())
                out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
    }
    AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB& db,
                                          PlayerId player, GameObjectId self) const override {
        (void)db; (void)self;
        const int units = static_cast<int>(friendlyUnits(state, player).size());
        AdditionalPlayCost c;
        c.exists = true;
        c.payable = units > 0;
        c.best.power_reduction = std::min(units, def_.power_cost);
        return c;
    }
    PlayCostAdjust payAdditionalPlayCost(
        CardContext& ctx,
        const std::function<bool(const PlayCostAdjust&)>& affordable) override {
        PlayCostAdjust adj;
        while (adj.power_reduction < def_.power_cost) {
            const auto units = friendlyUnits(ctx.state, ctx.controller);
            if (units.empty()) break;
            if (affordable(adj) &&
                !ctx.executor.confirmNow(ctx.controller,
                                         "Commander Ledros: kill a friendly unit to cost [Y] less"))
                break;
            const GameObjectId unit = ctx.executor.chooseObjectNow(ctx.controller, units);
            if (unit == kInvalidId) break;
            ctx.executor.killObject(unit);
            ++adj.power_reduction;
        }
        return adj;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 231;
        d.def_id = R"RB(ogn-231-298)RB";
        d.name = R"RB(Commander Ledros)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-231/298)RB";
        d.collector_number = 231;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Spirit)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 6;
        d.power_cost = 4;
        d.might = 8;
        d.rarity = Rarity::Rare;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB(As you play me, you may kill any number of friendly units as an additional cost. Reduce my cost by [Y] for each killed this way.
[Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
[Ganking] (I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1183be5effc6275f17da09b983feb36632752af4-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_231(CardRegistry& r) {
    r.registerCard(231, std::make_unique<CommanderLedros>());
}

} // namespace riftbound
