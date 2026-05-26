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

// "When a unit moves FROM here, give it +1 [M] this turn."
// Fires via WhenAUnitMovesFromHere on this battlefield's card; the departed unit
// is the trigger subject.
class BackAlleyBar : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override {
        return TriggerType::WhenAUnitMovesFromHere;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        GameObjectId subj = ctx.state.chain.resuming
            ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
        if (subj == kInvalidId || !ctx.state.objectExists(subj)) return;
        ctx.executor.giveTemporaryMight(subj, 1);
        ctx.events.logTrace("BACK-ALLEY BAR: unit left -> +1 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 272;
        d.def_id = R"RB(ogn-277-298)RB";
        d.name = R"RB(Back-Alley Bar)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-277/298)RB";
        d.collector_number = 277;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When a unit moves from here, give it +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3e9f659a32e390b45bc87a01bdd6af4a8a3565f7-1038x744.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_272(CardRegistry& r) {
    r.registerCard(272, std::make_unique<BackAlleyBar>());
}

} // namespace riftbound
