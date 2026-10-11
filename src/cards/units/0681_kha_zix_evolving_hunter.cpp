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

class KhaZixEvolvingHunter : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenIConquerOrHold, TriggerType::WhenIAttack};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (ctx.firing_trigger == TriggerType::WhenIConquerOrHold) {
            if (finalizingChoices(ctx)) return;   // no choices
            gainXp(ctx, ctx.controller, 1);       // [Hunt]
            return;
        }
        // WhenIAttack — optional: spend 3 XP, deal my Might to an enemy unit here.
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        auto enemiesHere = [&]() {
            if (!my_bf) return std::vector<GameObjectId>{};
            return chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
                return u.controller != ctx.controller && u.battlefieldId() == my_bf;
            });
        };
        // The 3 XP is a cost: offered only when it can be paid, and spent as
        // the ability is finalized — it stays spent if the unit chosen is gone
        // by the time this resolves.
        auto still_legal = [&]() {
            return costAlreadyPaid(ctx) ||
                   (ctx.state.player(ctx.controller).xp >= 3 && !enemiesHere().empty());
        };
        int yes = confirmOptional(ctx, "Kha'Zix: spend 3 XP to deal my Might?", still_legal);
        GameObjectId tgt = kInvalidId;
        if (yes == 1)
            tgt = pickTarget(ctx, "Kha'Zix: deal Might to enemy here", enemiesHere());
        bool paid = costAlreadyPaid(ctx);
        if (yes == 1 && tgt != kInvalidId && costDueNow(ctx) &&
            ctx.state.player(ctx.controller).xp >= 3) {
            ctx.state.player(ctx.controller).xp -= 3;
            paid = true;
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (yes != 1 || !paid || tgt == kInvalidId) return;
        const auto& self = ctx.state.getObject(ctx.source);
        if (!self.location.has_value()) return;   // "my Might": I must still be on the board
        int might = self.current_might;
        ctx.executor.dealDamage(tgt, might, ctx.source);
        if (ctx.state.objectExists(tgt) && ctx.state.getObject(tgt).hasLethalDamage()) {
            ctx.executor.killObject(tgt);
        }
        ctx.events.logTrace("KHA'ZIX: spent 3 XP, dealt " + std::to_string(might));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 681;
        d.def_id = R"RB(unl-119-219)RB";
        d.name = R"RB(Kha'Zix, Evolving Hunter)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-119/219)RB";
        d.collector_number = 119;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Kha'Zix)RB", R"RB(The Void)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Hunt);
        d.ability_text = R"RB([Hunt] (When I conquer or hold, gain 1 XP.)
When I attack, you may spend 3 XP to deal damage equal to my Might to an enemy unit here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ac07031af5b35881d781b7a7d7b78c59a0fb2cd4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_681(CardRegistry& r) {
    r.registerCard(681, std::make_unique<KhaZixEvolvingHunter>());
}

} // namespace riftbound
