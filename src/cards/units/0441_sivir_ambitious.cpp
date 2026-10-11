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

// "[Deflect 2] ... When I conquer after an attack, if you assigned 5 or more
//  excess damage to enemy units, you may deal that much to an enemy unit."

class SivirAmbitious : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    // "after an attack, if you assigned 5 or more excess damage": the conquer
    // follows a combat here that I attacked in (as Tryndamere, Barbarian).
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType, GameObjectId) const override {
        if (!state.objectExists(source)) return false;
        const auto bf = state.getObject(source).battlefieldId();
        if (!bf) return false;
        const auto& ps = state.player(controller);
        return ps.last_combat_bf == *bf && ps.last_combat_attacker &&
               excessCombatDamageAt(state, controller, *bf) >= 5;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId me = ctx.controller;
        auto enemies = [&]() {
            return chooseableUnits(ctx.state, me,
                                   [me](const GameObject& o) { return o.controller != me; });
        };
        // "you may deal that much to an enemy unit"
        int conf = confirmOptional(ctx, "Sivir, Ambitious: deal the excess damage to an enemy unit?",
                                   [&]() { return !enemies().empty(); });
        GameObjectId tgt = kInvalidId;
        if (conf == 1) tgt = pickTarget(ctx, "Sivir, Ambitious: choose an enemy unit", enemies());
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (conf < 1 || tgt == kInvalidId) return;   // waiting / declined / mistarget
        // "that much": the excess damage of the combat that led to this conquer.
        const int amount = ctx.state.player(me).excess_combat_damage;
        if (amount <= 0) return;
        ctx.executor.dealDamage(tgt, amount, ctx.source);
        if (ctx.state.getObject(tgt).hasLethalDamage()) ctx.executor.killObject(tgt);
        ctx.events.logTrace("SIVIR AMBITIOUS: conquer after an attack -> " +
                            std::to_string(amount) + " to an enemy unit");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 441;
        d.def_id = R"RB(sfd-120-221)RB";
        d.name = R"RB(Sivir, Ambitious)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-120/221)RB";
        d.collector_number = 120;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Sivir)RB", R"RB(Shurima)RB"};
        d.energy_cost = 6;
        d.power_cost = 3;
        d.might = 7;
        d.rarity = Rarity::Epic;
        d.deflect_value = 2;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect 2] (Opponents must pay [A][A] to choose me with a spell or Ability.)
When I conquer after an attack, if you assigned 5 or more excess damage to enemy units, you may deal that much to an enemy unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bd3f242c9ca59cfc2d7dc5b146877c66bb310bb7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_441(CardRegistry& r) {
    r.registerCard(441, std::make_unique<SivirAmbitious>());
}

} // namespace riftbound
