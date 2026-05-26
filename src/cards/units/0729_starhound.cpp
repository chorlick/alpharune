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

// "When you play me, return a Bird, Cat, Dog, or Poro from your trash to your
// hand." Target lives in the trash, not on the board — enumerate the trash
// with a tag filter and pick at trigger resolution.

class Starhound : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> legal;
        for (auto cid : ps.trash) {
            if (!ctx.state.objectExists(cid)) continue;
            auto& obj = ctx.state.getObject(cid);
            if (!obj.isUnit()) continue;
            if (hasTag(obj, "Bird") || hasTag(obj, "Cat") ||
                hasTag(obj, "Dog") || hasTag(obj, "Poro")) {
                legal.push_back(cid);
            }
        }
        if (legal.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Starhound (Bird/Cat/Dog/Poro from trash)",
                                          legal);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto it = std::find(ps.trash.begin(), ps.trash.end(), picked);
        if (it == ps.trash.end()) return;
        ps.trash.erase(it);
        auto& obj = ctx.state.getObject(picked);
        obj.zone = ZoneType::Hand;
        obj.location = std::nullopt;
        ps.hand.push_back(picked);
        ctx.events.logTrace("STARHOUND: returned " + obj.name +
                             " from trash to hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 729;
        d.def_id = R"RB(unl-167-219)RB";
        d.name = R"RB(Starhound)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-167/219)RB";
        d.collector_number = 167;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Dog)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, return a Bird, Cat, Dog, or Poro from your trash to your hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/085edeb9e6bbb0fb71ab902393eb48ea33a8addc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_729(CardRegistry& r) {
    r.registerCard(729, std::make_unique<Starhound>());
}

} // namespace riftbound
