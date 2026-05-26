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

class BubbleBot : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        GameObjectId pick = kInvalidId;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source) continue;                 // "another"
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (!hasTag(obj, "Mech")) continue;
            if (obj.is_exhausted) { pick = id; break; }     // prefer exhausted
            if (pick == kInvalidId) pick = id;
        }
        if (pick == kInvalidId) return;
        ctx.executor.readyObject(pick);
        ctx.events.logTrace("BUBBLE BOT: readied another friendly Mech");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 385;
        d.def_id = R"RB(sfd-062-221)RB";
        d.name = R"RB(Bubble Bot)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-062/221)RB";
        d.collector_number = 62;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Mech)RB", R"RB(Yordle)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.ability_text = R"RB(When you play me, ready another friendly Mech.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c0fe9df24ae4b8a5b4fb8276b1bd6c5a5f5f7b2b-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_385(CardRegistry& r) {
    r.registerCard(385, std::make_unique<BubbleBot>());
}

} // namespace riftbound
