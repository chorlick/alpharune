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

class GrandDuelist : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "When one of your units becomes [Mighty], you may exhaust me to channel 1
    // rune exhausted. (A unit is Mighty while it has 5+ [M].)"
    // Wired via WhenAUnitBecomesMighty (cleanup edge-detect → "became_mighty" →
    // TriggerManager fires the controller's legend zone). Cost is exhausting
    // this legend; effect channels 1 rune entering exhausted (CR 316).
    TriggerType triggerType() const override {
        return TriggerType::WhenAUnitBecomesMighty;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto still_legal = [&]() {
            return ctx.state.objectExists(ctx.source) &&
                   !ctx.state.getObject(ctx.source).is_exhausted;
        };
        int conf = confirmOptional(ctx, "Grand Duelist: exhaust me to channel 1 rune?",
                                   still_legal);
        if (conf == -1) return;   // waiting for agent
        if (conf < 1) return;     // declined / already exhausted
        ctx.executor.exhaustObject(ctx.source);
        ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
        ctx.events.logTrace("GRAND DUELIST: exhausted -> channeled 1 rune (exhausted)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 519;
        d.def_id = R"RB(sfd-205-221)RB";
        d.name = R"RB(Grand Duelist)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-205/221)RB";
        d.collector_number = 205;
        d.artist = R"RB(Pandart Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Body, Domain::Order};
        d.tags = {R"RB(Fiora)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When one of your units becomes [Mighty], you may exhaust me to channel 1 rune exhausted. (A unit is Mighty while it has 5+ [M].))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/37064aa79c13316b5dd28f0a2b054821a43f6650-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_519(CardRegistry& r) {
    r.registerCard(519, std::make_unique<GrandDuelist>());
}

} // namespace riftbound
