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

class SinisterPoro : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When I attack, you may pay [1] to move an enemy unit here to its base."
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }

    std::vector<GameObjectId> enemiesHere(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        if (!ctx.state.objectExists(ctx.source)) return out;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return out;
        PlayerId opp = opponent(ctx.controller);
        for (auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller != opp || !obj.location.has_value()) continue;
            if (obj.untargetable_by_enemy) continue;
            auto bf = obj.battlefieldId();
            if (bf && *bf == *my_bf) out.push_back(id);
        }
        return out;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // CR 445.2 / 164.2.a: the [1] may come from floating Energy or from
        // exhausting a ready rune (payableEnergy / payEnergy).
        auto still_legal = [&]() {
            return payableEnergy(ctx.state, ctx.controller) >= 1 && !enemiesHere(ctx).empty();
        };
        int conf = confirmOptional(ctx, "Sinister Poro: pay [1] to move an enemy unit here?",
                                   still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined / can't
        GameObjectId tgt = pickTarget(ctx, "Sinister Poro: choose an enemy unit here",
                                      enemiesHere(ctx));
        // pickTarget returns kInvalidId while it waits on the agent and this
        // trigger is re-entered afterwards (confirmOptional then answers 1
        // again), so the [1] is paid only here, once the target is known —
        // paying before pickTarget charged it on every entry.
        if (tgt == kInvalidId || !ctx.state.objectExists(tgt)) return;
        if (costDueNow(ctx) && !payEnergy(ctx, ctx.controller, 1)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        ctx.executor.moveToBase(tgt);
        RIFT_TRACE(ctx.events, "SINISTER PORO: paid [1] -> moved an enemy unit to its base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 699;
        d.def_id = R"RB(unl-137-219)RB";
        d.name = R"RB(Sinister Poro)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-137/219)RB";
        d.collector_number = 137;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Poro)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.might = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When I attack, you may pay [1] to move an enemy unit here to its base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7f2a623a83556bafebfbe7cb280bdedcfe116531-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_699(CardRegistry& r) {
    r.registerCard(699, std::make_unique<SinisterPoro>());
}

} // namespace riftbound
