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

class BottledConstellation : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "At the start of your Main Phase, you may kill 3 other friendly units
    //  and/or gear to score 1 point."
    // The leading "may" is decided as the ability is finalized (CR 383.3.a),
    // and "kill 3 ... to" is a cost in the instruction right after it: the
    // ability's base cost, chosen and paid as it is finalized (CR 383.3.b,
    // 404.1.b.1, 405). The three are not targets (CR 355.10.c.1). They die
    // together; a death that something replaces is not a kill, and then the
    // cost is unpaid and no point is scored (Card::killAsCost, the engine's
    // reading for "kill X to" abilities; CR 357.2.a may differ: judge
    // question 16).
    TriggerType triggerType() const override { return TriggerType::AtStartOfMain; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto fodder = [&](const std::vector<GameObjectId>& taken) {
            return boardObjects(ctx.state, [&](const GameObject& obj) {
                if (obj.id == ctx.source || obj.controller != ctx.controller) return false;
                if (!obj.isUnit() && !obj.isGear()) return false;
                return std::find(taken.begin(), taken.end(), obj.id) == taken.end();
            });
        };
        const int go = confirmOptional(
            ctx, "Bottled Constellation: kill 3 other friendly units/gear to score 1 point",
            [&]() { return costAlreadyPaid(ctx) || fodder({}).size() >= 3; });
        if (go == -1) return;   // waiting for the agent
        std::vector<GameObjectId> victims;
        if (go == 1) {
            // Three picks, one prompt each (slots 7, 9, 11). A lone option is
            // taken without asking.
            for (size_t k = 0; k < 3; ++k) {
                const auto pick = selectObject(ctx,
                    "Bottled Constellation: kill which? (" + std::to_string(k + 1) + " of 3)",
                    fodder(victims), 7 + 2 * k);
                if (!pick.has_value()) return;   // waiting for the agent
                if (*pick != kInvalidId) victims.push_back(*pick);
            }
            if (costDueNow(ctx) && !costUnpaid(ctx)) {
                bool paid = victims.size() == 3;
                {   // they die at the same time; the batch is for these kills only
                    DeathBatch together(ctx.executor, victims);
                    for (auto id : victims) {
                        if (ctx.state.getObject(id).isGear()) ctx.executor.unattachGear(id);
                        paid = killAsCost(ctx, id) && paid;
                    }
                }
                if (!paid) markCostUnpaid(ctx);
            }
        }
        if (finalizingChoices(ctx)) return;   // choices and the cost end here (CR 383.3)
        if (go != 1 || costUnpaid(ctx)) return;
        // CR 194.1.c: scorePoints honours "can't gain points" and ends the
        // game at once at the Victory Score.
        if (ctx.executor.scorePoints(ctx.controller, 1) == 0)
            RIFT_TRACE(ctx.events, "BOTTLED CONSTELLATION: can't gain points");
        else
            RIFT_TRACE(ctx.events, "BOTTLED CONSTELLATION: scored 1 point -> " +
                std::to_string(ctx.state.player(ctx.controller).score));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 854;
        d.def_id = R"RB(ven-067-166)RB";
        d.name = R"RB(Bottled Constellation)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-067/166)RB";
        d.collector_number = 67;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 10;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(At the start of your Main Phase, you may kill 3 other friendly units and/or gear to score 1 point.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2068b7c202cab7f877c331a7bad0531d6a8f0d73-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_854(CardRegistry& r) {
    r.registerCard(854, std::make_unique<BottledConstellation>());
}

}  // namespace riftbound
