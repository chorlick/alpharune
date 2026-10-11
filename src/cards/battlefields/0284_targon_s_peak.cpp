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

class TargonSPeak : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // Only WhenYouConquerHere is a static (event-dispatched) trigger. The
    // end-of-turn ready-up is scheduled as a DelayedAbility; when it fires it
    // re-enters this onTrigger with firing_trigger == None (delayed-ability
    // dispatch leaves fired_trigger unset), which falls through to the ready
    // branch below.
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.firing_trigger == TriggerType::WhenYouConquerHere) {
            // Schedule the ready-up for end of this turn.
            DelayedAbility da;
            da.source = ctx.source;
            da.card_def_id = cardDefId();
            da.controller = ctx.controller;
            da.trigger = TriggerType::AtEndOfTurn;
            da.expires_on_turn = ctx.state.turn.turn_number;  // clean up EOT
            ctx.state.delayed_abilities.push_back(da);
            ctx.events.logTrace("TARGON'S PEAK: scheduled ready-2-runes for end of turn");
            return;
        }
        // AtEndOfTurn fire: ready up to 2 of the controller's exhausted runes.
        // The player says which (a ready rune of one domain is not the same
        // as a ready rune of another once Power has to be paid) and how many:
        // one prompt per rune, "None" on each (CR 355.13).
        auto exhausted = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects)
                if (obj.isRune() && obj.controller == ctx.controller && obj.is_exhausted &&
                    obj.location.has_value()) out.push_back(id);
            return out;
        };
        const auto picks = selectSeveral(ctx, "Targon's Peak: ready a rune (up to 2)",
                                         exhausted(), 2);
        if (!picks.has_value()) return;       // waiting on the agent
        const std::vector<GameObjectId>& to_ready = *picks;
        for (auto id : to_ready) ctx.executor.readyObject(id);
        ctx.events.logTrace("TARGON'S PEAK: readied " + std::to_string(to_ready.size()) +
                             " runes at end of turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 284;
        d.def_id = R"RB(ogn-289-298)RB";
        d.name = R"RB(Targon's Peak)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-289/298)RB";
        d.collector_number = 289;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here, ready up to 2 runes at the end of this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6ff1b34ad231645910f670496e1662de0b545a44-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_284(CardRegistry& r) {
    r.registerCard(284, std::make_unique<TargonSPeak>());
}

} // namespace riftbound
