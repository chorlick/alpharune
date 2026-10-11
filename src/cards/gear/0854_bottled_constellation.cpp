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
    // Resumable: yes/no (only offered with 3+ other friendly units/gear), then
    // three sequential picks; all three die together, then the point is scored.
    // resume_points: 0-2 confirmOptional, 20 = collecting picks, 21 = done.
    TriggerType triggerType() const override { return TriggerType::AtStartOfMain; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto fodder = [&](const std::vector<int32_t>& taken) {
            std::vector<GameObjectId> out;
            for (const auto& [id, obj] : ctx.state.objects.onBoard()) {
                if (id == ctx.source || obj.controller != ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                if (!obj.isUnit() && !obj.isGear()) continue;
                if (std::find(taken.begin(), taken.end(), static_cast<int32_t>(id.raw())) !=
                    taken.end()) continue;
                out.push_back(id);
            }
            std::sort(out.begin(), out.end());
            return out;
        };
        auto pay_and_score = [&](const std::vector<GameObjectId>& victims) {
            {   // the batch is for these kills only
                DeathBatch together(ctx.executor, victims);   // they die at the same time
                for (auto id : victims) {
                    if (!ctx.state.objectExists(id)) continue;
                    if (ctx.state.getObject(id).isGear()) ctx.executor.unattachGear(id);
                    ctx.executor.killObject(id);
                }
            }
            // CR 194.1.c: scorePoints honours "can't gain points" and ends the
            // game at once at the Victory Score.
            if (ctx.executor.scorePoints(ctx.controller, 1) == 0)
                RIFT_TRACE(ctx.events, "BOTTLED CONSTELLATION: can't gain points");
            else
                RIFT_TRACE(ctx.events, "BOTTLED CONSTELLATION: scored 1 point -> " +
                    std::to_string(ctx.state.player(ctx.controller).score));
        };
        const int go = confirmOptional(
            ctx, "Bottled Constellation: kill 3 other friendly units/gear to score 1 point",
            [&]() { return fodder({}).size() >= 3; });
        if (go != 1) return;
        if (!ctx.state.chain.resuming.has_value()) {
            // Direct invocation (no chain to suspend on): first three.
            auto all = fodder({});
            all.resize(3);
            pay_and_score(all);
            return;
        }
        auto& ri = *ctx.state.chain.resuming;
        auto prompt = [&]() {
            auto c = fodder(ri.resume_data);
            if (c.empty()) { ri.resume_point = 21; return; }
            std::vector<Intent> opts;
            for (auto id : c) opts.push_back(objChoice(ctx.controller, id));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                "Bottled Constellation: kill which? (" +
                std::to_string(ri.resume_data.size() + 1) + " of 3)");
            ri.resume_point = 20;
        };
        if (ri.resume_point == 2) {   // just confirmed
            ri.resume_data.clear();
            prompt();
            return;
        }
        if (ri.resume_point != 20) return;
        GameObjectId picked = takenObject(ctx);
        auto legal = fodder(ri.resume_data);
        if (legal.empty()) { ri.resume_point = 21; return; }
        if (std::find(legal.begin(), legal.end(), picked) == legal.end()) picked = legal.front();
        ri.resume_data.push_back(static_cast<int32_t>(picked.raw()));
        if (ri.resume_data.size() < 3) { prompt(); return; }
        ri.resume_point = 21;
        std::vector<GameObjectId> victims;
        for (auto v : ri.resume_data) victims.push_back(GameObjectId::fromRaw(v));
        pay_and_score(victims);
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
