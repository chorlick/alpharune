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

class BlastconeFae : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, give a unit -2 [M] this turn, to a minimum of 1 [M]."
    // Chosen as the ability goes on the chain (CR 383.3).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Played from facedown, a unit at the battlefield she was played to
        // (CR 811.1.d.3, whose first example is this card).
        auto units = atHiddenBattlefield(ctx.state, ctx.source,
            chooseableUnits(ctx.state, ctx.controller, [](const GameObject&) { return true; }));
        std::sort(units.begin(), units.end());
        const GameObjectId t = pickTarget(ctx, "Blastcone Fae: give a unit -2 [M]", units);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId) return;          // none / mistarget / waiting
        ctx.executor.giveTemporaryMight(t, -2, /*minimum=*/1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 97;
        d.def_id = R"RB(ogn-097-298)RB";
        d.name = R"RB(Blastcone Fae)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-097/298)RB";
        d.collector_number = 97;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Fae)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.might = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
When you play me, give a unit -2 [M] this turn, to a minimum of 1 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fac53b2216490c99ad7ce11dc5e663a692d6c104-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_97(CardRegistry& r) {
    r.registerCard(97, std::make_unique<BlastconeFae>());
}

} // namespace riftbound
