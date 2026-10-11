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

class MasterYiUnstoppable : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // Tiered by the controller's XP; each tier replaces the one below it:
    //   [Level 3]  I cost [2][G] less.
    //   [Level 6]  I cost [4][G][G] less instead.
    //   [Level 11] I cost [6][G][G][G] less instead.
    static int tier(const GameState& state, PlayerId player) {
        const int xp = state.player(player).xp;
        return xp >= 11 ? 3 : xp >= 6 ? 2 : xp >= 3 ? 1 : 0;
    }
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        return 2 * tier(state, player);
    }
    int selfPowerCostReduction(const GameState& state, PlayerId player) const override {
        return tier(state, player);
    }
    // "[Level 16][>] I can't be chosen by enemy spells and abilities."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        if (state.player(controller).xp < 16) return;
        for (auto& [id, obj] : state.objects)
            if (obj.card_def_id == cardDefId() && obj.controller == controller &&
                obj.location.has_value())
                obj.untargetable_by_enemy = true;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 621;
        d.def_id = R"RB(unl-059-219)RB";
        d.name = R"RB(Master Yi, Unstoppable)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-059/219)RB";
        d.collector_number = 59;
        d.artist = R"RB(Ziyan Lin)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Master Yi)RB", R"RB(Ionia)RB"};
        d.energy_cost = 12;
        d.power_cost = 3;
        d.might = 12;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Level);
        d.ability_text = R"RB([Level 3][>] I cost [2][G] less. (While you have 3+ XP, get the effect.)
[Level 6][>] I cost [4][G][G] less instead.
[Level 11][>] I cost [6][G][G][G] less instead.
[Level 16][>] I can't be chosen by enemy spells and abilities.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8a6d46d701eebaff0850a5b7400816517006d7e9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_621(CardRegistry& r) {
    r.registerCard(621, std::make_unique<MasterYiUnstoppable>());
}

} // namespace riftbound
