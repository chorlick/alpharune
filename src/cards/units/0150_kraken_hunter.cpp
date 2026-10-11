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

class KrakenHunter : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Accelerate] / [Assault] are engine-handled keywords.
    // "As you play me, you may spend any number of buffs as an additional cost.
    // Reduce my cost by [O] for each buff you spend." An additional cost
    // chosen before payment (CR 356.2.b): one buff at a time, from a friendly
    // unit of the player's choice; the player may stop once what is left can
    // be paid.
    static std::vector<GameObjectId> buffed(const GameState& state, PlayerId player) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects.onBoard())
            if (obj.isUnit() && obj.controller == player && obj.location.has_value() &&
                obj.buff_count > 0) out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
    }
    AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB& db,
                                          PlayerId player, GameObjectId self) const override {
        (void)db; (void)self;
        int buffs = 0;
        for (auto id : buffed(state, player)) buffs += state.getObject(id).buff_count;
        AdditionalPlayCost c;
        c.exists = true;
        c.payable = buffs > 0;
        c.best.power_reduction = std::min(buffs, def_.power_cost);
        return c;
    }
    PlayCostAdjust payAdditionalPlayCost(
        CardContext& ctx,
        const std::function<bool(const PlayCostAdjust&)>& affordable) override {
        PlayCostAdjust adj;
        while (adj.power_reduction < def_.power_cost) {
            const auto units = buffed(ctx.state, ctx.controller);
            if (units.empty()) break;
            if (affordable(adj) &&
                !ctx.executor.confirmNow(ctx.controller,
                                         "Kraken Hunter: spend a buff to cost [O] less"))
                break;
            const GameObjectId unit = ctx.executor.chooseObjectNow(ctx.controller, units);
            if (unit == kInvalidId) break;
            ctx.executor.spendBuff(unit, ctx.controller);
            ++adj.power_reduction;
        }
        return adj;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 150;
        d.def_id = R"RB(ogn-150-298)RB";
        d.name = R"RB(Kraken Hunter)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-150/298)RB";
        d.collector_number = 150;
        d.artist = R"RB(JiHun Lee)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Pirate)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 3;
        d.power_cost = 2;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.assault_value = 1;
        d.keywords.set(Keyword::Accelerate);
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB([Accelerate] (You may pay [1][O] as an additional cost to have me enter ready.)
[Assault] (+1 [M] while I'm an attacker.)
As you play me, you may spend any number of buffs as an additional cost. Reduce my cost by [O] for each buff you spend.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/51aab4710d000a9c1e665a37ef8c919ff11b0282-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_150(CardRegistry& r) {
    r.registerCard(150, std::make_unique<KrakenHunter>());
}

} // namespace riftbound
