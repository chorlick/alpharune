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
// The mandatory sacrifice is modeled as a WhenYouPlayMe forced kill of an
// agent-chosen Bird/Cat/Dog/Poro. APPROXIMATIONS / ENGINE GAP:
//  - Timing: the sacrifice fires as a play trigger (after Stalking Wolf is on
//    the board), not strictly as a pre-play additional cost.
//  - "play me to its battlefield": tying Stalking Wolf's landing zone to the
//    sacrificed unit's battlefield is not modeled — getPlayLocations is
//    consulted before the kill choice exists, so the location can't depend on
//    it. Stalking Wolf lands per the normal play rules.

class StalkingWolf : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> legal;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source) continue;
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (hasTag(obj, "Bird") || hasTag(obj, "Cat") ||
                hasTag(obj, "Dog") || hasTag(obj, "Poro")) {
                legal.push_back(id);
            }
        }
        if (legal.empty()) return;
        GameObjectId victim = pickTarget(ctx, "Stalking Wolf: kill a Bird/Cat/Dog/Poro you control",
                                         legal);
        if (victim == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended
        }
        if (victim == kInvalidId || !ctx.state.objectExists(victim)) return;
        ctx.executor.killObject(victim);
        ctx.events.logTrace("STALKING WOLF: sacrificed a Bird/Cat/Dog/Poro (additional cost)");
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
As an additional cost to play me, kill a Bird, Cat, Dog, or Poro you control. You may play me to its battlefield (even if you don't have other units there).)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5f5f66caad21d59bf966ddc501bb4f9c84a595c6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_728(CardRegistry& r) {
    r.registerCard(728, std::make_unique<StalkingWolf>());
}

} // namespace riftbound
