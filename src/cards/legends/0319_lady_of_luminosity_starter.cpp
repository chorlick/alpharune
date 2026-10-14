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
    // "When you play a spell that costs [5] or more, draw 1." The spell is the
    // trigger's subject (TriggerManager::onSpellResolved); its cost is the
    // Energy cost printed on its card. "that costs [5] or more" is the
    // trigger condition itself (CR 383.1): a cheaper spell puts nothing on
    // the chain (finding G9).
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayASpell; }
    bool triggerConditionOn(const GameState& state, const TriggerEvent& ev) const override {
        if (ev.subject == kInvalidId || !state.objectExists(ev.subject)) return false;
        return state.getObject(ev.subject).isSpell() && ev.subject_card != nullptr &&
               ev.subject_card->energy_cost >= 5;
    }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.drawCards(ctx.controller, 1);
        RIFT_TRACE(ctx.events, "LADY OF LUMINOSITY: played [5]+ spell -> draw 1");
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
