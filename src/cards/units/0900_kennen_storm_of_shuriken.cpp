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

class KennenStormOfShuriken : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // ── behaviour (Vendetta batch C) ──
    // "When you play me, [Burn 2]."
    // "When I conquer, give a spell in your trash [Flow] equal to its cost
    //  this turn."
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIConquer};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.firing_trigger == TriggerType::WhenYouPlayMe) {
            ctx.executor.burn(ctx.controller, 2);
            return;
        }
        if (ctx.firing_trigger != TriggerType::WhenIConquer) return;
        std::vector<GameObjectId> spells;
        for (auto cid : ctx.state.player(ctx.controller).trash) {
            if (!ctx.state.objectExists(cid)) continue;
            const auto& obj = ctx.state.getObject(cid);
            if (obj.isSpell() && obj.card_def_id != kInvalidId) spells.push_back(cid);
        }
        GameObjectId picked = pickTarget(ctx, "Kennen, Storm of Shuriken: grant [Flow]", spells);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        // The engine's Flow generator reads this stamp: a spell in trash whose
        // "__flow_granted_turn" equals the current turn may be played for its
        // own printed cost, then banished.
        ctx.state.getObject(picked).card_counters["__flow_granted_turn"] =
            ctx.state.turn.turn_number;
        RIFT_TRACE(ctx.events, "KENNEN, STORM OF SHURIKEN: " + ctx.state.getObject(picked).name +
                            " has [Flow] equal to its cost this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 900;
        d.def_id = R"RB(ven-113-166)RB";
        d.name = R"RB(Kennen, Storm of Shuriken)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-113/166)RB";
        d.collector_number = 113;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Yordle)RB", R"RB(Kennen)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When you play me, [Burn 2]. (Put the top 2 cards of your Main Deck into your trash.)
When I conquer, give a spell in your trash [Flow] equal to its cost this turn. (You may play it from your trash for its Flow cost. Then banish it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2f27c99f3df2940940dddd65105e5e42bbeeab3d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_900(CardRegistry& r) {
    r.registerCard(900, std::make_unique<KennenStormOfShuriken>());
}

}  // namespace riftbound
