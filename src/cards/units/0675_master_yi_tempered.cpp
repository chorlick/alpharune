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

class MasterYiTempered : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // [Hunt 2]
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 2;
        ctx.events.logTrace("MASTER YI: Hunt 2 -> +2 XP (now " +
                             std::to_string(ctx.state.player(ctx.controller).xp) + ")");
    }

    // [Level 6] static: [Deflect] and [Ganking] while controller has 6+ XP.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        if (state.player(controller).xp < 6) return;
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller) continue;
            GameObject::AuraEffect deflect;
            deflect.source = id;
            deflect.keyword = Keyword::Deflect;
            obj.aura_effects.push_back(deflect);
            GameObject::AuraEffect ganking;
            ganking.source = id;
            ganking.keyword = Keyword::Ganking;
            obj.aura_effects.push_back(ganking);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 675;
        d.def_id = R"RB(unl-113-219)RB";
        d.name = R"RB(Master Yi, Tempered)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-113/219)RB";
        d.collector_number = 113;
        d.artist = R"RB(HXY)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Master Yi)RB", R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.keywords.set(Keyword::Ganking);
        d.keywords.set(Keyword::Hunt);
        d.keywords.set(Keyword::Level);
        d.ability_text = R"RB([Hunt 2] (When I conquer or hold, gain 2 XP.)
[Level 6][>] I have [Deflect] and [Ganking]. (While you have 6+ XP, opponents must pay [A] to choose me with a spell or ability and I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a21feb3555825392fdb98f868840db3827ad0389-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_675(CardRegistry& r) {
    r.registerCard(675, std::make_unique<MasterYiTempered>());
}

} // namespace riftbound
