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

// "When you conquer here, you may ready a friendly gear. If it's an Equipment,
//  you may detach it."

class VeiledTemple : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        ctx.executor.readyObject(targets[0]);
        // If it's an Equipment, you may detach it.
        auto& gear = ctx.state.getObject(targets[0]);
        if (isEquipment(gear) && gear.attached_to.has_value()) {
            int conf = confirmOptional(ctx, "Veiled Temple: detach the Equipment?",
                                       [&]() {
                if (!ctx.state.objectExists(targets[0])) return false;
                return ctx.state.getObject(targets[0]).attached_to.has_value();
            });
            if (conf == 1) {
                ctx.executor.unattachGear(targets[0]);
                ctx.events.logTrace("VEILED TEMPLE: detached Equipment");
            }
        }
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_gear = true, .must_be_friendly = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 535;
        d.def_id = R"RB(sfd-221-221)RB";
        d.name = R"RB(Veiled Temple)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-221/221)RB";
        d.collector_number = 221;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here, you may ready a friendly gear. If it's an Equipment, you may detach it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6f2b38874a09b3e3df3fe584ea77e84aa5423e37-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_535(CardRegistry& r) {
    r.registerCard(535, std::make_unique<VeiledTemple>());
}

} // namespace riftbound
