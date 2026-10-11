#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class ScuttleCrab : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIDie};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.firing_trigger == TriggerType::WhenYouPlayMe) {
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("SCUTTLE CRAB: play — draw 1");
            return;
        }
        if (ctx.firing_trigger == TriggerType::WhenIDie) {
            // [Deathknell] — choose an opponent (only one in 1v1): they reveal
            // their hand, and I may look at their facedown cards this turn.
            auto opp = opponent(ctx.controller);
            auto show = [&](GameObjectId card_id, ZoneType zone, deck::Place place) {
                if (!ctx.state.objectExists(card_id)) return;
                auto& obj = ctx.state.getObject(card_id);
                ctx.events.emit(CardRevealedEvent{
                    card_id, obj.card_def_id, obj.owner,
                    /*revealed_to_all=*/false, /*revealed_to=*/ctx.controller, zone});
                knowledge::seen(ctx.state, card_id, place, knowledge::only(ctx.controller));
            };
            for (auto card_id : ctx.state.player(opp).hand)
                show(card_id, ZoneType::Hand, deck::Place::Hand);
            // ENGINE-GAP: "this turn" covers cards the opponent hides later in
            // the turn too; there is no standing look-at permission, so only
            // the cards facedown now are shown.
            for (const auto& bf : ctx.state.battlefields)
                for (auto card_id : bf.facedown)
                    if (ctx.state.objectExists(card_id) &&
                        ctx.state.getObject(card_id).controller == opp)
                        show(card_id, ZoneType::FacedownZone, deck::Place::Facedown);
            gainXp(ctx, ctx.controller, 1);
            ctx.events.logTrace("SCUTTLE CRAB: [Deathknell] reveal opp hand + facedown cards, +1 XP");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 615;
        d.def_id = R"RB(unl-053-219)RB";
        d.name = R"RB(Scuttle Crab)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-053/219)RB";
        d.collector_number = 53;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB((Units with 0 [M] can conquer and hold.)
When you play me, draw 1.
[Deathknell][>] Choose an opponent. They reveal their hand. You can look at their facedown cards this turn. Gain 1 XP. (When I die, get the effects.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8c9a251b2deb21716fba0ac83c6aac96563fa19d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_615(CardRegistry& r) {
    r.registerCard(615, std::make_unique<ScuttleCrab>());
}

} // namespace riftbound
