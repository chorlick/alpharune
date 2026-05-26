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

class GemhandHunter : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // Hunt: gain 1 XP when I conquer or hold.
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 1;
        ctx.events.logTrace("GEMHAND HUNTER: Hunt — gain 1 XP");
    }

    // Level 6: continuous +1 [M] while controller has 6+ XP.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        if (state.player(controller).xp < 6) return;
        // Find every on-board instance of this card controlled by `controller`
        // and push a +1 might aura effect onto it.
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            GameObject::AuraEffect ae;
            ae.source = id;
            ae.might_bonus = 1;
            obj.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 656;
        d.def_id = R"RB(unl-094-219)RB";
        d.name = R"RB(Gemhand Hunter)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-094/219)RB";
        d.collector_number = 94;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Yordle)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.keywords.set(Keyword::Hunt);
        d.keywords.set(Keyword::Level);
        d.ability_text = R"RB([Hunt] (When I conquer or hold, gain 1 XP.)
[Level 6][>] I have +1 [M]. (While you have 6+ XP, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d824d666dc5f50c3c513c3f9722f6eafe21c6289-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_656(CardRegistry& r) {
    r.registerCard(656, std::make_unique<GemhandHunter>());
}

} // namespace riftbound
