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

class VayneHunter : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "If an opponent controls a battlefield, I enter ready."
    bool entersReadyOnPlay(const GameState& state,
                           PlayerId controller) const override {
        PlayerId opp = opponent(controller);
        for (const auto& bf : state.battlefields)
            if (bf.controller.has_value() && *bf.controller == opp) return true;
        return false;
    }

    // "When I conquer, you may pay [1] to return me to my owner's hand."
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& ps = ctx.state.player(ctx.controller);
        auto still_legal = [&]() {
            return ps.rune_pool.energy >= 1 && ctx.state.objectExists(ctx.source);
        };
        if (!still_legal()) return;
        int conf = confirmOptional(ctx, "Vayne: pay [1] to return me to hand?",
                                   still_legal);
        if (conf < 1) return;
        ps.rune_pool.energy -= 1;
        ctx.executor.bounceToHand(ctx.source);
        ctx.events.logTrace("VAYNE: paid [1] -> returned to owner's hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 35;
        d.def_id = R"RB(ogn-035-298)RB";
        d.name = R"RB(Vayne, Hunter)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-035/298)RB";
        d.collector_number = 35;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Vayne)RB", R"RB(Demacia)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 2;
        d.rarity = Rarity::Rare;
        d.assault_value = 3;
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB([Assault 3] (+3 [M] while I'm an attacker.)
If an opponent controls a battlefield, I enter ready.
When I conquer, you may pay [1] to return me to my owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b38a22e6f4bfaa8589f32754a7466fe5214e81b5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_35(CardRegistry& r) {
    r.registerCard(35, std::make_unique<VayneHunter>());
}

} // namespace riftbound
