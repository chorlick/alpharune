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

class LonelyPoro_777 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    // (Whether units that die together leave it "alone" is judge question 10,
    // docs/judge-questions-2026-10.md; testing phase D80. Both printings read
    // it this way.)
    // "If I died alone": no other friendly unit was here as I died — judged
    // from the last-known information (CR 383.2.c.3.b), which for units that
    // die at the same time is taken for all of them before the first leaves.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& lk = asItLeft(ctx, ctx.source);
        if (!lk.valid) return;
        if (!lk.other_friendly_units_here.empty()) {
            RIFT_TRACE(ctx.events, "LONELY PORO: did not die alone -> no draw");
            return;
        }
        ctx.executor.drawCards(ctx.controller, 1);
        RIFT_TRACE(ctx.events, "LONELY PORO: died alone -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 777;
        d.def_id = R"RB(unl-221-219)RB";
        d.name = R"RB(Lonely Poro)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-221/219)RB";
        d.collector_number = 221;
        d.artist = R"RB(FOREDAWN)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Poro)RB", R"RB(Freljord)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Showcase;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell][>] If I died alone, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/82b7043ef6519a4c7e2eb7d45cb6ae91dfd749ca-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_777(CardRegistry& r) {
    r.registerCard(777, std::make_unique<LonelyPoro_777>());
}

} // namespace riftbound
