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

class JaeMedarda : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you choose me with a spell, draw 1."
    // WhenYouChooseAFriendlyUnit fires on every friendly watcher with the
    // chosen unit as subject: only my own choice counts, and only when a
    // SPELL made it. Checked as the choice happens: the item that is choosing
    // is the one being finalized / resolved (chain.resuming) or, for a spell
    // whose targets were fixed as it was played, the newest item naming me.
    TriggerType triggerType() const override { return TriggerType::WhenYouChooseAFriendlyUnit; }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType /*trigger*/, GameObjectId subject) const override {
        if (subject != source) return false;
        auto names_me = [source](const ChainItem& item) {
            return std::find(item.chosen_objects.begin(), item.chosen_objects.end(), source)
                       != item.chosen_objects.end() ||
                   std::find(item.targets.begin(), item.targets.end(), source)
                       != item.targets.end();
        };
        if (state.chain.resuming.has_value() && names_me(*state.chain.resuming))
            return state.chain.resuming->is_spell && state.chain.resuming->controller == controller;
        for (auto it = state.chain.items.rbegin(); it != state.chain.items.rend(); ++it)
            if (names_me(*it)) return it->is_spell && it->controller == controller;
        return false;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.drawCards(ctx.controller, 1);
        RIFT_TRACE(ctx.events, "JAE MEDARDA: chosen by a spell -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 463;
        d.def_id = R"RB(sfd-142-221)RB";
        d.name = R"RB(Jae Medarda)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-142/221)RB";
        d.collector_number = 142;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 5;
        d.power_cost = 2;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you choose me with a spell, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/92eac6bb020a068d9c1668b59ecaaeab05112b3f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_463(CardRegistry& r) {
    r.registerCard(463, std::make_unique<JaeMedarda>());
}

} // namespace riftbound
