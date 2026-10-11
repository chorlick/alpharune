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

class ViDestructive : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "Recycle 1 from your trash: Give me +1 [M] this turn." The recycle is
    // the activation cost (no [E]): my controller chooses the card and it is
    // recycled as the ability is activated (CR 355), before anyone responds.
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {}; }
    bool canActivateAbility(const GameState& state,
                            PlayerId controller) const override {
        return !state.player(controller).trash.empty();
    }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        if (!costAlreadyPaid(ctx)) {
            auto& ps = ctx.state.player(ctx.controller);
            const GameObjectId card = pickTarget(ctx, "Vi: recycle which card from your trash",
                                                 ps.trash);
            if (card == kInvalidId) return;   // empty trash / waiting
            if (costDueNow(ctx)) {
                auto it = std::find(ps.trash.begin(), ps.trash.end(), card);
                if (it == ps.trash.end()) return;
                ps.trash.erase(it);
                ctx.executor.recycleCards(ctx.controller, {card});
            }
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 355)
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;   // left the board
        ctx.executor.giveTemporaryMight(ctx.source, 1);
        RIFT_TRACE(ctx.events, "VI: recycled 1 from trash -> +1 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 36;
        d.def_id = R"RB(ogn-036-298)RB";
        d.name = R"RB(Vi, Destructive)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-036/298)RB";
        d.collector_number = 36;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Vi)RB", R"RB(Piltover)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([Ganking] (I can move from battlefield to battlefield.)
Recycle 1 from your trash: Give me +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7ab52254ac49b8853fc7ae65b03aaee3f8c5994a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_36(CardRegistry& r) {
    r.registerCard(36, std::make_unique<ViDestructive>());
}

} // namespace riftbound
