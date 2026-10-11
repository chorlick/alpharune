#include "cards/card_helpers.h"
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

class Wallop : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // "Ready a unit." (main effect — fully implemented)
        if (!targets.empty())
            ctx.executor.readyObject(targets[0]);
    }
    // "As you play this, you may spend a buff as an additional cost. If you
    // do, ignore this spell's cost." An additional cost chosen before payment
    // (CR 356.2.b, 356.1.b): the buff comes from a friendly unit of the
    // player's choice, and with it the spell is playable with no Energy.
    static std::vector<GameObjectId> buffed(const GameState& state, PlayerId player) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects)
            if (obj.isUnit() && obj.controller == player && obj.location.has_value() &&
                obj.buff_count > 0) out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
    }
    AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB& db,
                                          PlayerId player, GameObjectId self) const override {
        (void)db; (void)self;
        AdditionalPlayCost c;
        c.exists = true;
        c.payable = !buffed(state, player).empty();
        c.best.ignore_cost = true;
        return c;
    }
    PlayCostAdjust payAdditionalPlayCost(
        CardContext& ctx,
        const std::function<bool(const PlayCostAdjust&)>& affordable) override {
        const auto units = buffed(ctx.state, ctx.controller);
        if (units.empty()) return {};
        if (affordable(PlayCostAdjust{}) &&
            !ctx.executor.confirmNow(ctx.controller, "spend a buff to ignore this spell's cost"))
            return {};
        const GameObjectId unit = ctx.executor.chooseObjectNow(ctx.controller, units);
        if (unit == kInvalidId || !spendBuff(ctx, unit)) return {};
        PlayCostAdjust adj;
        adj.ignore_cost = true;
        return adj;
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 146;
        d.def_id = R"RB(ogn-146-298)RB";
        d.name = R"RB(Wallop)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-146/298)RB";
        d.collector_number = 146;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
As you play this, you may spend a buff as an additional cost. If you do, ignore this spell's cost.
Ready a unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7bbd2eb8ce224e1872a1c920d7c64d796b2355cd-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_146(CardRegistry& r) {
    r.registerCard(146, std::make_unique<Wallop>());
}

} // namespace riftbound
