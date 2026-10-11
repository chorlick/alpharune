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

class OverzealousFan : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIDefend; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "When I defend, you may kill me to move an attacking unit to its
        // base." Killing me is the cost, paid as the trigger is finalized (CR
        // 383.3); the attacking unit is the controller's choice.
        // The combat's battlefield: mine while I'm there, afterwards the one
        // the attacker captured at fire time is at.
        auto combat_bf = [&]() -> std::optional<BattlefieldId> {
            const auto& self = ctx.state.getObject(ctx.source);
            if (auto bf = self.battlefieldId()) return bf;
            auto it = self.card_counters.find("__defend_attacker_id");
            if (it == self.card_counters.end()) return std::nullopt;
            const auto attacker = static_cast<GameObjectId>(it->second);
            if (!ctx.state.objectExists(attacker)) return std::nullopt;
            return ctx.state.getObject(attacker).battlefieldId();
        };
        auto attackers = [&]() {
            const auto bf = combat_bf();
            if (!bf) return std::vector<GameObjectId>{};
            const PlayerId me = ctx.controller;
            return chooseableUnits(ctx.state, me, [&](const GameObject& o) {
                return o.controller != me && o.battlefieldId() == bf &&
                       o.combat_designation == CombatDesignation::Attacker;
            });
        };
        auto still_legal = [&]() {
            return costAlreadyPaid(ctx) ||
                   (ctx.state.getObject(ctx.source).location.has_value() && !attackers().empty());
        };
        int conf = confirmOptional(ctx,
            "Overzealous Fan: kill me to move an attacking unit to its base?", still_legal);
        GameObjectId attacker = kInvalidId;
        if (conf == 1) attacker = pickTarget(ctx, "Overzealous Fan: choose an attacking unit",
                                             attackers());
        // "Kill me to ...": a death that was replaced is not a kill, so the
        // cost was not paid and nothing is moved. The cost is attempted once:
        // the failed attempt is kept on the chain item (past the pick*
        // helpers' slots 0..6) so the resolution does not try again.
        constexpr size_t kKillReplacedSlot = 7;
        auto kill_replaced = [&]() {
            const auto& ri = ctx.state.chain.resuming;
            return ri.has_value() && ri->resume_data.size() > kKillReplacedSlot &&
                   ri->resume_data[kKillReplacedSlot] == 1;
        };
        if (conf == 1 && attacker != kInvalidId && costDueNow(ctx) && !kill_replaced()) {
            // The cost can only be paid by a Fan that is still on the board.
            if (!ctx.state.getObject(ctx.source).location.has_value()) return;
            ctx.executor.killObject(ctx.source);
            if (ctx.state.getObject(ctx.source).location.has_value()) {
                if (!ctx.state.chain.resuming.has_value()) return;   // direct call: no item
                auto& data = ctx.state.chain.resuming->resume_data;
                while (data.size() <= kKillReplacedSlot) data.push_back(0);
                data[kKillReplacedSlot] = 1;
            }
        }
        if (finalizingChoices(ctx)) return;   // choices and the cost end here (CR 383.3)
        if (conf < 1 || attacker == kInvalidId) return;   // waiting / declined / mistarget
        if (kill_replaced()) return;                      // I did not die: nothing was paid
        std::string attacker_name = ctx.state.getObject(attacker).name;
        ctx.executor.moveToBase(attacker);   // to ITS base; it keeps buffs, damage and gear
        ctx.events.logTrace("OVERZEALOUS FAN: killed self, moved " + attacker_name + " to its base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 449;
        d.def_id = R"RB(sfd-128-221)RB";
        d.name = R"RB(Overzealous Fan)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-128/221)RB";
        d.collector_number = 128;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.ability_text = R"RB(When I defend, you may kill me to move an attacking unit to its base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/922d7f337fbf14f0d62d43f69ca2a1e7480dd022-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_449(CardRegistry& r) {
    r.registerCard(449, std::make_unique<OverzealousFan>());
}

} // namespace riftbound
