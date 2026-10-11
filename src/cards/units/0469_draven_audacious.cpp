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

class DravenAudacious : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenIWinCombat, TriggerType::WhenIDie};
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.firing_trigger == TriggerType::WhenIWinCombat) {
            if (!ctx.state.objectExists(ctx.source)) return;
            auto& self = ctx.state.getObject(ctx.source);
            int turn_id = ctx.state.turn.turn_number + 1;
            int& last = self.card_counters["draven_win_turn"];
            if (last == turn_id) return;  // already scored this turn
            last = turn_id;
            ctx.executor.scorePoints(ctx.controller, 1);   // CR 194.1.c
            RIFT_TRACE(ctx.events, "DRAVEN AUDACIOUS: first combat win -> score 1");
        } else if (ctx.firing_trigger == TriggerType::WhenIDie) {
            // "When I die in combat": read from the last-known information the
            // kill paths snapshot just before the death (CR 383.2.c.3.b) — I was
            // an attacker or defender, or at a battlefield with a combat in
            // progress. Dying at a battlefield outside combat (a spell in the
            // Main Phase, a non-combat showdown) does not count. Without a
            // snapshot nothing is known about the death, so nothing happens.
            if (!ctx.state.objectExists(ctx.source)) return;
            const auto& lk = asItLeft(ctx, ctx.source);
            if (!lk.valid) return;
            if (!lk.in_combat && lk.combat_designation == CombatDesignation::None) return;
            // "Choose an opponent. They gain 1 point." 1v1: the one opponent.
            ctx.executor.scorePoints(opponent(ctx.controller), 1);   // CR 194.1.c
            RIFT_TRACE(ctx.events, "DRAVEN AUDACIOUS: died in combat -> opponent scores 1");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 469;
        d.def_id = R"RB(sfd-148-221)RB";
        d.name = R"RB(Draven, Audacious)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-148/221)RB";
        d.collector_number = 148;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Draven)RB", R"RB(Noxus)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Epic;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
The first time I win a combat each turn, you score 1 point.
When I die in combat, choose an opponent. They score 1 point.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8247622cbe90eadada8d0660ea31d146b1207a87-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_469(CardRegistry& r) {
    r.registerCard(469, std::make_unique<DravenAudacious>());
}

} // namespace riftbound
