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

class GlascMixologist : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }

    // "with cost no more than [3] and no more than [A]": a limit on the card's
    // printed cost — Energy cost at most 3 and Power cost at most 1 (the same
    // rule as Spectral Matron 226).
    std::vector<GameObjectId> eligibleTrashUnits(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        auto& ps = ctx.state.player(ctx.controller);
        for (auto cid : ps.trash) {
            if (!ctx.state.objectExists(cid)) continue;
            auto& obj = ctx.state.getObject(cid);
            if (!obj.isUnit()) continue;
            if (obj.card_def_id == kInvalidId) continue;
            const auto& cdef = ctx.executor.cardDB().get(obj.card_def_id);
            if (cdef.energy_cost > 3) continue;   // "no more than [3]"
            if (cdef.power_cost > 1) continue;    // "no more than [A]"
            out.push_back(cid);
        }
        return out;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto still_legal = [&]() {
            return !eligibleTrashUnits(ctx).empty();
        };
        int conf = confirmOptional(ctx,
            "Glasc Mixologist: play a <=3-cost unit from trash?", still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined / nothing eligible

        auto eligible = eligibleTrashUnits(ctx);
        if (eligible.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Glasc Mixologist (unit from trash)",
                                          eligible);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        // Suspended for agent decision.
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;

        // CR 355.2.a: a played unit goes to its controller's base or to a
        // battlefield they control (and may be played to) — chosen as it is
        // played, through the shared helper (as Spectral Matron 226).
        const int where = pickEffectPlayLocation(*this, ctx, "Glasc Mixologist: play it to");
        if (where == -1) return;   // waiting for the agent
        // playIgnoringCost takes it out of the trash.
        ctx.executor.playIgnoringCost(ctx.controller, picked,
                                      effectPlayLocation(ctx.state, ctx.controller, where));
        ctx.events.logTrace("GLASC MIXOLOGIST: played a unit from trash for free");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 486;
        d.def_id = R"RB(sfd-165-221)RB";
        d.name = R"RB(Glasc Mixologist)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-165/221)RB";
        d.collector_number = 165;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Zaun)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell] — You may play a unit with cost no more than [3] and no more than [A] from your trash, ignoring its cost. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e7422fee71796aa61f8d9691d07654b901fc20f2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_486(CardRegistry& r) {
    r.registerCard(486, std::make_unique<GlascMixologist>());
}

} // namespace riftbound
