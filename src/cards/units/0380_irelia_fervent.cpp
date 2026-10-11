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

class IreliaFervent : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you choose OR ready me, give me +1 [M] this turn."
    // Choose: WhenYouChooseAFriendlyUnit fires on every friendly watcher with
    // the chosen unit as subject — only my own choice counts.
    // Ready: WhenIBecomeReady covers an effect readying me and my controller
    // readying me in their Awaken Phase (CR 416.3.a).
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouChooseAFriendlyUnit, TriggerType::WhenIBecomeReady};
    }
    bool triggerCondition(const GameState& /*state*/, GameObjectId source,
                          PlayerId /*controller*/, TriggerType trigger,
                          GameObjectId subject) const override {
        return trigger != TriggerType::WhenYouChooseAFriendlyUnit || subject == source;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // "give me": only while I'm still on the board.
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;
        ctx.executor.giveTemporaryMight(ctx.source, 1);
        RIFT_TRACE(ctx.events, "IRELIA FERVENT: chosen/readied -> +1 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 380;
        d.def_id = R"RB(sfd-057-221)RB";
        d.name = R"RB(Irelia, Fervent)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-057/221)RB";
        d.collector_number = 57;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Irelia)RB", R"RB(Ionia)RB"};
        d.energy_cost = 5;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
When you choose or ready me, give me +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fe05fa55781f8036f8bfc9c10bba94326a0c8cc9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_380(CardRegistry& r) {
    r.registerCard(380, std::make_unique<IreliaFervent>());
}

} // namespace riftbound
