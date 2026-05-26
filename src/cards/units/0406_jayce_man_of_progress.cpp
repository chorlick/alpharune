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

class JayceManOfProgress : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    GameObjectId findFriendlyGear(CardContext& ctx) const {
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source) continue;
            if (!obj.isGear() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            return id;
        }
        return kInvalidId;
    }

    std::vector<GameObjectId> eligibleHandGears(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        auto& ps = ctx.state.player(ctx.controller);
        for (auto cid : ps.hand) {
            if (!ctx.state.objectExists(cid)) continue;
            auto& obj = ctx.state.getObject(cid);
            if (!obj.isGear() || obj.card_def_id == kInvalidId) continue;
            if (ctx.executor.cardDB().get(obj.card_def_id).energy_cost > 7) continue;
            out.push_back(cid);
        }
        return out;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto still_legal = [&]() { return findFriendlyGear(ctx) != kInvalidId; };
        int conf = confirmOptional(ctx, "Jayce: kill a friendly gear?", still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined / nothing to kill

        GameObjectId gear = findFriendlyGear(ctx);
        if (gear == kInvalidId || !ctx.state.objectExists(gear)) return;
        ctx.executor.killObject(gear);
        ctx.events.logTrace("JAYCE: killed a friendly gear");

        // "If you do, you may play a gear (cost <= 7) from hand ignoring its
        // Energy cost." Pick a hand gear and play it free.
        auto eligible = eligibleHandGears(ctx);
        if (eligible.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Jayce: play a gear from hand", eligible);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto& ps = ctx.state.player(ctx.controller);
        auto it = std::find(ps.hand.begin(), ps.hand.end(), picked);
        if (it != ps.hand.end()) ps.hand.erase(it);
        ctx.executor.playIgnoringCost(ctx.controller, picked);
        ctx.events.logTrace("JAYCE: played a gear from hand ignoring cost");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 406;
        d.def_id = R"RB(sfd-084-221)RB";
        d.name = R"RB(Jayce, Man of Progress)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-084/221)RB";
        d.collector_number = 84;
        d.artist = R"RB(TUTU)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Jayce)RB", R"RB(Piltover)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, you may kill a friendly gear. If you do, you may play a gear with Energy cost no more than [7] from hand this turn, ignoring its Energy cost. (You must still pay its Power cost.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b508fa4e52ebbd5f66e1fc7df28cfb70acdfe1f5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_406(CardRegistry& r) {
    r.registerCard(406, std::make_unique<JayceManOfProgress>());
}

} // namespace riftbound
