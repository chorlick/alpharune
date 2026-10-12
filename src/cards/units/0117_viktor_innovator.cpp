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

class ViktorInnovator : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play a card on an opponent's turn, play a 1 [M] Recruit unit
    // token in your base."
    // "A card": WhenYouPlayACard fires for a unit or gear card as it is played
    // and for a spell once it has resolved (CR 420.4.a) — and not for a token:
    // "Tokens are not cards, but can still be Played" (CR 350.2). This card
    // used to register for "you play a unit / a gear / a spell", which a
    // token play also announces, so its own Recruit triggered it again and
    // the chain never emptied (finding F1). Viktor himself, played on an
    // opponent's turn by an effect, is such a card too (CR 383.2.c.1).
    // "On an opponent's turn" follows the condition immediately: it is part
    // of the trigger condition (CR 383.2.a.1), judged as the card is played.
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayACard, TriggerType::WhenYouPlayMe};
    }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType trigger, GameObjectId subject) const override {
        if (state.turn.turn_player == controller) return false;   // not an opponent's turn
        const GameObjectId played = trigger == TriggerType::WhenYouPlayMe ? source : subject;
        if (played == kInvalidId || !state.objectExists(played)) return false;
        return !state.getObject(played).isToken();
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        LocationId loc{BaseLocation{ctx.controller}};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit",
                                 1, {"Recruit"}, KeywordSet{}, loc, false);
        RIFT_TRACE(ctx.events, "VIKTOR, INNOVATOR: off-turn play -> 1[M] Recruit in base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 117;
        d.def_id = R"RB(ogn-117-298)RB";
        d.name = R"RB(Viktor, Innovator)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-117/298)RB";
        d.collector_number = 117;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Viktor)RB", R"RB(Zaun)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play a card on an opponent's turn, play a 1 [M] Recruit unit token in your base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/35be0b10f13611b4f5d828f2a79001036ab57ba1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_117(CardRegistry& r) {
    r.registerCard(117, std::make_unique<ViktorInnovator>());
}

} // namespace riftbound
