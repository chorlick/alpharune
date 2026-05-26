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

class FireBelowTheMountain : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        // [Add] [A] resolves immediately (CR 429.2) — not via the chain.
        ctx.executor.addFloatingUniversalPower(ctx.controller, 1);
        ctx.events.logTrace("ACTIVATE: Fire Below the Mountain adds [A] "
                            "(spend earmarked for gear — not engine-enforced)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 506;
        d.def_id = R"RB(sfd-189-221)RB";
        d.name = R"RB(Fire Below the Mountain)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-189/221)RB";
        d.collector_number = 189;
        d.artist = R"RB(Pandart Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Calm, Domain::Mind};
        d.tags = {R"RB(Ornn)RB"};
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([E]: [Reaction] — [Add] [A]. Use only to play gear or use gear abilities. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ed58d654034d545e54c85d836f3a6552772dd75b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_506(CardRegistry& r) {
    r.registerCard(506, std::make_unique<FireBelowTheMountain>());
}

} // namespace riftbound
