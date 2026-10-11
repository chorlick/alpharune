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

class CuratorOfTheSands : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play a unit, gear, or activated ability with Energy cost [7] or
    // more, you may exhaust me to ready up to 2 runes."
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayACard, TriggerType::WhenYouActivateAnAbility};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto& ri = ctx.state.chain.resuming.value();
        if (ctx.firing_trigger != TriggerType::WhenYouActivateAnAbility) {
            const GameObjectId played = ri.triggering_subject;
            if (played == kInvalidId || !ctx.state.objectExists(played)) return;
            const auto& p = ctx.state.getObject(played);
            if (!p.isUnit() && !p.isGear()) return;
        }
        if (ri.triggering_spell_energy_spent < 7) return;

        auto exhausted_runes = [&ctx](GameObjectId except) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects)
                if (obj.isRune() && obj.controller == ctx.controller &&
                    obj.is_exhausted && obj.location.has_value() && id != except)
                    out.push_back(id);
            std::sort(out.begin(), out.end());
            return out;
        };
        auto still_legal = [&]() {
            return ctx.state.objectExists(ctx.source) &&
                   !ctx.state.getObject(ctx.source).is_exhausted &&
                   !exhausted_runes(kInvalidId).empty();
        };
        if (ctx.state.chain.resuming->may_answer == -1 && !still_legal()) return;
        int conf = confirmOptional(ctx, "Curator of the Sands: exhaust to ready 2 runes?",
                                   still_legal);
        if (conf < 1) return;
        // Which runes is the controller's choice (they differ by Domain). The
        // legend is exhausted only after both are known: the callback is
        // re-entered from the top with each answer.
        auto [first, second] = pickTargetPair(
            ctx, "Curator of the Sands: runes to ready", exhausted_runes(kInvalidId),
            [&](GameObjectId picked) { return exhausted_runes(picked); });
        if (first == kInvalidId) return;   // waiting for the choice
        ctx.executor.exhaustObject(ctx.source);
        ctx.executor.readyObject(first);
        if (second != kInvalidId) ctx.executor.readyObject(second);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 932;
        d.def_id = R"RB(ven-145-166)RB";
        d.name = R"RB(Curator of the Sands)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-145/166)RB";
        d.collector_number = 145;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Calm, Domain::Mind};
        d.tags = {R"RB(Nasus)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play a unit, gear, or activated ability with Energy cost [7] or more, you may exhaust me to ready up to 2 runes.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f26a25c5fddba7da788455c3d82b85c5abc7a2eb-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_932(CardRegistry& r) {
    r.registerCard(932, std::make_unique<CuratorOfTheSands>());
}

}  // namespace riftbound
