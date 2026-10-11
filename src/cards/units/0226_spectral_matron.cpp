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

class SpectralMatron : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    // "costing no more than [3] and no more than [A]": a limit on the card's
    // printed cost — Energy cost at most 3 and Power cost at most 1.
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
        auto still_legal = [&]() { return !eligibleTrashUnits(ctx).empty(); };
        int conf = confirmOptional(ctx,
            "Spectral Matron: play a <=3-cost unit from trash?", still_legal);
        GameObjectId picked = kInvalidId;
        if (conf == 1)
            picked = pickTarget(ctx, "Spectral Matron (unit from trash)",
                                eligibleTrashUnits(ctx));
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (conf < 1 || picked == kInvalidId) return;   // declined / none / waiting

        // CR 355.2.a: a played unit goes to its controller's base or to a
        // battlefield they control (and may be played to) — chosen as it is
        // played, through the shared helper every effect play uses.
        const int where = pickEffectPlayLocation(*this, ctx, "Spectral Matron: play it to");
        if (where == -1) return;   // waiting for the agent
        ctx.executor.playIgnoringCost(ctx.controller, picked,
                                      effectPlayLocation(ctx.state, ctx.controller, where));
        RIFT_TRACE(ctx.events, "SPECTRAL MATRON: played a unit from trash for free");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 226;
        d.def_id = R"RB(ogn-226-298)RB";
        d.name = R"RB(Spectral Matron)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-226/298)RB";
        d.collector_number = 226;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Spirit)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 4;
        d.power_cost = 2;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, you may play a unit costing no more than [3] and no more than [A] from your trash, ignoring its cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c8aebd03c8722d485ebfc38774488778c756ff5c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_226(CardRegistry& r) {
    r.registerCard(226, std::make_unique<SpectralMatron>());
}

} // namespace riftbound
