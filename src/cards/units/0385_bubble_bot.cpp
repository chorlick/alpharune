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
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "When you play me, ready another friendly Mech." — the controller's choice.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> mechs;
        for (auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (id == ctx.source) continue;                 // "another"
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (hasTag(obj, "Mech")) mechs.push_back(id);
        }
        const GameObjectId pick = pickTarget(ctx, "Bubble Bot: ready another friendly Mech", mechs);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (pick == kInvalidId) return;       // none / mistarget / waiting
        ctx.executor.readyObject(pick);
        RIFT_TRACE(ctx.events, "BUBBLE BOT: readied another friendly Mech");
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
