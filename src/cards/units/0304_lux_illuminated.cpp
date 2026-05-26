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
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // Gate on the triggering spell's printed energy cost (>= 5). Find the
        // most-recently-played spell and read its CardDef cost.
        GameObjectId sid = findMostRecentlyPlayedSpell(ctx.state, ctx.controller);
        if (sid == kInvalidId || !ctx.state.objectExists(sid)) return;
        const auto& def = ctx.executor.cardDB().get(
            ctx.state.getObject(sid).card_def_id);
        if (def.energy_cost < 5) return;
        ctx.executor.giveTemporaryMight(ctx.source, 3);
        ctx.events.logTrace("LUX ILLUMINATED: 5+ cost spell -> +3 [M] this turn");
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
