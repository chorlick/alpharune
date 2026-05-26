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

class SoulSword : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        // [G] equip: recycle a Calm rune for power.
        return standardEquip(ctx, ctx.source, unit, Domain::Calm);
    }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // Find this gear's on-board instance(s); if attached and the
        // controller has >= 3 XP, push +1 might onto the equipped unit.
        if (state.player(controller).xp < 3) return;
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller) continue;
            if (!obj.attached_to.has_value()) continue;
            auto unit_id = *obj.attached_to;
            auto uit = state.objects.find(unit_id);
            if (uit == state.objects.end()) continue;
            uit->second.aura_effects.push_back(
                GameObject::AuraEffect{.source = id, .might_bonus = 1});
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 601;
        d.def_id = R"RB(unl-039-219)RB";
        d.name = R"RB(Soul Sword)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-039/219)RB";
        d.collector_number = 39;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 1;
        d.might_bonus = 1;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [G] ([G]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB([Level 3][>] I have an additional +1 [M]. (While you have 3+ XP, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/519361230cb8554cdf0f5dd795e115ffbb5bb932-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_601(CardRegistry& r) {
    r.registerCard(601, std::make_unique<SoulSword>());
}

} // namespace riftbound
