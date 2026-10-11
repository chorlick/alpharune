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

class JayceManOfProgress : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    std::vector<GameObjectId> friendlyGear(CardContext& ctx) const {
        const PlayerId me = ctx.controller;
        return boardObjects(ctx.state, [me](const GameObject& o) {
            return o.isGear() && o.controller == me;
        });
    }

    // "you may kill a friendly gear" — the controller chooses whether and
    // which. "If you do, you may play a gear with Energy cost no more than [7]
    // from hand this turn, ignoring its Energy cost. (You must still pay its
    // Power cost.)" — a permission for the rest of the turn: the next gear
    // with a printed Energy cost of [7] or less played this turn pays no
    // Energy; its Power is paid as usual. A dearer gear neither gets the
    // discount nor uses the permission up.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        int conf = confirmOptional(ctx, "Jayce: kill a friendly gear?",
                                   [&]() { return !friendlyGear(ctx).empty(); });
        GameObjectId gear = kInvalidId;
        if (conf == 1) gear = pickTarget(ctx, "Jayce: choose a friendly gear to kill",
                                         friendlyGear(ctx));
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (conf < 1 || gear == kInvalidId) return;   // waiting / declined / mistarget
        ctx.executor.killObject(gear);
        ctx.events.logTrace("JAYCE: killed a friendly gear");

        PlayerState::CostModifier mod;
        mod.source = ctx.source;
        mod.energy_reduction = 7;
        mod.this_turn_only = true;
        mod.gear_only = true;
        mod.max_printed_energy_cost = 7;   // "Energy cost no more than [7]"
        mod.from_hand_only = true;         // "from hand": not a gear played from the trash
        mod.next_card_only = true;   // one gear: used up by the next gear played
        ctx.state.player(ctx.controller).cost_modifiers.push_back(mod);
        ctx.events.logTrace("JAYCE: the next gear played this turn ignores up to [7] of its Energy cost");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 406;
        d.def_id = R"RB(sfd-084-221)RB";
        d.name = R"RB(Jayce, Man of Progress)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-084/221)RB";
        d.collector_number = 84;
        d.artist = R"RB(TUTU)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Jayce)RB", R"RB(Piltover)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, you may kill a friendly gear. If you do, you may play a gear with Energy cost no more than [7] from hand this turn, ignoring its Energy cost. (You must still pay its Power cost.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b508fa4e52ebbd5f66e1fc7df28cfb70acdfe1f5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_406(CardRegistry& r) {
    r.registerCard(406, std::make_unique<JayceManOfProgress>());
}

} // namespace riftbound
