#include "cards/card.h"
#include "cards/card_helpers.h"
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

class MinahSwiftfoot : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // ── behaviour (Vendetta batch C) ──
    // "When I move to a battlefield, choose one — Each player discards 1. /
    //  Each player draws 1."
    TriggerType triggerType() const override { return TriggerType::WhenIMoveToFB; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        int mode = pickMode(ctx, "Minah Swiftfoot", 2,
                            {"Each player discards 1", "Each player draws 1"});
                            if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (mode < 0) return;   // waiting for the agent
        for (auto p : {ctx.controller, opponent(ctx.controller)}) {
            if (mode == 0) ctx.executor.discardCards(p, 1);
            else           ctx.executor.drawCards(p, 1);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 898;
        d.def_id = R"RB(ven-111-166)RB";
        d.name = R"RB(Minah Swiftfoot)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-111/166)RB";
        d.collector_number = 111;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I move to a battlefield, choose one —
Each player discards 1.
Each player draws 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/30b6e4a44bcadaa53168bd4ac043fbf2136f5eb3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_898(CardRegistry& r) {
    r.registerCard(898, std::make_unique<MinahSwiftfoot>());
}

}  // namespace riftbound
