#include "cards/card.h"
#include "cards/card_registry.h"
#include "cards/card_helpers.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "When a unit you control becomes [Mighty], you may pay [Y] to ready it.
//  (A unit is Mighty while it has 5+ [M].)"
// Wired via WhenAUnitBecomesMighty: GameEngine::cleanup edge-detects the 5+ [M]
// crossing and emits ObjectStateChangedEvent{"became_mighty"}; TriggerManager
// fans it out to the controller's on-board cards with the now-Mighty unit as
// the subject. We confirmOptional, pay one Order power ([Y]), then ready it.

class FioraWorthy : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override {
        return TriggerType::WhenAUnitBecomesMighty;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        GameObjectId subject = ctx.state.chain.resuming
            ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
        auto subject_on_board = [&]() {
            return subject != kInvalidId && ctx.state.objectExists(subject) &&
                   ctx.state.getObject(subject).isUnit() &&
                   ctx.state.getObject(subject).location.has_value();
        };
        // The [Y] is a cost: offered only when it can be paid, and paid as the
        // ability is finalized (CR 383.3), not when it resolves.
        auto still_legal = [&]() {
            return costAlreadyPaid(ctx) ||
                   (subject_on_board() &&
                    payableOnePower(ctx.state, ctx.controller, Domain::Order));
        };
        int conf = confirmOptional(ctx, "Fiora, Worthy: pay [Y] to ready it?",
                                   still_legal);
        bool paid = costAlreadyPaid(ctx);
        if (conf == 1 && costDueNow(ctx))
            paid = payOnePower(ctx, ctx.controller, Domain::Order);
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (conf != 1 || !paid) return;
        if (!subject_on_board()) return;      // it left the board in response
        ctx.executor.readyObject(subject);
        RIFT_TRACE(ctx.events, "FIORA WORTHY: paid [Y] -> readied the Mighty unit");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 500;
        d.def_id = R"RB(sfd-180-221)RB";
        d.name = R"RB(Fiora, Worthy)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-180/221)RB";
        d.collector_number = 180;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Fiora)RB", R"RB(Demacia)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When a unit you control becomes [Mighty], you may pay [Y] to ready it. (A unit is Mighty while it has 5+ [M].))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7339e76b7796242a15e97cfddbe04c32d5a05063-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_500(CardRegistry& r) {
    r.registerCard(500, std::make_unique<FioraWorthy>());
}

} // namespace riftbound
