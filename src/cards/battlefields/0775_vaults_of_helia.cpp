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

class VaultsOfHelia : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        PlayerId who = ctx.controller;
        PlayerState::CostModifier m;
        m.source = ctx.source;
        m.energy_increase = 1;
        m.affects_non_token_only = true;
        m.this_turn_only = true;
        ctx.state.player(who).cost_modifiers.push_back(m);
        ctx.events.logTrace("VAULTS OF HELIA: " + std::string(toString(who)) +
                             "'s non-token units cost +1 this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 775;
        d.def_id = R"RB(unl-219-219)RB";
        d.name = R"RB(Vaults of Helia)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-219/219)RB";
        d.collector_number = 219;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you hold here, your non-token units cost [1] more to play this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9fc7b9a8294665881eece0dacaf3fc70e39f19e4-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_775(CardRegistry& r) {
    r.registerCard(775, std::make_unique<VaultsOfHelia>());
}

} // namespace riftbound
