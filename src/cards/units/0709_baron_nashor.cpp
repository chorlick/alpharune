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

class BaronNashor : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool canBeChosenByEnemy() const override { return false; }

    // CR 135.2.b.3 / CR 355.1: "As you play me" runs DURING the play
    // action, not as a triggered ability. Implementing this via
    // Card::onPlay (invoked from executePlayCard between cost payment
    // and chain insertion) means there is no chain item, no priority
    // window between Baron being played and him entering the Pit —
    // closing the gap where an opponent could interrupt his arrival.
    // Baron never touches base; his location is rewritten before
    // resolvePermanent emits EnteredBoardEvent.
    void onPlay(CardContext& ctx) override {
        // Locate an existing Baron Pit by its BF card name.
        BattlefieldId pit_id = kInvalidId;
        for (auto& bf : ctx.state.battlefields) {
            if (!ctx.state.objectExists(bf.card_object_id)) continue;
            if (ctx.state.getObject(bf.card_object_id).name == "Baron Pit") {
                pit_id = bf.id;
                break;
            }
        }

        if (pit_id == kInvalidId) {
            // First Baron Nashor on the board this game — spawn the Pit
            // and rewrite Baron's pre-resolution location. "If you do,
            // I enter there" sets the play-step location atomically
            // with the play, NOT after he's landed at base.
            pit_id = ctx.executor.addBattlefieldToken(
                "Baron Pit", /*accepts_any_inbound=*/true);
            if (ctx.state.objectExists(ctx.source)) {
                auto& baron = ctx.state.getObject(ctx.source);
                baron.location = BattlefieldLocation{pit_id};
                ctx.events.logTrace("BARON NASHOR: created Baron Pit (bf=" +
                                     std::to_string(pit_id) +
                                     ") and will enter directly there");
            }
        } else {
            // Pit already existed (typically: the OTHER player's Baron
            // built it earlier). Per "If you do, I enter there", this
            // Baron does NOT enter the Pit — he stays where the normal
            // permanent-play resolution placed him (whatever location
            // the controller chose during step 2 of the play sequence).
            ctx.events.logTrace("BARON NASHOR: Baron Pit (bf=" +
                                 std::to_string(pit_id) +
                                 ") already exists; staying at chosen location.");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 709;
        d.def_id = R"RB(unl-147-219)RB";
        d.name = R"RB(Baron Nashor)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-147/219)RB";
        d.collector_number = 147;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(The Void)RB"};
        d.energy_cost = 10;
        d.power_cost = 3;
        d.might = 12;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(As you play me, add the Baron Pit battlefield token to the board if it's not there already. If you do, I enter there. (It has "Units can move here from anywhere.")
I can't be chosen by enemy spells and abilities.
Other friendly units have +2 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/59946969e8d21869c3ffe801a3ffbdd8165a873f-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_709(CardRegistry& r) {
    r.registerCard(709, std::make_unique<BaronNashor>());
}

} // namespace riftbound
