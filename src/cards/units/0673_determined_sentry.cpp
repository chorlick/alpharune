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

class DeterminedSentry : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onPlay(CardContext& ctx) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.state.getObject(ctx.source).cant_move_to_base = true;
        ctx.events.logTrace("DETERMINED SENTRY: can't move to base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 673;
        d.def_id = R"RB(unl-111-219)RB";
        d.name = R"RB(Determined Sentry)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-111/219)RB";
        d.collector_number = 111;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Yordle)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 1;
        d.might = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(I can't move to base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b491c050b8505c52000c5ad7bb5a0b7855fa4ad1-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_673(CardRegistry& r) {
    r.registerCard(673, std::make_unique<DeterminedSentry>());
}

} // namespace riftbound
