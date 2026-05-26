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

// "Bird, Cat, Dog, Poro, and Ivern units here have +1 [M]." (aura is engine-
//  handled). "When you score here, you may replace this with the battlefield
//  it replaced." Reverse-replace (CR 438) — restore the banished original.

class Brush : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouScoreHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Locate the BF slot this Brush card occupies.
        BattlefieldState* slot = nullptr;
        for (auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == ctx.source) { slot = &bf; break; }
        }
        if (!slot || !slot->was_replaced || !slot->replaced_card.has_value()) return;
        GameObjectId original = *slot->replaced_card;
        if (!ctx.state.objectExists(original)) return;

        int conf = confirmOptional(ctx, "Brush: replace this with the battlefield it replaced?",
                                   [&]() { return true; });
        if (conf != 1) return;

        // Send the Brush token to banishment (tokens cease to exist; this is a
        // BF token, route to banishment without tracking).
        if (ctx.state.objectExists(ctx.source)) {
            auto& tok = ctx.state.getObject(ctx.source);
            tok.zone = ZoneType::Banishment;
            tok.location = std::nullopt;
        }
        // Restore the original battlefield card into the slot.
        auto& orig = ctx.state.getObject(original);
        PlayerId owner = orig.owner;
        if (owner != PlayerId::None) {
            auto& bz = ctx.state.player(owner).banishment;
            bz.erase(std::remove(bz.begin(), bz.end(), original), bz.end());
        }
        orig.zone = ZoneType::BattlefieldZone;
        orig.location = LocationId{BattlefieldLocation{slot->id}};
        slot->card_object_id = original;
        slot->replaced_card.reset();
        slot->was_replaced = false;
        slot->is_token = false;
        ctx.events.logTrace("BRUSH: scored -> reverse-replaced back to original BF");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 561;
        d.def_id = R"RB(unl-t03)RB";
        d.name = R"RB(Brush)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-T03)RB";
        d.collector_number = 3;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.ability_text = R"RB(Bird, Cat, Dog, Poro, and Ivern units here have +1 [M].
When you score here, you may replace this with the battlefield it replaced.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fad09d6bd9bf38e376f430ecb0b400762420d061-1039x744.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_561(CardRegistry& r) {
    r.registerCard(561, std::make_unique<Brush>());
}

} // namespace riftbound
