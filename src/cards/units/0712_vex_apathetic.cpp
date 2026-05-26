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

class VexApathetic : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Deflect] ... When an opponent plays a unit while I'm at a battlefield,
    // [Stun] it." [Deflect] is engine-handled. The played-unit id is captured
    // by TriggerManager into card_counters["__opp_played_unit_id"].
    // TODO: "They can't move it this turn / it doesn't deal combat damage this
    // turn" — needs per-unit can't-move / no-combat-damage flags (not modeled).
    TriggerType triggerType() const override { return TriggerType::WhenOpponentPlaysAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (!self.battlefieldId().has_value()) return;  // "while I'm at a battlefield"
        auto it = self.card_counters.find("__opp_played_unit_id");
        if (it == self.card_counters.end()) return;
        GameObjectId victim = static_cast<GameObjectId>(it->second);
        if (!ctx.state.objectExists(victim)) return;
        ctx.executor.stunUnitBy(victim, ctx.source);
        ctx.events.logTrace("VEX APATHETIC: opponent played a unit -> stun it");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 712;
        d.def_id = R"RB(unl-150-219)RB";
        d.name = R"RB(Vex, Apathetic)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-150/219)RB";
        d.collector_number = 150;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Yordle)RB", R"RB(Vex)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
When an opponent plays a unit while I'm at a battlefield, [Stun] it. They can't move it this turn. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7f13b0216c5d805400bb64cf26cd119acfc6c5ca-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_712(CardRegistry& r) {
    r.registerCard(712, std::make_unique<VexApathetic>());
}

} // namespace riftbound
