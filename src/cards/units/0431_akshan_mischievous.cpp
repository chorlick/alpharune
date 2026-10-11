#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/units/weaponmaster_base.h"
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

// [Weaponmaster] is the shared keyword ability (weaponmaster_base.h), on
// WhenYouPlayMe. The gear theft is a second "when you play me" ability with a
// choice of its own, so it rides the card's other play trigger (AsYouPlayMe):
// two chain items, each with its own choices.
class AkshanMischievous : public WeaponmasterUnit {
public:
    const CardDef& def() const override { return def_; }
    // "You may pay [O][O] as an additional cost to play me." Now a real
    // optional play-time cost; the gear-steal below only happens "if you paid".
    OptionalAdditionalCost optionalAdditionalCost() const override {
        return {/*valid=*/true, /*energy=*/0, /*power=*/2, Domain::Order,
                /*any_domain=*/false, /*paid_flag=*/"__akshan_paid"};
    }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::AsYouPlayMe};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (ctx.firing_trigger != TriggerType::AsYouPlayMe) {
            WeaponmasterUnit::onTrigger(ctx, targets);
            return;
        }
        // "if you paid the additional cost" — the flag set at play time.
        auto& counters = ctx.state.getObject(ctx.source).card_counters;
        auto paid = counters.find("__akshan_paid");
        if (paid == counters.end() || paid->second != 1) return;
        PlayerId me = ctx.controller;

        // "move an enemy gear to your base" — the controller chooses which.
        std::vector<GameObjectId> enemy_gear;
        for (auto& [id, obj] : ctx.state.objects)
            if (obj.isGear() && obj.controller != me && obj.location.has_value())
                enemy_gear.push_back(id);
        const GameObjectId target_gear = pickTarget(ctx, "Akshan: move an enemy gear to your base",
                                                    enemy_gear);
        if (target_gear == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;   // waiting for the agent
        // The payment covers this play only.
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        counters.erase("__akshan_paid");
        if (target_gear == kInvalidId) return;

        // "You control it until I leave the board." Reverts to the original
        // owner via GameEngine::revertLapsedControl once this Akshan is gone —
        // so nothing is taken, and nothing is detached, if I already left.
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;

        // Detach it from whatever it was equipped to (it moves zones).
        const bool is_equipment = isEquipment(ctx.state.getObject(target_gear));
        if (ctx.state.getObject(target_gear).attached_to.has_value())
            ctx.executor.unattachGear(target_gear);
        ctx.executor.takeControlUntilSourceLeaves(target_gear, me, ctx.source);
        ctx.executor.moveToBase(target_gear);
        ctx.events.logTrace("AKSHAN: stole enemy gear until I leave the board");

        // If it's an Equipment, attach it to me.
        if (is_equipment && equipEndsOnBoard(ctx.state, target_gear, ctx.source)) {
            attachFree(ctx, target_gear, ctx.source);
            ctx.events.logTrace("AKSHAN: attached stolen Equipment to me");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 431;
        d.def_id = R"RB(sfd-109-221)RB";
        d.name = R"RB(Akshan, Mischievous)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-109/221)RB";
        d.collector_number = 109;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Akshan)RB", R"RB(Shurima)RB", R"RB(Sentinel)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Weaponmaster);
        d.ability_text = R"RB([Weaponmaster]
You may pay [O][O] as an additional cost to play me.
When you play me, if you paid the additional cost, move an enemy gear to your base. You control it until I leave the board. If it's an Equipment, attach it to me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f9e79f88463c1d516b9f1b053661937676e0e1f4-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_431(CardRegistry& r) {
    r.registerCard(431, std::make_unique<AkshanMischievous>());
}

} // namespace riftbound
