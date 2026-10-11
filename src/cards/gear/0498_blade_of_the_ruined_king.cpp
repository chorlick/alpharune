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

class BladeOfTheRuinedKing : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasEquipAbility() const override { return true; }

    // "[Equip] — [Y], Kill a friendly unit". All-or-nothing: both parts of
    // the cost are checked before either is paid. Which unit dies is a direct
    // agent decision (EffectExecutor::chooseObjectNow), not a default pick.
    static std::vector<GameObjectId> killable(const GameState& state, PlayerId player,
                                              GameObjectId unit) {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller != player) continue;
            if (!obj.location.has_value()) continue;
            if (id == unit) continue;  // can't kill the unit we're equipping
            out.push_back(id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
    EquipCost equipCost(const GameState&, PlayerId, GameObjectId, GameObjectId) const override {
        return {.valid = true, .power = 1, .power_domain = Domain::Order};
    }
    bool canPayEquipExtraCost(const GameState& state, PlayerId controller, GameObjectId,
                              GameObjectId unit) const override {
        return !killable(state, controller, unit).empty();
    }
    bool payEquipExtraCost(CardContext& ctx, GameObjectId unit) override {
        const GameObjectId victim =
            ctx.executor.chooseObjectNow(ctx.controller, killable(ctx.state, ctx.controller, unit));
        if (victim == kInvalidId) return false;
        ctx.executor.killObject(victim);
        return true;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 498;
        d.def_id = R"RB(sfd-178-221)RB";
        d.name = R"RB(Blade of the Ruined King)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-178/221)RB";
        d.collector_number = 178;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might_bonus = 4;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] — [Y], Kill a friendly unit (Pay the cost: Attach this to a unit you control.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/26ab126258a15afd380c313e973f7469808ce55f-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_498(CardRegistry& r) {
    r.registerCard(498, std::make_unique<BladeOfTheRuinedKing>());
}

} // namespace riftbound
