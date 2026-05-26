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

class Gustwalker : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Hunt 2] — gain 2 XP on conquer/hold (unconditional).
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.state.player(ctx.controller).xp += 2;
        ctx.events.logTrace("GUSTWALKER: Hunt 2 -> +2 XP");
    }
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    bool requiresLevel() const override { return true; }
    int levelThreshold() const override { return 3; }
    // "[Level 3] I have +1 [M] and [Ganking]." Level hooks are not consumed by
    // the engine; implement the tier inline by checking controller XP.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        if (state.player(controller).xp < 3) return;
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId() || obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            GameObject::AuraEffect might;
            might.source = id;
            might.might_bonus = 1;
            obj.aura_effects.push_back(might);
            GameObject::AuraEffect ganking;
            ganking.source = id;
            ganking.keyword = Keyword::Ganking;
            obj.aura_effects.push_back(ganking);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 637;
        d.def_id = R"RB(unl-075-219)RB";
        d.name = R"RB(Gustwalker)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-075/219)RB";
        d.collector_number = 75;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Dog)RB", R"RB(Ionia)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Ganking);
        d.keywords.set(Keyword::Hunt);
        d.keywords.set(Keyword::Level);
        d.ability_text = R"RB([Hunt 2] (When I conquer or hold, gain 2 XP.)
[Level 3][>] I have +1 [M] and [Ganking]. (While you have 3+ XP, get the effect. A [Ganking] unit can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3f314cea0c05d2ea274a81d723299e4e20b2ebde-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_637(CardRegistry& r) {
    r.registerCard(637, std::make_unique<Gustwalker>());
}

} // namespace riftbound
