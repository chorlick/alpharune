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
    // "While you control this battlefield" and "if you spent [4] or more" are
    // the trigger's condition (CR 383.2.a.1): asked when the spell has been
    // played (`controller` is the player who played it, `subject` the spell).
    // What was spent on that spell is stamped on it as it is played
    // (TriggerManager::onCardPlayed); PlayerState::last_spell_energy_spent is
    // the fallback for a path that names no spell.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType /*trigger*/, GameObjectId subject) const override {
        bool mine = false;
        for (const auto& bf : state.battlefields)
            if (bf.card_object_id == source)
                mine = bf.controller.has_value() && *bf.controller == controller;
        if (!mine) return false;
        int spent = state.player(controller).last_spell_energy_spent;
        if (subject != kInvalidId && state.objectExists(subject)) {
            const auto& c = state.getObject(subject).card_counters;
            if (const auto it = c.find("__energy_spent_on_play"); it != c.end()) spent = it->second;
        }
        return spent >= 4;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        RIFT_TRACE(ctx.events, "FORGOTTEN LIBRARY: predict 1");
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
        // No Keyword::Predict: Predict is a game action (CR 437), not a
        // keyword the card has (testing phase R12; plans/08 8.8).
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
