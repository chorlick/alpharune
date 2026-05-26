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

class ForgottenLibrary : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayASpell; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // CR gate "While you control this battlefield" — fire only if the
        // BF object's controller (set by ctx.controller via fireTrigger)
        // matches the spell-player. Read from per-trigger snapshot
        // (Phase 6q+) so a subsequent spell-play doesn't clobber the
        // value we need. Falls back to PlayerState for legacy paths.
        int spent = ctx.state.chain.resuming.has_value()
            ? ctx.state.chain.resuming->triggering_spell_energy_spent
            : 0;
        if (spent == 0) {
            spent = ctx.state.player(ctx.controller).last_spell_energy_spent;
        }
        if (spent < 4) {
            ctx.events.logTrace("FORGOTTEN LIBRARY: skip (spent=" +
                                 std::to_string(spent) + " < 4)");
            return;
        }
        ctx.events.logTrace("FORGOTTEN LIBRARY: predict 1");
        ctx.executor.predict(ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 767;
        d.def_id = R"RB(unl-211-219)RB";
        d.name = R"RB(Forgotten Library)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-211/219)RB";
        d.collector_number = 211;
        d.artist = R"RB(Chris Kintner)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Predict);
        d.ability_text = R"RB(While you control this battlefield, when you play a spell, if you spent [4] or more, [Predict]. (Look at the top card of your Main Deck. You may recycle it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a7905fb94abe453f05a4108cf5424fd1ca50f6de-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_767(CardRegistry& r) {
    r.registerCard(767, std::make_unique<ForgottenLibrary>());
}

} // namespace riftbound
