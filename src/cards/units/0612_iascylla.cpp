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

class Iascylla : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // Only WhenIHold is declared as a static trigger. The deferred
    // "next Main Phase" move is delivered through a DelayedAbility
    // (checkDelayedAbilities fires it independently of triggerTypes()),
    // which resolves through onTrigger with firing_trigger == None — so
    // we MUST NOT also declare AtStartOfMain here, or the engine's
    // per-object AtStartOfMain scan would (double-)fire it every turn.
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);

        if (ctx.firing_trigger == TriggerType::WhenIHold) {
            // Schedule the deferred move for the controller's next Main
            // Phase. Record which battlefield "this" is, so the deferred
            // fire pulls the enemy to the correct BF even if Iascylla moves.
            auto bf = self.battlefieldId();
            if (!bf) return;
            self.card_counters["__iascylla_pull_bf"] = static_cast<int>(*bf);

            DelayedAbility da;
            da.source = ctx.source;
            da.card_def_id = cardDefId();
            da.controller = ctx.controller;
            da.trigger = TriggerType::AtStartOfMain;
            da.expires_on_turn = -1;  // persists past current turn until it fires
            ctx.state.delayed_abilities.push_back(da);
            ctx.events.logTrace("IASCYLLA: scheduled next-Main-Phase enemy pull");
            return;
        }

        // AtStartOfMain (deferred fire). Only act if a pull was scheduled.
        // IMPORTANT: do NOT erase the stash up front — confirmOptional /
        // pickTarget suspend resolution and the chain re-enters this onTrigger;
        // the stash must survive those re-entries. Erase only once the decision
        // definitively commits (no legal target / declined / moved).
        auto it = self.card_counters.find("__iascylla_pull_bf");
        if (it == self.card_counters.end()) return;
        BattlefieldId bf = static_cast<BattlefieldId>(it->second);

        std::vector<GameObjectId> legal;
        PlayerId opp = opponent(ctx.controller);
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.isAtBattlefield()) continue;
            legal.push_back(id);
        }
        auto still_legal = [&legal]() { return !legal.empty(); };
        if (!still_legal()) { self.card_counters.erase("__iascylla_pull_bf"); return; }
        auto conf = confirmOptional(ctx, "Iascylla: move enemy unit here?",
                                      still_legal);
        if (conf == -1) return;  // suspended for agent input — keep the stash
        if (conf < 1) { self.card_counters.erase("__iascylla_pull_bf"); return; }
        GameObjectId picked = pickTarget(ctx, "Iascylla: pick enemy", legal);
        if (picked == kInvalidId) return;  // suspended (legal non-empty) — keep stash
        self.card_counters.erase("__iascylla_pull_bf");  // committing now
        if (!ctx.state.objectExists(picked)) return;
        ctx.executor.moveToBattlefield(picked, bf);
        ctx.events.logTrace("IASCYLLA: pulled " +
                             ctx.state.getObject(picked).name +
                             " to BF" + std::to_string(bf));
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
