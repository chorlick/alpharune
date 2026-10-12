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

class VolibearImposing : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Shield 3] / [Tank] are engine-handled keywords.
    // "When an opponent moves to a battlefield other than mine, draw 1."
    // Fires via WhenAnOpponentMovesToBattlefield; the moved enemy unit is the
    // subject. Gate: I must be at a battlefield and the destination must differ.
    TriggerType triggerType() const override {
        return TriggerType::WhenAnOpponentMovesToBattlefield;
    }
    // "to a battlefield other than mine" is part of the trigger condition:
    // judged when the move happens (the mover is already where it went). A
    // move to my battlefield, or any move while I'm not at a battlefield,
    // puts nothing on the chain.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId subject) const override {
        if (subject == kInvalidId || !state.objectExists(subject)) return false;
        if (!state.objectExists(source)) return false;
        const auto mine = state.getObject(source).battlefieldId();
        if (!mine) return false;   // I'm not at a battlefield -> "other than mine" doesn't apply
        const auto to = state.getObject(subject).battlefieldId();
        return to.has_value() && *to != *mine;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.drawCards(ctx.controller, 1);
        RIFT_TRACE(ctx.events, "VOLIBEAR IMPOSING: opponent moved elsewhere -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 158;
        d.def_id = R"RB(ogn-158-298)RB";
        d.name = R"RB(Volibear, Imposing)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-158/298)RB";
        d.collector_number = 158;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Volibear)RB", R"RB(Freljord)RB"};
        d.energy_cost = 12;
        d.power_cost = 2;
        d.might = 10;
        d.rarity = Rarity::Rare;
        d.shield_value = 3;
        d.keywords.set(Keyword::Shield);
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Shield 3] (+3 [M] while I'm a defender.)
[Tank] (I must be assigned combat damage first.)
When an opponent moves to a battlefield other than mine, draw 1. (Bases are not battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bcb15f95f4a72f8b070a3b1cd54e6482fe1a4b3e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_158(CardRegistry& r) {
    r.registerCard(158, std::make_unique<VolibearImposing>());
}

} // namespace riftbound
