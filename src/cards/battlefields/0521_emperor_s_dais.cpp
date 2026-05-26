#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class EmperorSDais : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // Find "here" — the battlefield whose card object is this card.
        BattlefieldId here = kInvalidId;
        for (auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == ctx.source) { here = bf.id; break; }
        }
        if (here == kInvalidId) return;

        // A friendly unit must be here to bounce.
        GameObjectId bounce_target = kInvalidId;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            auto bf = obj.battlefieldId();
            if (bf && *bf == here) { bounce_target = id; break; }
        }

        // Count a ready rune to pay [1].
        auto base_loc = BaseLocation{ctx.controller};
        GameObjectId pay_rune = kInvalidId;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isRune() || obj.controller != ctx.controller || obj.is_exhausted) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            pay_rune = id;
            break;
        }

        // "You may" — only offer when both the [1] and a unit-here exist.
        int decision = confirmOptional(ctx, "Emperor's Dais: pay [1], return a unit",
            [&]() {
                return bounce_target != kInvalidId && pay_rune != kInvalidId;
            });
        if (decision == -1) return;  // waiting on agent choice
        if (decision == 0) return;   // declined / not legal

        // Re-validate the pick (state may have shifted while deciding).
        if (!ctx.state.objectExists(bounce_target) || !ctx.state.objectExists(pay_rune)) return;
        // Pay [1] — exhaust the ready rune.
        ctx.state.getObject(pay_rune).is_exhausted = true;
        // Return the unit to its owner's hand.
        ctx.executor.bounceToHand(bounce_target);
        // "If you do" — play a 2M Sand Soldier unit token here.
        auto loc = LocationId{BattlefieldLocation{here}};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sand Soldier", 2,
                                 {"Sand Soldier"}, KeywordSet{}, loc);
        ctx.events.logTrace("EMPEROR'S DAIS: paid [1], bounced unit, spawned 2M Sand Soldier");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 521;
        d.def_id = R"RB(sfd-207-221)RB";
        d.name = R"RB(Emperor's Dais)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-207/221)RB";
        d.collector_number = 207;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here, you may pay [1] and return a unit you control here to its owner's hand. If you do, play a 2 [M] Sand Soldier unit token here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c1ea4f6f58a62fc2b62647aa3459109e3d10297a-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_521(CardRegistry& r) {
    r.registerCard(521, std::make_unique<EmperorSDais>());
}

} // namespace riftbound
