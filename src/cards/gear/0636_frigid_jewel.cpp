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

class FrigidJewel : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you draw your second card each turn, give a friendly unit +2 [M]
    // this turn." WhenYouDrawACard fires per draw; it triggers on the draw
    // that takes the controller's draws_this_turn to 2 or more, once per turn
    // (card_counters[kFiredTurn] on this gear). Which friendly unit is the
    // controller's choice, made as the ability is finalized (CR 383.3).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouDrawACard; }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType, GameObjectId /*subject*/) const override {
        if (state.player(controller).draws_this_turn < 2) return false;   // not yet the 2nd draw
        if (!state.objectExists(source)) return false;
        const auto& counters = state.getObject(source).card_counters;
        auto it = counters.find(kFiredTurn);
        return it == counters.end() || it->second != state.turn.turn_number + 1;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // Once per turn. Bookkeeping only (and idempotent), so it is safe ahead
        // of the choice: a later draw this turn must not trigger again while
        // this ability is still on the chain.
        if (ctx.state.objectExists(ctx.source))
            ctx.state.getObject(ctx.source).card_counters[kFiredTurn] =
                ctx.state.turn.turn_number + 1;   // +1: avoid the 0-default
        std::vector<GameObjectId> friendly;
        for (const auto& [id, obj] : ctx.state.objects)
            if (obj.isUnit() && obj.controller == ctx.controller && obj.location.has_value())
                friendly.push_back(id);
        std::sort(friendly.begin(), friendly.end());
        const GameObjectId tgt =
            pickTarget(ctx, "Frigid Jewel: give a friendly unit +2 [M]", friendly);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (tgt == kInvalidId) return;        // none / mistarget / waiting
        ctx.executor.giveTemporaryMight(tgt, 2);
        RIFT_TRACE(ctx.events, "FRIGID JEWEL: 2nd draw -> a friendly unit +2 [M] this turn");
    }
private:
    static constexpr const char* kFiredTurn = "__frigid_fired_turn";
    const CardDef def_ = [] {
        CardDef d;
        d.id = 636;
        d.def_id = R"RB(unl-074-219)RB";
        d.name = R"RB(Frigid Jewel)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-074/219)RB";
        d.collector_number = 74;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you draw your second card each turn, give a friendly unit +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f1a6c28649bf4478b4728379b6d548058739f0e9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_636(CardRegistry& r) {
    r.registerCard(636, std::make_unique<FrigidJewel>());
}

} // namespace riftbound
