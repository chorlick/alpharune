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

class SpectralMatron : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    // Available power [A]: floating power in the pool + the player's ready
    // (unexhausted) runes that could still be recycled for power.
    int availablePower(CardContext& ctx) const {
        auto& ps = ctx.state.player(ctx.controller);
        int avail = ps.rune_pool.totalPower();
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isRune() || obj.controller != ctx.controller) continue;
            if (obj.is_exhausted) continue;
            if (!obj.location.has_value()) continue;
            avail++;
        }
        return avail;
    }

    std::vector<GameObjectId> eligibleTrashUnits(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        auto& ps = ctx.state.player(ctx.controller);
        int avail = availablePower(ctx);
        for (auto cid : ps.trash) {
            if (!ctx.state.objectExists(cid)) continue;
            auto& obj = ctx.state.getObject(cid);
            if (!obj.isUnit()) continue;
            if (obj.card_def_id == kInvalidId) continue;
            int cost = ctx.executor.cardDB().get(obj.card_def_id).energy_cost;
            if (cost > 3) continue;       // "no more than [3]"
            if (cost > avail) continue;   // "no more than [A]" (available power)
            out.push_back(cid);
        }
        return out;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto still_legal = [&]() { return !eligibleTrashUnits(ctx).empty(); };
        int conf = confirmOptional(ctx,
            "Spectral Matron: play a <=3-cost unit from trash?", still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined / nothing eligible

        auto eligible = eligibleTrashUnits(ctx);
        if (eligible.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Spectral Matron (unit from trash)",
                                          eligible);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;

        auto& ps = ctx.state.player(ctx.controller);
        auto it = std::find(ps.trash.begin(), ps.trash.end(), picked);
        if (it != ps.trash.end()) ps.trash.erase(it);
        ctx.executor.playIgnoringCost(ctx.controller, picked);
        ctx.events.logTrace("SPECTRAL MATRON: played a unit from trash for free");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 226;
        d.def_id = R"RB(ogn-226-298)RB";
        d.name = R"RB(Spectral Matron)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-226/298)RB";
        d.collector_number = 226;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Spirit)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 4;
        d.power_cost = 2;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, you may play a unit costing no more than [3] and no more than [A] from your trash, ignoring its cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c8aebd03c8722d485ebfc38774488778c756ff5c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_226(CardRegistry& r) {
    r.registerCard(226, std::make_unique<SpectralMatron>());
}

} // namespace riftbound
