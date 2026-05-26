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

// "[Assault 3] If an opponent controls a battlefield, I enter ready. When I
//  conquer, you may pay [1] to return me to my owner's hand."

class VayneHunter : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    bool entersReadyOnPlay(const GameState& state, PlayerId controller) const override {
        PlayerId opp = opponent(controller);
        for (auto& bf : state.battlefields) {
            if (bf.controller.has_value() && *bf.controller == opp) return true;
        }
        return false;
    }

    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& ps = ctx.state.player(ctx.controller);
        int conf = confirmOptional(ctx, "Vayne, Hunter: pay [1] to return me to hand?",
                                   [&]() {
            return ctx.state.objectExists(ctx.source) && ps.rune_pool.energy >= 1;
        });
        if (conf != 1) return;
        if (ps.rune_pool.energy < 1) return;
        ps.rune_pool.energy -= 1;
        ctx.executor.bounceToHand(ctx.source);
        ctx.events.logTrace("VAYNE, HUNTER: conquer -> paid [1] to return to hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 537;
        d.def_id = R"RB(sfd-223-221)RB";
        d.name = R"RB(Vayne, Hunter)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-223/221)RB";
        d.collector_number = 223;
        d.artist = R"RB(John Kafka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Vayne)RB", R"RB(Demacia)RB", R"RB(Sentinel)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 2;
        d.rarity = Rarity::Showcase;
        d.assault_value = 3;
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB([Assault 3] (+3 [M] while I'm an attacker.)
If an opponent controls a battlefield, I enter ready.
When I conquer, you may pay [1] to return me to my owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6f8caecc98cb5dae1b818677ef5baaaa4a5f7cb2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_537(CardRegistry& r) {
    r.registerCard(537, std::make_unique<VayneHunter>());
}

} // namespace riftbound
