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

class SunkenTemple : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    // "When you conquer here with one or more [Mighty] units": part of the
    // trigger condition, so it is checked when the conquer happens and not
    // again as the ability resolves (losing the Mighty unit in response does
    // not undo an ability that already triggered and was paid for).
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        const auto my_bf = battlefieldOfCard(state, source);
        if (!my_bf) return false;
        for (const auto& [id, obj] : state.objects.onBoard())
            if (obj.isUnit() && obj.controller == controller &&
                obj.battlefieldId() == my_bf && isMighty(obj)) return true;
        return false;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // CR 445.2 / 164.2.a: the [1] may come from floating Energy or from
        // exhausting a ready rune (payableEnergy / payEnergy).
        auto still_legal = [&]() {
            return payableEnergy(ctx.state, ctx.controller) >= 1;
        };
        // No return ahead of the "may": with [1] unpayable confirmOptional
        // stores the "no" on the chain item, so the resolution asks nothing
        // if the [1] has become payable since (CR 383.3.a).
        int conf = confirmOptional(ctx, "Sunken Temple: pay [1] to draw 1?",
                                   still_legal);
        if (conf < 1) return;
        if (costDueNow(ctx) && !payEnergy(ctx, ctx.controller, 1)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        ctx.executor.drawCards(ctx.controller, 1);
        RIFT_TRACE(ctx.events, "SUNKEN TEMPLE: paid [1] -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 532;
        d.def_id = R"RB(sfd-218-221)RB";
        d.name = R"RB(Sunken Temple)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-218/221)RB";
        d.collector_number = 218;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here with one or more [Mighty] units, you may pay [1] to draw 1. (A unit is Mighty while it has 5+ [M].))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/06f6d17929d19000006cf281d013ecbe1543af0e-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_532(CardRegistry& r) {
    r.registerCard(532, std::make_unique<SunkenTemple>());
}

} // namespace riftbound
