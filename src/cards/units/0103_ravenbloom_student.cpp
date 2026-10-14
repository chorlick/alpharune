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

class RavenbloomStudent : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // Played back by the spell it would trigger on (Temporal Breach on this
    // Student): it is on the board when that spell's play is completed by its
    // resolution (CR 420.4.a), so it triggers. Judge question 10 (finding
    // G26); pinned by Batch1b.G26_RavenbloomStudentPlayedBackByTemporalBreach
    // TriggersOnIt_J7.
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayASpell; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;   // left the board
        ctx.executor.giveTemporaryMight(ctx.source, 1);
        RIFT_TRACE(ctx.events, "RAVENBLOOM STUDENT: +1 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 103;
        d.def_id = R"RB(ogn-103-298)RB";
        d.name = R"RB(Ravenbloom Student)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-103/298)RB";
        d.collector_number = 103;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play a spell, give me +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b98b1ff793b6eb263957258742cc83d50fda6537-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_103(CardRegistry& r) {
    r.registerCard(103, std::make_unique<RavenbloomStudent>());
}

} // namespace riftbound
