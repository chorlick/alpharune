#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class ChemBaroness : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When you or an ally hold, you may exhaust me to play a Gold gear token
    // exhausted." — a hold, not a conquer.
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "exhaust me" can only be paid by a ready legend.
        auto still_legal = [&]() {
            return ctx.state.objectExists(ctx.source) &&
                   !ctx.state.getObject(ctx.source).is_exhausted;
        };
        const int yes = confirmOptional(ctx, "Chem-Baroness: exhaust to play a Gold token?",
                                        still_legal);
        if (yes == 1 && costDueNow(ctx)) ctx.executor.exhaustObject(ctx.source);
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (yes != 1) return;
        // CR 187.5: a Gold gear token has "[Reaction][>] Kill this, [E]:
        // [Add] [A]" — carried by the registered Gold card.
        createGoldExhausted(ctx);
        RIFT_TRACE(ctx.events, "CHEM-BARONESS: played a Gold token");
    }

    // "While your score is within 3 points of the Victory Score, your Gold
    // [ADD] an additional [1]." Published on the legend itself every aura
    // pass, in card_counters["__gold_add_bonus_energy"] (0 = no bonus); the
    // Gold cards (326, 564) read it from their controller's legend as they add.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId, GameObject& self) {
            const bool close =
                state.player(controller).score + 3 >= state.effectiveVictoryScore();
            self.card_counters["__gold_add_bonus_energy"] = close ? 1 : 0;
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 556;
        d.def_id = R"RB(sfd-249-221)RB";
        d.name = R"RB(Chem-Baroness)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-249/221)RB";
        d.collector_number = 249;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Mind, Domain::Order};
        d.tags = {R"RB(Renata Glasc)RB"};
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(When you or an ally hold, you may exhaust me to play a Gold gear token exhausted.
While your score is within 3 points of the Victory Score, your Gold [ADD] an additional [1].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5df25d5a1351d0a97e103ef8e155991297b86ca9-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_556(CardRegistry& r) {
    r.registerCard(556, std::make_unique<ChemBaroness>());
}

} // namespace riftbound
