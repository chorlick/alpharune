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

class ProdigalExplorer : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.executor.drawCards(ctx.controller, 1);
    }
    TriggerType triggerType() const override { return TriggerType::Activated; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    bool isActionAbility() const override { return true; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 514;
        d.def_id = R"RB(sfd-199-221)RB";
        d.name = R"RB(Prodigal Explorer)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-199/221)RB";
        d.collector_number = 199;
        d.artist = R"RB(Pandart Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Mind, Domain::Chaos};
        d.tags = {R"RB(Ezreal)RB"};
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([E]: [Reaction] — Draw 1. Use only if you've chosen enemy units and/or gear twice this turn with spells or unit abilities.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d0e143d9edbc14971b2a7b463b3c25b2b6a0c098-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_514(CardRegistry& r) {
    r.registerCard(514, std::make_unique<ProdigalExplorer>());
}

} // namespace riftbound
