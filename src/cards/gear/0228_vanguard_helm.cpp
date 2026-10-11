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

class VanguardHelm : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When a buffed friendly unit dies, buff another friendly unit."
    // The condition is on the unit that died, as it last existed on the board
    // (CR 383.2.c.3.b).
    bool triggerCondition(const GameState& state, GameObjectId, PlayerId, TriggerType,
                          GameObjectId subject) const override {
        if (subject == kInvalidId || !state.objectExists(subject)) return false;
        const auto& dead = state.getObject(subject);
        return (dead.last_known.valid ? dead.last_known.buff_count : dead.buff_count) > 0;
    }
    TriggerType triggerType() const override { return TriggerType::WhenAFriendlyUnitDies; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Prefer to buff a friendly unit ("if it doesn't have a buff, +1").
        std::vector<GameObjectId> targets;
        for (auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (id == ctx.source) continue;
            if (obj.isUnit() && obj.controller == ctx.controller && obj.location.has_value())
                targets.push_back(id);
        }
        // No return on an empty list: pickTarget stores "no target" on the
        // chain item, so the resolution asks nothing if a friendly unit has
        // arrived since.
        GameObjectId picked = pickTarget(ctx, "Vanguard Helm (buff a friendly unit)", targets);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        if (ctx.state.getObject(picked).buff_count == 0)
            ctx.executor.buffUnit(picked);
        RIFT_TRACE(ctx.events, "VANGUARD HELM: friendly death -> buff a friendly unit");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 228;
        d.def_id = R"RB(ogn-228-298)RB";
        d.name = R"RB(Vanguard Helm)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-228/298)RB";
        d.collector_number = 228;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When a buffed friendly unit dies, buff another friendly unit. (If it doesn't have a buff, it gets a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9718c666cbf789a572760c826d0a06b26b787ae5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_228(CardRegistry& r) {
    r.registerCard(228, std::make_unique<VanguardHelm>());
}

} // namespace riftbound
