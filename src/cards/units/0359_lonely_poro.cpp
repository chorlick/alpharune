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

class LonelyPoro_359 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Deathknell] — If I died alone, draw 1." Alone = no OTHER friendly
    // units at the location where I died.
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    // "If I died alone": no other friendly unit was here as I died — judged
    // from the last-known information (CR 383.2.c.3.b), which for units that
    // die at the same time is taken for all of them before the first leaves.
    // The same reading as the other printing, Lonely Poro 777. (It used to
    // read the board as the ability resolved: units that died with it were
    // gone by then, and a unit that arrived since cancelled the draw. Whether
    // units that die together leave it "alone" is judge question 8,
    // docs/judge-questions-2026-10.md; testing phase D80.)
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
        d.id = 359;
        d.def_id = R"RB(sfd-036-221)RB";
        d.name = R"RB(Lonely Poro)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-036/221)RB";
        d.collector_number = 36;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Poro)RB", R"RB(Freljord)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell] — If I died alone, draw 1. (When I die, get the effect. I'm alone if there are no other friendly units here.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f5ab933035b7a6d6bb1d35c10c13f58c7fbf3398-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_359(CardRegistry& r) {
    r.registerCard(359, std::make_unique<LonelyPoro_359>());
}

} // namespace riftbound
