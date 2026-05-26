#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class Mistfall : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you buff a friendly unit, you may pay [O] and exhaust this to ready it."
    // Fires via WhenYouBuffAFriendlyUnit; the buffed unit is the trigger subject.
    TriggerType triggerType() const override {
        return TriggerType::WhenYouBuffAFriendlyUnit;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        GameObjectId subject = ctx.state.chain.resuming
            ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
        GameObjectId self = ctx.source;
        auto still_legal = [&]() {
            return ctx.state.objectExists(self) &&
                   !ctx.state.getObject(self).is_exhausted &&
                   subject != kInvalidId && ctx.state.objectExists(subject);
        };
        if (!still_legal()) return;
        int conf = confirmOptional(ctx,
            "Mistfall: pay [O] + exhaust to ready the buffed unit?", still_legal);
        if (conf < 1) return;
        if (!payOnePower(ctx, ctx.controller, Domain::Body)) return;  // [O]
        ctx.state.getObject(self).is_exhausted = true;                // exhaust this
        ctx.executor.readyObject(subject);
        ctx.events.logTrace("MISTFALL: paid [O]+exhaust -> ready buffed unit");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 152;
        d.def_id = R"RB(ogn-152-298)RB";
        d.name = R"RB(Mistfall)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-152/298)RB";
        d.collector_number = 152;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.energy_cost = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you buff a friendly unit, you may pay [O] and exhaust this to ready it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1cedf0dcd6c4a44ae867e9f04f9f179d9b91c357-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_152(CardRegistry& r) {
    r.registerCard(152, std::make_unique<Mistfall>());
}

} // namespace riftbound
