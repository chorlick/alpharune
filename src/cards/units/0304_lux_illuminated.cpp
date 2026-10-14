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

class LuxIlluminated : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When you play a spell that costs [5] or more, give me +3 [M] this turn."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayASpell; }
    // "a spell that costs [5] or more" is the trigger condition (CR 383.1):
    // the spell that was played is the trigger's subject, its cost the one
    // printed on its card. A cheaper spell puts nothing on the chain.
    bool triggerConditionOn(const GameState& state, const TriggerEvent& ev) const override {
        if (ev.subject == kInvalidId || !state.objectExists(ev.subject)) return false;
        return ev.subject_card != nullptr && ev.subject_card->energy_cost >= 5;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "give me": only while I'm still on the board.
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;
        ctx.executor.giveTemporaryMight(ctx.source, 3);
        RIFT_TRACE(ctx.events, "LUX ILLUMINATED: 5+ cost spell -> +3 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 304;
        d.def_id = R"RB(ogs-006-024)RB";
        d.name = R"RB(Lux, Illuminated)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-006/024)RB";
        d.collector_number = 6;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Lux)RB", R"RB(Demacia)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play a spell that costs [5] or more, give me +3 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a0d10edf30abb6fde21f5d386e9a7db3c1b0a098-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_304(CardRegistry& r) {
    r.registerCard(304, std::make_unique<LuxIlluminated>());
}

} // namespace riftbound
