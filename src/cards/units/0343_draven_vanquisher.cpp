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

class DravenVanquisher : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When I win a combat, play a Gold gear token exhausted.
    //  When I attack or defend, you may pay [R]. If you do, give me +2 [M]
    //  this turn."
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenIWinCombat, TriggerType::WhenIAttackOrDefend};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.firing_trigger == TriggerType::WhenIWinCombat) {
            createGoldExhausted(ctx);
            ctx.events.logTrace("DRAVEN: won combat -> Gold gear token (exhausted)");
            return;
        }
        if (ctx.firing_trigger == TriggerType::WhenIAttackOrDefend) {
            if (!ctx.state.objectExists(ctx.source)) return;
            // "you may pay [R]" — gate on a [R] (Fury) power being available
            // (a ready Fury rune at base, recyclable for power via payOnePower).
            auto base_loc = LocationId{BaseLocation{ctx.controller}};
            auto has_fury_power = [&]() -> bool {
                for (auto& [id, obj] : ctx.state.objects) {
                    if (!obj.isRune() || obj.controller != ctx.controller) continue;
                    if (obj.is_exhausted || !obj.location.has_value()) continue;
                    if (*obj.location != base_loc) continue;
                    for (auto d : obj.domains) if (d == Domain::Fury) return true;
                }
                return false;
            };
            int conf = confirmOptional(ctx, "Draven: pay [R] for +2 [M]?",
                                       has_fury_power);
            if (conf == -1) return;  // waiting for agent
            if (conf == 0) return;   // declined / can't pay
            if (!payOnePower(ctx, ctx.controller, Domain::Fury)) return;
            ctx.executor.giveTemporaryMight(ctx.source, 2);
            ctx.events.logTrace("DRAVEN: paid [R] -> +2 [M] this turn");
            return;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 343;
        d.def_id = R"RB(sfd-020-221)RB";
        d.name = R"RB(Draven, Vanquisher)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-020/221)RB";
        d.collector_number = 20;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Draven)RB", R"RB(Noxus)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I win a combat, play a Gold gear token exhausted.
When I attack or defend, you may pay [R]. If you do, give me +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8e49e0b83df68f37a15bf704b83475144fd9ef92-744x1039.png)RB";
        d.banned = true;  // tournament ban (formerly cards/ban-list.csv)
        return d;
    }();
};

}  // anonymous namespace

void register_card_343(CardRegistry& r) {
    r.registerCard(343, std::make_unique<DravenVanquisher>());
}

} // namespace riftbound
