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

class PoppyParagon : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Deflect]" engine-handled. "When you play me, if an opponent's score is
    //  within 3 points of the Victory Score, ready me and gain 3 XP."
    static bool oppNearVictory(const GameState& state, PlayerId controller) {
        int vs = state.mode.victory_score;
        return state.player(opponent(controller)).score >= vs - 3;
    }
    // "ready me" — enter ready when the condition holds at play time.
    bool entersReadyOnPlay(const GameState& state, PlayerId controller) const override {
        return oppNearVictory(state, controller);
    }
    // "gain 3 XP" — gated on the same condition.
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!oppNearVictory(ctx.state, ctx.controller)) return;
        ctx.state.player(ctx.controller).xp += 3;
        ctx.events.logTrace("POPPY PARAGON: opponent near Victory Score -> ready + gain 3 XP");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 678;
        d.def_id = R"RB(unl-116-219)RB";
        d.name = R"RB(Poppy, Paragon)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-116/219)RB";
        d.collector_number = 116;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Yordle)RB", R"RB(Demacia)RB", R"RB(Poppy)RB"};
        d.energy_cost = 5;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
When you play me, if an opponent's score is within 3 points of the Victory Score, ready me and gain 3 XP.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ef8a8434e1a4c4262dd97213741a4b126cb1449b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_678(CardRegistry& r) {
    r.registerCard(678, std::make_unique<PoppyParagon>());
}

} // namespace riftbound
