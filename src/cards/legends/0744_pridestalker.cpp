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

class Pridestalker : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play a unit, give a unit +1 [M] this turn." Any unit on the
    // board — not necessarily the one played — chosen by the controller as
    // the ability is finalized (CR 383.3).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto units = chooseableUnits(ctx.state, ctx.controller,
                                     [](const GameObject&) { return true; });
        std::sort(units.begin(), units.end());
        const GameObjectId pick = pickTarget(ctx, "Pridestalker: give a unit +1 [M]", units);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (pick == kInvalidId) return;       // none / mistarget / waiting
        ctx.executor.giveTemporaryMight(pick, 1);
        RIFT_TRACE(ctx.events, "PRIDESTALKER: +1M to " +
                             ctx.state.getObject(pick).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 744;
        d.def_id = R"RB(unl-183-219)RB";
        d.name = R"RB(Pridestalker)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-183/219)RB";
        d.collector_number = 183;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Fury, Domain::Body};
        d.tags = {R"RB(Rengar)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play a unit, give a unit +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b3d3085f62aee993b9f5b80d4659a88439da83be-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_744(CardRegistry& r) {
    r.registerCard(744, std::make_unique<Pridestalker>());
}

} // namespace riftbound
