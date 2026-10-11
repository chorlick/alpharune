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
    // The "may", the exhaust (a cost within instructions, CR 383.3.b) and the
    // runes are all decided as the ability is finalized (CR 355.5, 383.3).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
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

        // "runes", with no "friendly" (Sona 73) or "your" (Ekko 110): the word
        // refers to any rune on the board (CR 133.3.a, 355.9.a.1) and only a
        // printed restriction narrows a choice (CR 355.9.b), so an opponent's
        // exhausted rune may be named too.
        auto readiable = [&ctx](GameObjectId id) {
            if (!ctx.state.objectExists(id)) return false;
            const auto& obj = ctx.state.getObject(id);
            return obj.isRune() && obj.is_exhausted && obj.location.has_value();
        };
        auto exhausted_runes = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects)
                if (readiable(id)) out.push_back(id);
            std::sort(out.begin(), out.end());
            return out;
        };
        // The cost is all the "may" needs: "up to 2" includes none (CR
        // 355.13), so it is offered with no exhausted rune too.
        auto still_legal = [&]() {
            return ctx.state.objectExists(ctx.source) &&
                   !ctx.state.getObject(ctx.source).is_exhausted;
        };
        int conf = confirmOptional(ctx, "Curator of the Sands: exhaust to ready up to 2 runes?",
                                   still_legal);
        if (conf < 1) return;
        // Which runes, and how many, is the controller's choice (they differ
        // by Domain): one prompt per rune, "None" on each. The legend is
        // exhausted only after the picks are known: the callback is
        // re-entered from the top with each answer, and a cost is paid
        // immediately before the end-of-choices marker.
        const std::string label = "Curator of the Sands: ready a rune (up to 2)";
        // At resolution selectSeveral asks nothing: it returns the runes
        // named at finalization, without any that has been readied or has
        // left the board since, and no other in its place.
        const auto chosen = selectSeveral(ctx, label, exhausted_runes(), 2);
        if (!chosen.has_value()) return;    // waiting for the choice
        const std::vector<GameObjectId>& picks = *chosen;
        if (costDueNow(ctx)) ctx.executor.exhaustObject(ctx.source);
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        // No second readiable() check: every id selectSeveral returns is in
        // the list built for it in this entry, and the only state change
        // since is the cost — exhausting me, a legend, which no rune's
        // readiable() depends on. Readying one rune does not touch another.
        for (auto id : picks) ctx.executor.readyObject(id);
        ctx.events.logTrace("CURATOR OF THE SANDS: readied " + std::to_string(picks.size()) +
                            " rune(s)");
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
