#include "cards/card.h"
#include "cards/card_helpers.h"
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

class MaddenedMarauder : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, move a unit from a battlefield to its base." Chosen
    // as the ability goes on the chain (CR 383.3); only a unit at a
    // battlefield can be moved from one.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [](const GameObject& o) {
            return o.isAtBattlefield();
        });
        std::sort(units.begin(), units.end());
        const GameObjectId t = pickTarget(ctx, "Maddened Marauder: move a unit to its base",
                                          units);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId) return;          // none / mistarget / waiting
        ctx.executor.moveToBase(t);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 191;
        d.def_id = R"RB(ogn-191-298)RB";
        d.name = R"RB(Maddened Marauder)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-191/298)RB";
        d.collector_number = 191;
        d.artist = R"RB(Alex Heath)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Pirate)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 5;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Tank] (I must be assigned combat damage first.)
When you play me, move a unit from a battlefield to its base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0f3a2ab8e5894e0dd1b625ee29f3304e7832ef9a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_191(CardRegistry& r) {
    r.registerCard(191, std::make_unique<MaddenedMarauder>());
}

} // namespace riftbound
