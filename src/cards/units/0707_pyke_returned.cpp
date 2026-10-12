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

class PykeReturned : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "Once each turn, when an enemy unit dies while I'm at a battlefield,
    // play a Gold gear token exhausted." [Hidden]/[Backline] engine-handled.
    TriggerType triggerType() const override { return TriggerType::WhenAnEnemyUnitDies; }
    // "while I'm at a battlefield" and "once each turn" are the trigger's
    // condition (CR 383.2.a.1, 383.3.e.1): asked when the enemy unit dies. A
    // Pyke at his base, or one whose ability was already performed this turn
    // or is still on the chain, puts nothing on the chain.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType trigger, GameObjectId /*subject*/) const override {
        if (!state.objectExists(source)) return false;
        const auto& self = state.getObject(source);
        if (!self.isAtBattlefield()) return false;
        const auto it = self.card_counters.find("__pyke_token_turn");
        if (it != self.card_counters.end() && it->second == state.turn.turn_number) return false;
        return !triggeredAbilityOnChain(state, source, trigger);
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        // Not asked again where Pyke is: he was at a battlefield when the unit
        // died, and the ability resolves even if he has left since.
        self.card_counters["__pyke_token_turn"] = ctx.state.turn.turn_number;
        // CR 149.2: gear is played to its controller's Base unless an effect
        // says otherwise ("while I'm at a battlefield" is the condition, not a
        // place for the token). Exhausted, as the real Gold card (CR 187.5).
        createGoldExhausted(ctx);
        RIFT_TRACE(ctx.events, "PYKE RETURNED: enemy unit died -> Gold gear token (exhausted)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 707;
        d.def_id = R"RB(unl-145-219)RB";
        d.name = R"RB(Pyke, Returned)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-145/219)RB";
        d.collector_number = 145;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Pyke)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Backline);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Backline] (I must be assigned combat damage last.)
Once each turn, when an enemy unit dies while I'm at a battlefield, play a Gold gear token exhausted. (It has "[Reaction][>] Kill this, [E]: [Add] [A]."))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2eaa5396b1cac9b245bde5a56b87314f8ae76a38-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_707(CardRegistry& r) {
    r.registerCard(707, std::make_unique<PykeReturned>());
}

} // namespace riftbound
