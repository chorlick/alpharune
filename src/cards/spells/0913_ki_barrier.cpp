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

class KiBarrier : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Choose a unit. Prevent the next 7 damage that would be dealt to it this
    //  turn." EffectExecutor::dealDamage consumes prevent_damage_remaining;
    //  the expiration step clears what is left.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        std::sort(legal.begin(), legal.end());
        GameObjectId picked = pickTarget(ctx, "Ki Barrier", legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto& unit = ctx.state.getObject(picked);
        unit.prevent_damage_remaining += 7;
        RIFT_TRACE(ctx.events, "KI BARRIER: prevent the next 7 damage to " + unit.name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 913;
        d.def_id = R"RB(ven-126-166)RB";
        d.name = R"RB(Ki Barrier)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-126/166)RB";
        d.collector_number = 126;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a unit. Prevent the next 7 damage that would be dealt to it this turn. (Opponents can assign it extra combat damage to kill it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/10fa1b5646489adb8ae70ff41a63ff856d8f20d1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_913(CardRegistry& r) {
    r.registerCard(913, std::make_unique<KiBarrier>());
}

}  // namespace riftbound
