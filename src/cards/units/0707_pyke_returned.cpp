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

class PykeReturned : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "Once each turn, when an enemy unit dies while I'm at a battlefield,
    // play a Gold gear token exhausted." [Hidden]/[Backline] engine-handled.
    TriggerType triggerType() const override { return TriggerType::WhenAnEnemyUnitDies; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (!self.battlefieldId().has_value()) return;  // "while I'm at a battlefield"
        auto it = self.card_counters.find("__pyke_token_turn");
        if (it != self.card_counters.end() && it->second == ctx.state.turn.turn_number)
            return;  // "once each turn"
        self.card_counters["__pyke_token_turn"] = ctx.state.turn.turn_number;
        LocationId loc = self.location.value_or(LocationId{BaseLocation{ctx.controller}});
        auto tid = ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold",
                                            0, {"Gold"}, KeywordSet{}, loc,
                                            /*enter_ready=*/false);
        if (ctx.state.objectExists(tid)) {
            auto& tok = ctx.state.getObject(tid);
            tok.card_def_id = 326;     // real Gold gear, so its [Reaction] ability dispatches
            tok.is_exhausted = true;
        }
        ctx.events.logTrace("PYKE RETURNED: enemy unit died -> Gold gear token (exhausted)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 707;
        d.def_id = R"RB(unl-145-219)RB";
        d.name = R"RB(Pyke, Returned)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-145/219)RB";
        d.collector_number = 145;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Pyke)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Backline);
        d.keywords.set(Keyword::Hidden);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Backline] (I must be assigned combat damage last.)
Once each turn, when an enemy unit dies while I'm at a battlefield, play a Gold gear token exhausted. (It has "[Reaction][>] Kill this, [E]: [Add] [A]."))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2eaa5396b1cac9b245bde5a56b87314f8ae76a38-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_707(CardRegistry& r) {
    r.registerCard(707, std::make_unique<PykeReturned>());
}

} // namespace riftbound
