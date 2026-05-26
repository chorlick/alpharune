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

class BrynhirThundersong : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        PlayerId opp = opponent(ctx.controller);
        ctx.state.player(opp).cant_play_cards_this_turn = true;
        ctx.events.logTrace("BRYNHIR: " + std::string(toString(opp)) +
                            " can't play cards this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 26;
        d.def_id = R"RB(ogn-026-298)RB";
        d.name = R"RB(Brynhir Thundersong)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-026/298)RB";
        d.collector_number = 26;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Freljord)RB"};
        d.energy_cost = 6;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, opponents can't play cards this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/44e805aa0f2928acf1156eef692403df319b333a-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_26(CardRegistry& r) {
    r.registerCard(26, std::make_unique<BrynhirThundersong>());
}

} // namespace riftbound
