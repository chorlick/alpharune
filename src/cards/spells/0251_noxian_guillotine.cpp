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

class NoxianGuillotine : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose a unit. Kill it the next time it takes damage this turn.
    //  [Legion] — Kill it now instead."
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty()) return;
        GameObjectId tgt = targets[0];
        // Legion = the controller has played another card this turn (this spell
        // already incremented the counter, so >= 2 means Legion is active).
        bool legion = ctx.state.player(ctx.controller).cards_played_this_turn >= 2;
        if (legion) {
            ctx.executor.killObject(tgt);          // "Kill it now instead."
            ctx.events.logTrace("NOXIAN GUILLOTINE: [Legion] kill now");
            return;
        }
        // Without Legion: a one-shot delayed ability (CR 389) watching that
        // unit until end of turn.
        DelayedAbility da;
        da.source = ctx.source;
        da.card_def_id = cardDefId();
        da.controller = ctx.controller;
        da.trigger = TriggerType::WhenAUnitTakesDamage;
        da.target_filter = tgt;
        da.expires_on_turn = ctx.state.turn.turn_number;
        ctx.state.delayed_abilities.push_back(da);
        ctx.events.logTrace("NOXIAN GUILLOTINE: kills " + ctx.state.getObject(tgt).name +
                            " the next time it takes damage this turn");
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        const GameObjectId unit = ctx.state.chain.resuming->triggering_subject;
        if (unit == kInvalidId || !ctx.state.objectExists(unit)) return;
        const auto& u = ctx.state.getObject(unit);
        if (!u.isUnit() || !u.location.has_value()) return;   // already gone
        ctx.executor.killObject(unit);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    // The base spell works without Legion: Legion only UPGRADES it to an
    // immediate kill, so playing it is not gated on Legion.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 251;
        d.def_id = R"RB(ogn-254-298)RB";
        d.name = R"RB(Noxian Guillotine)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-254/298)RB";
        d.collector_number = 254;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Order};
        d.tags = {R"RB(Darius)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Legion);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Choose a unit. Kill it the next time it takes damage this turn.
[Legion] — Kill it now instead. (Get the effect if you've played another card this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/57f646b0393b58b657cfabf66357d9fc4a34e046-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_251(CardRegistry& r) {
    r.registerCard(251, std::make_unique<NoxianGuillotine>());
}

} // namespace riftbound
