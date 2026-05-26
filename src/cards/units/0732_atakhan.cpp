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

class Atakhan : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // ([Ganking] engine-handled.)
    //
    // "You may kill a friendly unit as an additional cost to play me. If you
    //  do, I cost [1] less for each Energy it costs and [Y] less for each Power
    //  it costs."
    // ENGINE GAP: this is an OPTIONAL non-resource additional cost (kill a
    // friendly unit) whose payment REDUCES Atakhan's own play cost by the
    // killed unit's printed Energy/Power. The only play-cost-reduction hook is
    // selfCostReduction, which is const/read-only — it cannot make the
    // play-time kill choice nor remove the chosen unit. There is no optional
    // additional-cost path that both sacrifices a unit AND feeds its cost back
    // as a reduction. Left unimplemented; would require engine support.
    //
    // "When I attack, the defender must kill one of their units here." This IS
    // implemented below. The defender's forced choice is approximated as their
    // rational pick (lowest-Might unit they control at my battlefield), per the
    // King's Edict #237 convention.
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return;
        PlayerId defender = opponent(ctx.controller);
        GameObjectId victim = kInvalidId;
        int best_might = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != defender) continue;
            auto bf = obj.battlefieldId();
            if (!bf || *bf != *my_bf) continue;  // "here"
            if (victim == kInvalidId || obj.current_might < best_might) {
                victim = id;
                best_might = obj.current_might;
            }
        }
        if (victim != kInvalidId) {
            ctx.executor.killObject(victim);
            ctx.events.logTrace("ATAKHAN: attack -> defender kills a unit here (approx.)");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 732;
        d.def_id = R"RB(unl-170-219)RB";
        d.name = R"RB(Atakhan)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-170/219)RB";
        d.collector_number = 170;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Noxus)RB", R"RB(Demon)RB"};
        d.energy_cost = 10;
        d.power_cost = 3;
        d.might = 7;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB(You may kill a friendly unit as an additional cost to play me. If you do, I cost [1] less for each Energy it costs and [Y] less for each Power it costs.
[Ganking] (I can move from battlefield to battlefield.)
When I attack, the defender must kill one of their units here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a0b3994aad6b6c64ace14193c1363713e6b6ede2-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_732(CardRegistry& r) {
    r.registerCard(732, std::make_unique<Atakhan>());
}

} // namespace riftbound
