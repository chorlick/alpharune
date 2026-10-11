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
    // The conqueror decides everything as the ability is finalized (CR 383.3):
    // the "may", which gear, and — for an attached Equipment — whether to
    // detach it. One confirmOptional per callback, so the second "may" is a
    // two-mode pickMode.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto gears = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects)
                if (obj.isGear() && obj.controller == ctx.controller && obj.location.has_value())
                    out.push_back(id);
            std::sort(out.begin(), out.end());   // stable option order, not hash order
            return out;
        };
        const int go = confirmOptional(ctx, "Veiled Temple: ready a friendly gear?",
                                       [&]() { return !gears().empty(); });
        GameObjectId gear_id = kInvalidId;
        int detach = 0;
        if (go == 1) {
            gear_id = pickTarget(ctx, "Veiled Temple: ready which gear", gears());
            if (gear_id != kInvalidId && ctx.state.objectExists(gear_id)) {
                const auto& g = ctx.state.getObject(gear_id);
                if (isEquipment(g) && g.attached_to.has_value())
                    detach = pickMode(ctx, "Veiled Temple: detach the Equipment?", 2,
                                      {"Leave it attached", "Detach it"});
            }
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (go < 1 || gear_id == kInvalidId || detach < 0) return;   // declined / none / waiting
        ctx.executor.readyObject(gear_id);
        if (detach == 1 && ctx.state.getObject(gear_id).attached_to.has_value()) {
            ctx.executor.unattachGear(gear_id);
            ctx.events.logTrace("VEILED TEMPLE: detached Equipment");
        }
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
