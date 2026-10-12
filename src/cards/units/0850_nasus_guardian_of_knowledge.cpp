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

class NasusGuardianOfKnowledge : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "Once each turn, when an enemy unit here dies, channel 1 rune exhausted."
    TriggerType triggerType() const override { return TriggerType::WhenAnEnemyUnitDiesHere; }
    // "Once each turn" (CR 383.3.e.1): an ability that was already performed
    // this turn — or is still on the chain — does not trigger again. Asked
    // when the enemy unit dies ("here" is the engine's dispatch).
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType trigger, GameObjectId /*subject*/) const override {
        if (!state.objectExists(source)) return false;
        const auto& c = state.getObject(source).card_counters;
        const auto it = c.find("__nasus_guardian_turn");
        if (it != c.end() && it->second == state.turn.turn_number + 1) return false;
        return !triggeredAbilityOnChain(state, source, trigger);
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // Performed now, wherever Nasus is by then (the effect is not his own).
        ctx.state.getObject(ctx.source).card_counters["__nasus_guardian_turn"] =
            ctx.state.turn.turn_number + 1;   // +1 so 0 = never
        ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
        RIFT_TRACE(ctx.events, "NASUS, GUARDIAN OF KNOWLEDGE: enemy unit died here -> channel 1 exhausted");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 850;
        d.def_id = R"RB(ven-063-166)RB";
        d.name = R"RB(Nasus, Guardian of Knowledge)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-063/166)RB";
        d.collector_number = 63;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Dog)RB", R"RB(Shurima)RB", R"RB(Nasus)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Once each turn, when an enemy unit here dies, channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/aa96dbb8b4d72db7e47a72ce652ad542f55bf31d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_850(CardRegistry& r) {
    r.registerCard(850, std::make_unique<NasusGuardianOfKnowledge>());
}

}  // namespace riftbound
