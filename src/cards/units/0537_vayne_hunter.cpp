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

class VayneHunter_537 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    bool entersReadyOnPlay(const GameState& state, PlayerId controller) const override {
        PlayerId opp = opponent(controller);
        for (auto& bf : state.battlefields) {
            if (bf.controller.has_value() && *bf.controller == opp) return true;
        }
        return false;
    }

    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto on_board = [&ctx]() {
            return ctx.state.objectExists(ctx.source) &&
                   ctx.state.getObject(ctx.source).location.has_value();
        };
        // CR 445.2 / 164.2.a: the [1] may come from floating Energy or from
        // exhausting a ready rune (payableEnergy / payEnergy).
        int conf = confirmOptional(ctx, "Vayne, Hunter: pay [1] to return me to hand?",
                                   [&]() {
            return costAlreadyPaid(ctx) ||
                   (on_board() && payableEnergy(ctx.state, ctx.controller) >= 1);
        });
        if (conf != 1) return;
        if (costDueNow(ctx) && !payEnergy(ctx, ctx.controller, 1)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        ctx.executor.bounceToHand(ctx.source);
        RIFT_TRACE(ctx.events, "VAYNE, HUNTER: conquer -> paid [1] to return to hand");
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
        // Tags as printed on SFD-223 (cards/raw/gallery_raw.json): the Spiritforged
        // reprint is not a copy of the Origins def — OGN-035 (card 35) prints Vayne, Demacia without Sentinel.
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
    r.registerCard(537, std::make_unique<VayneHunter_537>());
}

} // namespace riftbound
