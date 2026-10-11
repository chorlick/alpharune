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

class PitRookie : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, buff another friendly unit." My controller chooses
    // which, as the ability goes on the chain (CR 383.3). A unit that already
    // has a buff can be chosen and simply isn't buffed (CR 427.1.c).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> others;
        for (const auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (id == ctx.source) continue;
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            others.push_back(id);
        }
        std::sort(others.begin(), others.end());
        const GameObjectId t = pickTarget(ctx, "Pit Rookie: buff another friendly unit", others);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId) return;          // none / mistarget / waiting
        ctx.executor.buffUnit(t);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 136;
        d.def_id = R"RB(ogn-136-298)RB";
        d.name = R"RB(Pit Rookie)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-136/298)RB";
        d.collector_number = 136;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.ability_text = R"RB(When you play me, buff another friendly unit. (If it doesn't have a buff, it gets a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ec19eeaee85c7e5669387a3f6ccb7718f5a0f570-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_136(CardRegistry& r) {
    r.registerCard(136, std::make_unique<PitRookie>());
}

} // namespace riftbound
