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

class RoyalEntourage : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    // "When you play me, ready or exhaust a legend."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // Enumerate legend objects (either player's). Triggers get empty
        // targets, so pick at resolve time.
        std::vector<GameObjectId> legends;
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.card_type == CardType::Legend) legends.push_back(id);
        }
        int mode = pickMode(ctx, "Royal Entourage: ready or exhaust?", 2,
                            {"Ready", "Exhaust"});
        if (mode < 0) return;  // suspended, or nothing to choose
        GameObjectId tgt = pickTarget(ctx, "Royal Entourage: ready/exhaust a legend",
                                      legends);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (tgt == kInvalidId || !ctx.state.objectExists(tgt)) return;
        if (mode == 0) {
            ctx.executor.readyObject(tgt);
            RIFT_TRACE(ctx.events, "ROYAL ENTOURAGE: readied a legend");
        } else {
            ctx.executor.exhaustObject(tgt);
            RIFT_TRACE(ctx.events, "ROYAL ENTOURAGE: exhausted a legend");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 362;
        d.def_id = R"RB(sfd-039-221)RB";
        d.name = R"RB(Royal Entourage)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-039/221)RB";
        d.collector_number = 39;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 4;
        d.ability_text = R"RB(When you play me, ready or exhaust a legend.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a6f3ce580860fe80fdf1b41533ba89b5060e946a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_362(CardRegistry& r) {
    r.registerCard(362, std::make_unique<RoyalEntourage>());
}

} // namespace riftbound
