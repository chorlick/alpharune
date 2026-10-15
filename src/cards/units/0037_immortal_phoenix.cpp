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

class ImmortalPhoenix : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When you kill a unit with a spell, you may pay [1][R] to play me from
    //  your trash." Wired via WhenYouKillAUnitWithASpell (dispatched by
    //  TriggerManager::onUnitDied to the resolving spell controller's cards incl.
    //  trash). We pay [1][R] (1 energy + 1 Fury power) and play self from trash.
    TriggerType triggerType() const override {
        return TriggerType::WhenYouKillAUnitWithASpell;
    }
    // "play me from your trash": the ability works from its owner's trash
    // (CR 383.2.c.1 and its Immortal Phoenix example). The dispatch also
    // reaches the killer's cards on the board; a Phoenix there does not
    // trigger (testing phase B3).
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return state.objectExists(source) && state.getObject(source).zone == ZoneType::Trash;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto in_trash = [&]() { return ctx.state.getObject(ctx.source).zone == ZoneType::Trash; };
        // [1][R]: Energy from the pool or a ready rune, one Fury Power from the
        // pool or a recycled Fury rune (CR 164.2). One ready Fury rune covers
        // both: exhausted for the Energy, then recycled for the Power.
        auto can_pay = [&]() {
            return in_trash() && canPayEnergy(ctx, ctx.controller, 1) &&
                   payableOnePower(ctx.state, ctx.controller, Domain::Fury);
        };
        // No return ahead of the "may": with [1][R] unpayable confirmOptional
        // stores the "no" on the chain item, so the resolution asks nothing
        // if it has become payable since (CR 383.3.a).
        int conf = confirmOptional(ctx, "Immortal Phoenix: pay [1][R] to play from trash?",
                                   can_pay);
        if (conf < 1) return;     // waiting / declined / unaffordable
        // To my base or a battlefield I control (CR 355.2.a).
        const int where = pickEffectPlayLocation(*this, ctx, "Immortal Phoenix: play me to");
        if (where == -1) return;  // waiting for the agent
        // Energy first: the rune exhausted for it can still be recycled for [R].
        if (costDueNow(ctx) && !(payEnergy(ctx, ctx.controller, 1) &&
                                 payOnePower(ctx, ctx.controller, Domain::Fury))) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (!in_trash()) return;              // left the trash in response
        ctx.executor.playIgnoringCost(ctx.controller, ctx.source,
                                      effectPlayLocation(ctx.state, ctx.controller, where));
        RIFT_TRACE(ctx.events, "IMMORTAL PHOENIX: paid [1][R] -> played from trash");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 37;
        d.def_id = R"RB(ogn-037-298)RB";
        d.name = R"RB(Immortal Phoenix)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-037/298)RB";
        d.collector_number = 37;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Spirit)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Epic;
        d.assault_value = 2;
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB([Assault 2] (+2 [M] while I'm an attacker.)
When you kill a unit with a spell, you may pay [1][R] to play me from your trash.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7b623ae985bf5f362b6d8d4a17e9b8146aeae3c3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_37(CardRegistry& r) {
    r.registerCard(37, std::make_unique<ImmortalPhoenix>());
}

} // namespace riftbound
