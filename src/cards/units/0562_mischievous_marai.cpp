#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

// "When you play me to a battlefield, deal 2 to an enemy unit here."
// A condition on where she was played, not a permission to be played to any
// battlefield: no extraPlayLocations(). She reaches a battlefield her
// controller no longer controls only by being played from face down there
// ([Hidden]: "A hidden permanent must be played to that battlefield", CR 811.1.d.1).

class MischievousMarai : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "to a battlefield" is part of the trigger condition (CR 383.2.a.1):
    // asked as she is played, so no ability goes on the chain for a Marai
    // played to a base — moving her to a battlefield afterwards changes nothing.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return state.objectExists(source) && state.getObject(source).isAtBattlefield();
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        PlayerId opp = opponent(ctx.controller);
        // This is not the trigger condition asked a second time ("to a
        // battlefield" is triggerCondition, asked once as she is played): it
        // is what "here" refers to, which is checked as the instruction is
        // executed (CR 359.3.f.2 — Yasuo moved back to base in response:
        // "here" is no longer that battlefield and the trigger mistargets).
        // So a Marai who is at no battlefield as this resolves has no unit
        // "here": the list is empty and pickTarget turns the unit chosen at
        // finalization into a mistarget. No return ahead of pickTarget — it
        // stores "no target" on the chain item.
        std::vector<GameObjectId> legal;
        if (my_bf) {
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller != opp) continue;
                if (obj.untargetable_by_enemy) continue;
                auto bf = obj.battlefieldId();
                if (bf && *bf == *my_bf) legal.push_back(id);
            }
        }
        GameObjectId picked = pickTarget(ctx, "Mischievous Marai: deal 2 to enemy unit here",
                                         legal);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (picked == kInvalidId) return;
        if (!ctx.state.objectExists(picked)) return;
        ctx.executor.dealDamage(picked, 2, ctx.source);
        if (ctx.state.objectExists(picked) &&
            ctx.state.getObject(picked).hasLethalDamage())
            ctx.executor.killObject(picked);
        RIFT_TRACE(ctx.events, "MISCHIEVOUS MARAI: dealt 2 to an enemy unit here");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 562;
        d.def_id = R"RB(unl-003-219)RB";
        d.name = R"RB(Mischievous Marai)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-003/219)RB";
        d.collector_number = 3;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
When you play me to a battlefield, deal 2 to an enemy unit here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fb2e82cca7aa93c857ed1fe54de7344016779520-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_562(CardRegistry& r) {
    r.registerCard(562, std::make_unique<MischievousMarai>());
}

} // namespace riftbound
