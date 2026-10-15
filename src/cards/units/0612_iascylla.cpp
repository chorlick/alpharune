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

class Iascylla : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // Only WhenIHold is declared as a static trigger. The deferred
    // "next Main Phase" move is delivered through a DelayedAbility
    // (checkDelayedAbilities fires it independently of triggerTypes()),
    // which resolves through onTrigger with firing_trigger AtStartOfMain —
    // so we MUST NOT also declare AtStartOfMain here, or the engine's
    // per-object AtStartOfMain scan would (double-)fire it every turn.
    // The delayed ability's "you may" and its enemy unit are chosen as it is
    // finalized (CR 383.3.a, 403.2; a delayed trigger's choices are made then,
    // CR 355.5.b). The hold ability has nothing to choose.
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.firing_trigger == TriggerType::WhenIHold) {
            if (finalizingChoices(ctx)) return;   // nothing to choose
            if (!ctx.state.objectExists(ctx.source)) return;
            auto& self = ctx.state.getObject(ctx.source);
            // Schedule the deferred move for the controller's next Main
            // Phase. Record which battlefield "this" is, so the deferred
            // fire pulls the enemy to the correct BF even if Iascylla moves.
            auto bf = self.battlefieldId();
            if (!bf) return;
            self.card_counters["__iascylla_pull_bf"] = static_cast<int>((*bf).raw());

            DelayedAbility da;
            da.source = ctx.source;
            da.card_def_id = cardDefId();
            da.controller = ctx.controller;
            da.trigger = TriggerType::AtStartOfMain;
            da.expires_on_turn = DelayedAbility::kUntilFired;
            ctx.state.delayed_abilities.push_back(da);
            RIFT_TRACE(ctx.events, "IASCYLLA: scheduled next-Main-Phase enemy pull");
            return;
        }

        // AtStartOfMain (deferred fire): "this battlefield" is the one stashed
        // at hold time; none if no pull was scheduled, and then there is
        // nothing to move (the "may" is not offered).
        std::optional<BattlefieldId> bf;
        if (ctx.state.objectExists(ctx.source)) {
            const auto& counters = ctx.state.getObject(ctx.source).card_counters;
            const auto it = counters.find("__iascylla_pull_bf");
            if (it != counters.end()) bf = BattlefieldId::fromRaw(it->second);
        }
        // "an enemy unit": anywhere on the board (a base included) other
        // than this battlefield, one that may be chosen and moved by me.
        const auto legal = !bf.has_value() ? std::vector<GameObjectId>{}
            : chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
                  return u.controller != ctx.controller && u.battlefieldId() != bf &&
                         !u.cant_be_moved_by_enemy;
              });
        const int conf = confirmOptional(ctx, "Iascylla: move enemy unit here?",
                                         [&legal]() { return !legal.empty(); });
        if (conf == -1) return;   // waiting for the agent
        GameObjectId picked = kInvalidId;
        if (conf == 1) {
            picked = pickTarget(ctx, "Iascylla: pick enemy", legal);
            if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7)
                return;   // waiting for the agent
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (ctx.state.objectExists(ctx.source))
            ctx.state.getObject(ctx.source).card_counters.erase("__iascylla_pull_bf");
        if (conf != 1 || picked == kInvalidId || !bf.has_value()) return;
        ctx.executor.moveToBattlefield(picked, *bf);
        RIFT_TRACE(ctx.events, "IASCYLLA: pulled " +
                             ctx.state.getObject(picked).name +
                             " to BF" + toString(*bf));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 612;
        d.def_id = R"RB(unl-050-219)RB";
        d.name = R"RB(Iascylla)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-050/219)RB";
        d.collector_number = 50;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I hold, at the start of your next Main Phase, you may move an enemy unit to this battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/debdbfa22e4e75822fe1fbf0e7ab14bccbbdf191-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_612(CardRegistry& r) {
    r.registerCard(612, std::make_unique<Iascylla>());
}

} // namespace riftbound
