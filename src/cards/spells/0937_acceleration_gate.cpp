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

class AccelerationGate : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Ready up to 4 units, gear, and/or runes."
    // The objects are targets, up to four, zero allowed (CR 355.5, 355.13),
    // chosen as the spell is played, in its finalize pass, before its cost:
    // one prompt each, with "None". The text is not limited to friendly
    // objects — bare "units", "gear" and "runes" are any on the board, either
    // player's (CR 133.3.a, 355.9.a.1; judge question 1) — nor to exhausted
    // ones: at play nothing says what will be exhausted as it resolves (an
    // object exhausted in response is readied). Enemy objects that can't be
    // chosen by enemy spells are left out; an enemy [Deflect] unit is offered
    // only if its Deflect can be paid, and paid as it is chosen
    // (selectSeveralTargets). One that has left the board as it resolves is a
    // mistarget.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    static std::vector<GameObjectId> readyable(const GameState& state, PlayerId controller) {
        return boardObjects(state, [&](const GameObject& obj) {
            if (!obj.isUnit() && !obj.isGear() && !obj.isRune()) return false;
            return obj.controller == controller || !obj.untargetable_by_enemy;
        });
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto picks = selectSeveralTargets(*this, ctx,
            "Acceleration Gate: ready a unit, gear or rune (up to four)",
            readyable(ctx.state, ctx.controller), /*max_picks=*/4);
        if (!picks.has_value()) return;          // waiting for the agent
        if (finalizingChoices(ctx)) return;      // choices end here (CR 355)
        for (auto id : *picks) ctx.executor.readyObject(id);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 937;
        d.def_id = R"RB(ven-150-166)RB";
        d.name = R"RB(Acceleration Gate)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-150/166)RB";
        d.collector_number = 150;
        d.artist = R"RB(小牛设计/Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind, Domain::Body};
        d.tags = {R"RB(Jayce)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Ready up to 4 units, gear, and/or runes.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/31b71a16ee85cf30124fdaa2c575e709a4c83771-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_937(CardRegistry& r) {
    r.registerCard(937, std::make_unique<AccelerationGate>());
}

}  // namespace riftbound
