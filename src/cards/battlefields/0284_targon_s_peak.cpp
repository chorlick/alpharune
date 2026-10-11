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
    // The runes are the delayed trigger's choice: they are named when IT is
    // finalized at the end of the turn, not when the conquer ability is (CR
    // 355.5.b, whose example is this card).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.firing_trigger == TriggerType::WhenYouConquerHere) {
            if (finalizingChoices(ctx)) return;   // the conquer ability has no choice
            // Schedule the ready-up for end of this turn.
            DelayedAbility da;
            da.source = ctx.source;
            da.card_def_id = cardDefId();
            da.controller = ctx.controller;
            da.trigger = TriggerType::AtEndOfTurn;
            da.expires_on_turn = ctx.state.turn.turn_number;  // clean up EOT
            ctx.state.delayed_abilities.push_back(da);
            RIFT_TRACE(ctx.events, "TARGON'S PEAK: scheduled ready-2-runes for end of turn");
            return;
        }
        // AtEndOfTurn fire: ready up to 2 exhausted runes.
        // The player says which (a ready rune of one domain is not the same
        // as a ready rune of another once Power has to be paid) and how many:
        // one prompt per rune, "None" on each (CR 355.13).
        //
        // "runes", with no "friendly" (Sona 73) or "your" (Ekko 110): the word
        // refers to any rune on the board (CR 133.3.a, 355.9.a.1) and only a
        // printed restriction narrows a choice (CR 355.9.b), so an opponent's
        // exhausted rune may be named too.
        auto readiable = [&](GameObjectId id) {
            if (!ctx.state.objectExists(id)) return false;
            const auto& obj = ctx.state.getObject(id);
            return obj.isRune() && obj.is_exhausted && obj.location.has_value();
        };
        const std::string label = "Targon's Peak: ready a rune (up to 2)";
        // Asked as the ability is finalized. At resolution selectSeveral asks
        // nothing: it returns the runes named then, without any that has been
        // readied or has left the board since, and no other in its place.
        std::vector<GameObjectId> exhausted;
        for (auto& [id, obj] : ctx.state.objects)
            if (readiable(id)) exhausted.push_back(id);
        const auto chosen = selectSeveral(ctx, label, exhausted, 2);
        if (!chosen.has_value()) return;       // waiting on the agent
        const std::vector<GameObjectId>& picks = *chosen;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        int readied = 0;
        // No second readiable() check: every id selectSeveral returns is in
        // the list built above in this entry, and nothing changes a rune
        // between there and here (readying one rune does not touch another).
        for (auto id : picks) {
            ctx.executor.readyObject(id);
            ++readied;
        }
        RIFT_TRACE(ctx.events, "TARGON'S PEAK: readied " + std::to_string(readied) +
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
