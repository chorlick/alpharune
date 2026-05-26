#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class Mosstomper : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // [Hunt 2]
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 2;
        ctx.events.logTrace("MOSSTOMPER: Hunt 2 -> +2 XP (now " +
                             std::to_string(ctx.state.player(ctx.controller).xp) + ")");
    }

    // [Level 3] static: +1 [M] and [Deflect] while controller has 3+ XP.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        if (state.player(controller).xp < 3) return;
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller) continue;
            GameObject::AuraEffect might;
            might.source = id;
            might.might_bonus = 1;
            obj.aura_effects.push_back(might);
            GameObject::AuraEffect deflect;
            deflect.source = id;
            deflect.keyword = Keyword::Deflect;
            obj.aura_effects.push_back(deflect);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 609;
        d.def_id = R"RB(unl-047-219)RB";
        d.name = R"RB(Mosstomper)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-047/219)RB";
        d.collector_number = 47;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Dog)RB", R"RB(Ixtal)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.keywords.set(Keyword::Hunt);
        d.keywords.set(Keyword::Level);
        d.ability_text = R"RB([Hunt 2] (When I conquer or hold, gain 2 XP.)
[Level 3][>] I have +1 [M] and [Deflect]. (While you have 3+ XP, get the effect. Opponents must pay [A] to choose a [Deflect] unit with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0ea32d0f6f49c0b48b1ea2cc98fe0a3c540bc9db-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_609(CardRegistry& r) {
    r.registerCard(609, std::make_unique<Mosstomper>());
}

} // namespace riftbound
