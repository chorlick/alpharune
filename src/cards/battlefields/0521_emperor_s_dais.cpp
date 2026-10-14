#include "cards/card.h"
#include "cards/card_helpers.h"
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
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // Find "here" — the battlefield whose card object is this card.
        BattlefieldId here = kInvalidId;
        for (auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == ctx.source) { here = bf.id; break; }
        }
        if (here == kInvalidId) return;

        auto unitsHere = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects.onBoard()) {
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                auto bf = obj.battlefieldId();
                if (bf && *bf == here) out.push_back(id);
            }
            return out;
        };

        // "You may" — only offered when both the [1] and a unit here exist.
        int decision = confirmOptional(ctx, "Emperor's Dais: pay [1], return a unit",
            [&]() { return canPayEnergy(ctx, ctx.controller, 1) && !unitsHere().empty(); });
        if (decision < 1) return;   // waiting on the agent, declined or not legal

        // The cost: [1] and returning a unit you control here (your choice).
        if (costDueNow(ctx)) {
            GameObjectId unit = pickTarget(ctx, "Emperor's Dais: return which unit", unitsHere());
            if (unit == kInvalidId || !ctx.state.objectExists(unit)) return;
            if (!payEnergy(ctx, ctx.controller, 1)) return;
            ctx.executor.bounceToHand(unit);
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)

        // "to play a 2 [M] Sand Soldier unit token here."
        auto loc = LocationId{BattlefieldLocation{here}};
        // CR 187.3: a Sand Soldier token has 2 Might and the Shurima tag.
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sand Soldier", 2,
                                 {"Shurima"}, KeywordSet{}, loc);
        RIFT_TRACE(ctx.events, "EMPEROR'S DAIS: paid [1], bounced unit, spawned 2M Sand Soldier");
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
        d.ability_text = R"RB(When you conquer here, you may pay [1] and return a unit you control here to its owner's hand to play a 2 [M] Sand Soldier unit token here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c1ea4f6f58a62fc2b62647aa3459109e3d10297a-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_521(CardRegistry& r) {
    r.registerCard(521, std::make_unique<EmperorSDais>());
}

} // namespace riftbound
