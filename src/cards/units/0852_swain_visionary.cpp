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

class SwainVisionary : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // [Vision] is engine-handled (TriggerManager::onCardPlayed).
    // "When I conquer, if you've played a non-token unit, a non-token gear,
    // and a spell this turn, you score 1 point."
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        if (!ps.played_nontoken_unit_this_turn || !ps.played_nontoken_gear_this_turn ||
            !ps.played_spell_this_turn) return;
        // CR 194.1.c: a point from an ability. scorePoints honours the Tianna
        // Crownguard lock and ends the game at once at the Victory Score.
        if (ctx.executor.scorePoints(ctx.controller, 1) == 0) return;
        RIFT_TRACE(ctx.events, "SWAIN, VISIONARY: unit + gear + spell this turn -> score 1 -> " +
                            std::to_string(ps.score));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 852;
        d.def_id = R"RB(ven-065-166)RB";
        d.name = R"RB(Swain, Visionary)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-065/166)RB";
        d.collector_number = 65;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Noxus)RB", R"RB(Swain)RB", R"RB(Demon)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Vision);
        d.ability_text = R"RB([Vision] (When you play me, look at the top card of your Main Deck. You may recycle it.)
When I conquer, if you've played a non-token unit, a non-token gear, and a spell this turn, you score 1 point.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/66d8ec4322d6734719451bb3e004b7d2ec71af87-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_852(CardRegistry& r) {
    r.registerCard(852, std::make_unique<SwainVisionary>());
}

}  // namespace riftbound
