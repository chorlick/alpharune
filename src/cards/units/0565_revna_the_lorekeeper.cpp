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

class RevnaTheLorekeeper : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Ganking] is engine-handled via the printed keyword.
    // "When you play a spell, if you spent [4] or more, ready me."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayASpell; }
    // "if you spent [4] or more" is part of the trigger condition (CR
    // 383.2.a.1): asked when the spell has been played; a cheaper spell puts
    // nothing on the chain. What was spent on THAT spell is stamped on it as
    // it is played (TriggerManager::onCardPlayed) —
    // PlayerState::last_spell_energy_spent is the latest spell's, which may
    // be a different, dearer one played in response.
    bool triggerCondition(const GameState& state, GameObjectId /*source*/, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId subject) const override {
        if (subject == kInvalidId || !state.objectExists(subject)) return false;
        const auto& c = state.getObject(subject).card_counters;
        const auto it = c.find("__energy_spent_on_play");
        return it != c.end() && it->second >= 4;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.readyObject(ctx.source);
        RIFT_TRACE(ctx.events, "REVNA: spent 4+ on a spell -> ready me");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 565;
        d.def_id = R"RB(unl-005-219)RB";
        d.name = R"RB(Revna the Lorekeeper)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-005/219)RB";
        d.collector_number = 5;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Freljord)RB"};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 7;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([Ganking] (I can move from battlefield to battlefield.)
When you play a spell, if you spent [4] or more, ready me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8e251eed411d614de504480181b357df3f78b133-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_565(CardRegistry& r) {
    r.registerCard(565, std::make_unique<RevnaTheLorekeeper>());
}

} // namespace riftbound
