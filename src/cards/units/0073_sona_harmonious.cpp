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

class SonaHarmonious : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // Which runes is this ability's choice: it is made as the ability is
    // finalized (CR 355.5, 383.3), so the opponent responds knowing them.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::AtEndOfTurn; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto& item = ctx.state.chain.resuming;
        const bool resolving_chosen =
            item.has_value() && item->choices_finalized && !item->finalizing;
        // "if I'm at a battlefield" is asked when the ability goes on the
        // chain, not again as it resolves: "If she is removed in reaction to
        // the triggered ability, it will still resolve" (CR 383, the Sona
        // example).
        if (!resolving_chosen) {
            if (!ctx.state.objectExists(ctx.source)) return;
            if (!ctx.state.getObject(ctx.source).isAtBattlefield()) return;
        }
        // "friendly runes": printed, so only its controller's (CR 355.9.b,
        // 740.1.a) — unlike the bare "runes" of Dark Child 315, Targon's Peak
        // 284, Gentle Gemdragon 666, Curator of the Sands 932 and Hwei 642.
        auto readiable = [&](GameObjectId id) {
            if (!ctx.state.objectExists(id)) return false;
            const auto& obj = ctx.state.getObject(id);
            return obj.isRune() && obj.controller == ctx.controller && obj.is_exhausted &&
                   obj.location.has_value();
        };
        // "up to 4": the controller names the runes one at a time and may stop
        // at any prompt, the first included (CR 355.13). Which ones matters
        // when more than four are exhausted (a rune left exhausted is the one
        // to recycle for Power).
        const std::string label = "Sona, Harmonious: ready a friendly rune (up to four)";
        std::vector<GameObjectId> picks;
        if (resolving_chosen) {
            // Resolving: the runes named at finalization, as selectSeveral
            // kept them on the item (two slots per pick from slot 7). Nothing
            // is offered, so nothing is asked and nothing new can be named.
            for (size_t k = 0; k < 4; ++k) {
                const auto one = selectObject(ctx, label, {}, 7 + 2 * k, /*may_decline=*/true);
                if (!one.has_value() || *one == kInvalidId) break;
                picks.push_back(*one);
            }
        } else {
            std::vector<GameObjectId> exhausted;
            for (auto& [id, obj] : ctx.state.objects)
                if (readiable(id)) exhausted.push_back(id);
            const auto chosen = selectSeveral(ctx, label, exhausted, /*max_picks=*/4);
            if (!chosen.has_value()) return;   // waiting for the agent
            picks = *chosen;
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        // A rune readied or off the board since it was named is skipped.
        for (auto id : picks)
            if (readiable(id)) ctx.executor.readyObject(id);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 73;
        d.def_id = R"RB(ogn-073-298)RB";
        d.name = R"RB(Sona, Harmonious)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-073/298)RB";
        d.collector_number = 73;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Sona)RB", R"RB(Demacia)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(At the end of your turn, if I'm at a battlefield, ready up to 4 friendly runes.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8582f6430821fb912fcb3619c5ce9405f254cb2f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_73(CardRegistry& r) {
    r.registerCard(73, std::make_unique<SonaHarmonious>());
}

} // namespace riftbound
