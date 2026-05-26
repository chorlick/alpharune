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

class LadyOfLuminosityStarter : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayASpell; }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        // Find the most-recently-played spell (back of controller's trash).
        const auto& ps = ctx.state.player(ctx.controller);
        GameObjectId spell_id = kInvalidId;
        for (auto it = ps.trash.rbegin(); it != ps.trash.rend(); ++it) {
            if (!ctx.state.objectExists(*it)) continue;
            if (ctx.state.getObject(*it).isSpell()) { spell_id = *it; break; }
        }
        if (spell_id == kInvalidId) return;
        int printed = ctx.executor.cardDB()
            .get(ctx.state.getObject(spell_id).card_def_id).energy_cost;
        if (printed < 5) {
            ctx.events.logTrace("LADY OF LUMINOSITY: spell cost " +
                                std::to_string(printed) + " < 5 — no draw");
            return;
        }
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("LADY OF LUMINOSITY: played [5]+ spell -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 319;
        d.def_id = R"RB(ogs-021-024)RB";
        d.name = R"RB(Lady of Luminosity - Starter)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-021/024)RB";
        d.collector_number = 21;
        d.artist = R"RB(Grafit Studio/Maki Planas Mata)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Mind, Domain::Order};
        d.tags = {R"RB(Lux)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play a spell that costs [5] or more, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/44885d811b70621b188d9813b2b10b5cff1b81e6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_319(CardRegistry& r) {
    r.registerCard(319, std::make_unique<LadyOfLuminosityStarter>());
}

} // namespace riftbound
