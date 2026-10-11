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

class SuperMegaDeathRocket : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty()) {
            ctx.executor.dealDamage(targets[0], 5, ctx.source);
            const auto& unit = ctx.state.getObject(targets[0]);
            if (unit.location.has_value() && unit.hasLethalDamage())
                ctx.executor.killObject(targets[0]);
        }
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }

    // "When you conquer, you may discard 1 to return this from your trash to
    //  your hand." — a triggered ability that works while this is in its
    //  owner's trash (TriggerManager::onScore looks there for it). The "may",
    //  the card to discard and the discard itself are decided and paid when
    //  the ability is finalized.
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        auto in_trash = [&]() {
            return std::find(ps.trash.begin(), ps.trash.end(), ctx.source) != ps.trash.end();
        };
        int go = confirmOptional(ctx,
            "Super Mega Death Rocket!: discard 1 to return it to your hand?",
            [&]() { return in_trash() && (costAlreadyPaid(ctx) || !ps.hand.empty()); });
        if (go == -1) return;   // waiting for the answer
        if (go == 1 && !costAlreadyPaid(ctx)) {
            const GameObjectId discard =
                pickTarget(ctx, "Super Mega Death Rocket!: card to discard", ps.hand);
            if (discard == kInvalidId) {
                if (ctx.state.chain.resuming.has_value() &&
                    ctx.state.chain.resuming->resume_point == 7) return;   // waiting
                go = 0;   // nothing to discard
            } else if (costDueNow(ctx)) {
                ctx.executor.applyDiscard(ctx.controller, discard);
            }
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (go != 1 || !in_trash()) return;
        auto& self = ctx.state.getObject(ctx.source);
        ps.trash.erase(std::remove(ps.trash.begin(), ps.trash.end(), ctx.source), ps.trash.end());
        self.zone = ZoneType::Hand;
        self.location = std::nullopt;
        ps.hand.push_back(ctx.source);
        RIFT_TRACE(ctx.events, "SUPER MEGA DEATH ROCKET!: returned from trash to hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 250;
        d.def_id = R"RB(ogn-252-298)RB";
        d.name = R"RB(Super Mega Death Rocket!)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-252/298)RB";
        d.collector_number = 252;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Chaos};
        d.tags = {R"RB(Jinx)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Deal 5 to a unit.
When you conquer, you may discard 1 to return this from your trash to your hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/95d6fd4d7944e759cfe7ee5e208fa329b719333b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_250(CardRegistry& r) {
    r.registerCard(250, std::make_unique<SuperMegaDeathRocket>());
}

} // namespace riftbound
