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

class AkshanMischievous : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "You may pay [O][O] as an additional cost to play me." Now a real
    // optional play-time cost; the gear-steal below only happens "if you paid".
    OptionalAdditionalCost optionalAdditionalCost() const override {
        return {/*valid=*/true, /*energy=*/0, /*power=*/2, Domain::Order,
                /*any_domain=*/false, /*paid_flag=*/"__akshan_paid"};
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // "if you paid the additional cost" — gate on the flag set at play time.
        auto& self_obj = ctx.state.getObject(ctx.source);
        if (self_obj.card_counters["__akshan_paid"] != 1) return;
        PlayerId me = ctx.controller;

        // Find an enemy gear on the board.
        GameObjectId target_gear = kInvalidId;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isGear()) continue;
            if (obj.controller == me) continue;       // must be enemy-controlled
            if (!obj.location.has_value()) continue;   // on board
            target_gear = id;
            break;
        }
        if (target_gear == kInvalidId) return;

        // Detach it from whatever it was equipped to (it moves zones).
        auto& gear = ctx.state.getObject(target_gear);
        bool is_equipment = false;
        for (auto& tag : gear.tags) {
            if (tag == "Equipment") { is_equipment = true; break; }
        }
        if (gear.attached_to.has_value()) {
            ctx.executor.unattachGear(target_gear);
        }

        // "You control it until I leave the board." Reverts to the original
        // owner via GameEngine::revertLapsedControl once this Akshan is gone.
        ctx.executor.takeControlUntilSourceLeaves(target_gear, me, ctx.source);
        ctx.executor.moveToBase(target_gear);
        ctx.events.logTrace("AKSHAN: stole enemy gear until I leave the board");

        // If it's an Equipment, attach it to me.
        if (is_equipment && ctx.state.objectExists(target_gear) &&
            ctx.state.objectExists(ctx.source)) {
            auto& g = ctx.state.getObject(target_gear);
            auto& self = ctx.state.getObject(ctx.source);
            g.attached_to = ctx.source;
            self.attachments.push_back(target_gear);
            g.location = self.location;
            g.zone = self.zone;
            g.controller = me;
            self.attachment_might_bonus += g.might_bonus;
            self.recomputeMight();
            ctx.events.emit(ObjectStateChangedEvent{target_gear, "attached"});
            ctx.events.emit(ObjectStateChangedEvent{ctx.source, "equipped"});
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
