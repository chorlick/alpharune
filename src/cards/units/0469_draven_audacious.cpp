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
            ctx.state.player(ctx.controller).score++;
            ctx.events.logTrace("DRAVEN AUDACIOUS: first combat win -> score 1");
        } else if (ctx.firing_trigger == TriggerType::WhenIDie) {
            // "When I die in combat" — approximate "in combat" by requiring the
            // death to have happened at a battlefield (last_location).
            if (ctx.state.objectExists(ctx.source)) {
                const auto& self = ctx.state.getObject(ctx.source);
                bool died_at_bf = self.last_location.has_value() &&
                    std::holds_alternative<BattlefieldLocation>(*self.last_location);
                if (!died_at_bf) {
                    // Not clearly a combat death — skip to honor "in combat".
                    return;
                }
            }
            PlayerId opp = opponent(ctx.controller);
            ctx.state.player(opp).score++;
            ctx.events.logTrace("DRAVEN AUDACIOUS: died in combat -> opponent scores 1");
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
