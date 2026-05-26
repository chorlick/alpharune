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

class ApprenticeSmith : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }

    // "When I move, reveal the top card of your Main Deck. If it's a gear,
    // draw it. Otherwise, recycle it."
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.main_deck.empty()) return;
        GameObjectId top = ps.main_deck.back();
        ps.main_deck.pop_back();
        if (!ctx.state.objectExists(top)) return;
        auto& obj = ctx.state.getObject(top);
        ctx.events.emit(CardRevealedEvent{
            top, obj.card_def_id, obj.owner,
            /*revealed_to_all=*/true, /*revealed_to=*/ctx.controller,
            ZoneType::MainDeck});
        if (obj.isGear()) {
            obj.zone = ZoneType::Hand;
            ps.hand.push_back(top);
            ctx.events.logTrace("APPRENTICE SMITH: revealed gear " + obj.name +
                                 " -> drew it");
        } else {
            ctx.executor.recycleCards(ctx.controller, {top});
            ctx.events.logTrace("APPRENTICE SMITH: revealed non-gear " + obj.name +
                                 " -> recycled");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 364;
        d.def_id = R"RB(sfd-041-221)RB";
        d.name = R"RB(Apprentice Smith)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-041/221)RB";
        d.collector_number = 41;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Freljord)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When I move, reveal the top card of your Main Deck. If it's a gear, draw it. Otherwise, recycle it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3998c6171ddc7a021560d9d9600c8eced9a8d628-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_364(CardRegistry& r) {
    r.registerCard(364, std::make_unique<ApprenticeSmith>());
}

} // namespace riftbound
