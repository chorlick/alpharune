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

class LilliaProtectorOfDreams : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When you play a token unit, give me +1 [M] this turn." A token is
    // played (CR 350.2); TriggerManager fires this from TokenCreatedEvent.
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayATokenUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;   // left the board
        ctx.executor.giveTemporaryMight(ctx.source, 1);
        ctx.events.logTrace("LILLIA, PROTECTOR OF DREAMS: token unit played -> +1 [M] this turn");
    }
    // "Your token units have [Tank]." (Granted via aura.)
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != controller) continue;
            if (!obj.isToken() || !obj.location.has_value()) continue;
            GameObject::AuraEffect ae;
            ae.keyword = Keyword::Tank;
            obj.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 620;
        d.def_id = R"RB(unl-058-219)RB";
        d.name = R"RB(Lillia, Protector of Dreams)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-058/219)RB";
        d.collector_number = 58;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Fae)RB", R"RB(Lillia)RB", R"RB(Ionia)RB"};
        d.energy_cost = 5;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When you play a token unit, give me +1 [M] this turn.
Your token units have [Tank]. (They must be assigned combat damage first.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/daa5fe15e0a1c14c8dbb8f858d8a6d444ea6096e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_620(CardRegistry& r) {
    r.registerCard(620, std::make_unique<LilliaProtectorOfDreams>());
}

} // namespace riftbound
