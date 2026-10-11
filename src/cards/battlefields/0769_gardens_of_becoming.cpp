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

class GardensOfBecoming : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "Units here have '[E]: Gain 1 XP.'" Wired via the aura-granted-ability
    // pipeline: applyPassiveAura appends a GrantedAbilityRef{this, 0} to every
    // unit at this battlefield; the action generator emits an activation per
    // ref, and resolution routes back here to onActivate.
    //
    // The granted ability descriptor (cost only — bearer exhausts).
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility a;
        a.cost.exhaust = true;
        return {a};
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>&) override {
        gainXp(ctx, ctx.controller, 1);
        ctx.events.logTrace("GARDENS OF BECOMING: unit exhausted -> +1 XP");
    }
    void applyPassiveAura(GameState& state, PlayerId /*controller*/) const override {
        // Grant the ability to the units at every Gardens. The hook runs once
        // per battlefield slot holding this card (twice when both players
        // brought it), so a unit that already has this grant is skipped.
        for (const auto& b : state.battlefields) {
            if (!state.objectExists(b.card_object_id)) continue;
            if (state.getObject(b.card_object_id).card_def_id != cardDefId()) continue;
            for (auto& [id, obj] : state.objects) {
                if (!obj.isUnit()) continue;
                auto ubf = obj.battlefieldId();
                if (!ubf || *ubf != b.id) continue;
                bool has = false;
                for (const auto& g : obj.granted_abilities)
                    if (g.source_def_id == cardDefId() && g.ability_index == 0) has = true;
                if (!has) obj.granted_abilities.push_back({cardDefId(), 0});
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 769;
        d.def_id = R"RB(unl-213-219)RB";
        d.name = R"RB(Gardens of Becoming)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-213/219)RB";
        d.collector_number = 213;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Units here have "[E]: Gain 1 XP.")RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0497a44ab302ea055b6f1f0d00a36c8023ed2344-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_769(CardRegistry& r) {
    r.registerCard(769, std::make_unique<GardensOfBecoming>());
}

} // namespace riftbound
