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

class ImperialDecree : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "When any unit takes damage this turn, kill it." — a delayed triggered
    // ability (CR 389) that lasts until end of turn and fires for every unit
    // that is dealt damage from now on. Damage marked before this resolved
    // does not count.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        DelayedAbility da;
        da.source = ctx.source;
        da.card_def_id = cardDefId();
        da.controller = ctx.controller;
        da.trigger = TriggerType::WhenAUnitTakesDamage;
        da.expires_on_turn = ctx.state.turn.turn_number;
        da.repeats = true;
        ctx.state.delayed_abilities.push_back(da);
        RIFT_TRACE(ctx.events, "IMPERIAL DECREE: units that take damage this turn are killed");
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        const GameObjectId unit = ctx.state.chain.resuming->triggering_subject;
        if (unit == kInvalidId || !ctx.state.objectExists(unit)) return;
        const auto& u = ctx.state.getObject(unit);
        if (!u.isUnit() || !u.location.has_value()) return;   // already gone
        ctx.executor.killObject(unit);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 221;
        d.def_id = R"RB(ogn-221-298)RB";
        d.name = R"RB(Imperial Decree)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-221/298)RB";
        d.collector_number = 221;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 5;
        d.power_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
When any unit takes damage this turn, kill it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fc6ab617f1ce20c82ed76dc6446d42d416c08b34-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_221(CardRegistry& r) {
    r.registerCard(221, std::make_unique<ImperialDecree>());
}

} // namespace riftbound
