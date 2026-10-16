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

class ReaverSRow : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When you defend here, you may move a friendly unit here to base."
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        GameObjectId unit = kInvalidId;
        {   // the engine never hands a trigger targets: the card picks
            auto unitsHere = [&]() {
                std::vector<GameObjectId> out;
                const auto here = battlefieldOfCard(ctx.state, ctx.source);
                for (const auto& bf : ctx.state.battlefields) {
                    if (bf.id != here) continue;
                    for (auto& [id, obj] : ctx.state.objects.onBoard()) {
                        if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                        auto ubf = obj.battlefieldId();
                        if (ubf && *ubf == bf.id) out.push_back(id);
                    }
                }
                return out;
            };
            int go = confirmOptional(ctx, "Reaver's Row: move a friendly unit here to base?",
                                     [&]() { return !unitsHere().empty(); });
            if (go < 1) return;   // waiting on the agent, declined or no unit here
            unit = pickTarget(ctx, "Reaver's Row: move which unit to base", unitsHere());
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (unit == kInvalidId || !ctx.state.objectExists(unit)) return;
        ctx.executor.moveToBase(unit);
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouDefendHere; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true, .must_be_at_battlefield = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 280;
        d.def_id = R"RB(ogn-285-298)RB";
        d.name = R"RB(Reaver's Row)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-285/298)RB";
        d.collector_number = 285;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you defend here, you may move a friendly unit here to base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7d99fe84c9b815463198b5dbad694759940cac12-1038x744.png)RB";
        d.banned = true;  // tournament ban (first ban list; was missing here)
        return d;
    }();
};

}  // anonymous namespace

void register_card_280(CardRegistry& r) {
    r.registerCard(280, std::make_unique<ReaverSRow>());
}

} // namespace riftbound
