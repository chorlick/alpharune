#include "cards/card.h"
#include "cards/card_helpers.h"
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

class DragonForm : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Choose a unit. Its base Might becomes 5 this turn."
    // Modelled as a this-turn Might delta of (5 - base Might): every other
    // modifier (buffs, gear, auras, other temporary Might) still stacks on
    // top, exactly as it would on a base of 5, and it expires with the other
    // "this turn" Might at the expiration step. GameObject::base_might itself
    // is left untouched because nothing would restore it at end of turn.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        std::sort(legal.begin(), legal.end());
        GameObjectId picked = pickTarget(ctx, "Dragon Form", legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto& unit = ctx.state.getObject(picked);
        unit.temp_might_bonus += 5 - unit.base_might;
        unit.recomputeMight();
        RIFT_TRACE(ctx.events, "DRAGON FORM: " + unit.name + " base Might is 5 this turn -> " +
                            std::to_string(unit.current_might) + "M");
    }
    // "[Flow] [3]"
    FlowCost flowCost() const override { return flow(3); }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 903;
        d.def_id = R"RB(ven-116-166)RB";
        d.name = R"RB(Dragon Form)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-116/166)RB";
        d.collector_number = 116;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Flow);
        d.ability_text = R"RB(Choose a unit. Its base Might becomes 5 this turn.
[Flow] [3] (You may play this from your trash for its Flow cost. Then banish it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ded9b54d93d7c3ade8a1d65c8ac66a13d1246329-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_903(CardRegistry& r) {
    r.registerCard(903, std::make_unique<DragonForm>());
}

}  // namespace riftbound
