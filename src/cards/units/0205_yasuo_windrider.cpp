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

class YasuoWindrider_205 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "The third time I move in a turn, you score 1 point."
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    // "The third time I move in a turn" is the trigger condition itself (CR
    // 383.1): judged when the move happens. Every move bumps
    // GameObject::moves_this_turn before its event is announced, so the third
    // move of the turn is the one that leaves it at 3; the first, second and
    // fourth put nothing on the chain.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return state.objectExists(source) && state.getObject(source).moves_this_turn == 3;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.scorePoints(ctx.controller, 1);   // CR 194.1.c
        RIFT_TRACE(ctx.events, "YASUO WINDRIDER: 3rd move this turn -> score 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 205;
        d.def_id = R"RB(ogn-205-298)RB";
        d.name = R"RB(Yasuo, Windrider)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-205/298)RB";
        d.collector_number = 205;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Yasuo)RB", R"RB(Ionia)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([Ganking] (I can move from battlefield to battlefield.)
The third time I move in a turn, you score 1 point.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f5ba378ce4dad16d17e001814e091d5f484f2681-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_205(CardRegistry& r) {
    r.registerCard(205, std::make_unique<YasuoWindrider_205>());
}

} // namespace riftbound
