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

class Meditation : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "As an additional cost to play this, you may exhaust a friendly unit. If
    //  you do, draw 2. Otherwise, draw 1."
    // The "may", the unit and the exhaust belong to playing the spell: they
    // are decided and paid when it is finalized, not as it resolves.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // It chooses no unit of its controller's (the play gate, CR 355 -> 356).
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        (void)state; (void)controller; (void)object;
        return FinalizeChoice::No;
    }
    void onResolve(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        auto ready_friendly = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                if (obj.is_exhausted) continue;
                out.push_back(id);
            }
            std::sort(out.begin(), out.end());
            return out;
        };
        auto still_legal = [&]() {
            return costAlreadyPaid(ctx) || !ready_friendly().empty();
        };

        int conf = confirmOptional(ctx,
            "Meditation: exhaust a friendly unit to draw 2 (else draw 1)?",
            still_legal);
        if (conf == -1) return;  // waiting for agent choice

        bool paid = false;
        if (conf == 1) {
            if (costAlreadyPaid(ctx)) {
                paid = true;
            } else {
                const GameObjectId unit =
                    pickTarget(ctx, "Meditation: unit to exhaust", ready_friendly());
                if (unit == kInvalidId) {
                    if (ctx.state.chain.resuming.has_value() &&
                        ctx.state.chain.resuming->resume_point == 7) return;  // waiting
                } else if (costDueNow(ctx)) {
                    ctx.executor.exhaustObject(unit);
                    paid = true;
                }
            }
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)

        ctx.executor.drawCards(ctx.controller, paid ? 2 : 1);
        ctx.events.logTrace(paid ? "MEDITATION: exhausted friendly -> draw 2"
                                 : "MEDITATION: draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 48;
        d.def_id = R"RB(ogn-048-298)RB";
        d.name = R"RB(Meditation)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-048/298)RB";
        d.collector_number = 48;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
As an additional cost to play this, you may exhaust a friendly unit. If you do, draw 2. Otherwise, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7103336ab0d7f7bebdaa4155e4258d8d9beb06de-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_48(CardRegistry& r) {
    r.registerCard(48, std::make_unique<Meditation>());
}

} // namespace riftbound
