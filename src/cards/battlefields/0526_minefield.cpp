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

// "When you conquer here, put the top 2 cards of your Main Deck into your trash."

class Minefield : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Moving cards from the top of the Main Deck to the trash is burning
        // (CR 441.1), whatever the card calls it: with fewer than 2 cards
        // left the player moves what is there, Burns Out and moves the rest
        // (CR 432.1.b, 441.4) — EffectExecutor::burn.
        ctx.executor.burn(ctx.controller, 2);
        RIFT_TRACE(ctx.events, "MINEFIELD: conquer -> top 2 of Main Deck to trash");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 526;
        d.def_id = R"RB(sfd-212-221)RB";
        d.name = R"RB(Minefield)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-212/221)RB";
        d.collector_number = 212;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here, put the top 2 cards of your Main Deck into your trash.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/46614658b08563f07cc81a4f4fa4ec8a067710b9-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_526(CardRegistry& r) {
    r.registerCard(526, std::make_unique<Minefield>());
}

} // namespace riftbound
