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

class JayceBrilliantInventor : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    // "When you play me or the first time you play a non-token gear each turn,
    // you may ready something besides me that's exhausted."
    // The gear half rides WhenYouPlayACard (its chain item carries the played
    // card as triggering_subject, which WhenYouPlayAGear does not).
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenYouPlayACard};
    }
    static constexpr const char* kFirstGearTurn = "__jayce_first_gear_turn";
    // "the first time you play a non-token gear each turn": judged when the
    // gear is played. The stamp below is written as the ability goes on the
    // chain, before another gear can be played.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType trigger, GameObjectId subject) const override {
        if (trigger != TriggerType::WhenYouPlayACard) return true;
        if (subject == kInvalidId || !state.objectExists(subject)) return false;
        const auto& played = state.getObject(subject);
        if (!played.isGear() || played.isToken()) return false;      // non-token gear only
        const auto& counters = state.getObject(source).card_counters;
        auto it = counters.find(kFirstGearTurn);
        return it == counters.end() || it->second != state.turn.turn_number + 1;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // A non-token gear has been played this turn (the one that triggered
        // me, or one played before I was): later ones are not "the first".
        // Writing the same stamp again on a re-entry changes nothing, and no
        // path below depends on it, so a re-chosen ability is not lost.
        if (ctx.firing_trigger == TriggerType::WhenYouPlayACard ||
            ctx.state.player(ctx.controller).played_nontoken_gear_this_turn)
            ctx.state.getObject(ctx.source).card_counters[kFirstGearTurn] =
                ctx.state.turn.turn_number + 1;   // +1 so 0 = never
        // "something besides me that's exhausted": the text is not limited to
        // my controller's objects (as Miss Fortune, Captain 162) — units, gear
        // and runes on the board and legends in their Legend Zone, either
        // player's; enemy ones that can't be chosen by enemy abilities are
        // left out.
        auto find_targets = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects.inPlay()) {
                if (id == ctx.source || !obj.is_exhausted) continue;
                const bool legend = obj.isLegend() && obj.zone == ZoneType::LegendZone;
                if (!obj.location.has_value() && !legend) continue;
                if (obj.controller != ctx.controller && obj.untargetable_by_enemy) continue;
                out.push_back(id);
            }
            std::sort(out.begin(), out.end());
            return out;
        };
        int answer = confirmOptional(ctx, "Jayce, Brilliant Inventor: ready something else?",
                                     [&]() { return !find_targets().empty(); });
        if (answer != 1) return;                      // waiting, declined or nothing exhausted
        auto target = pickTarget(ctx, "Jayce, Brilliant Inventor: ready", find_targets());
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (target == kInvalidId || !ctx.state.objectExists(target)) return;
        ctx.executor.readyObject(target);
        RIFT_TRACE(ctx.events, "JAYCE, BRILLIANT INVENTOR: readied " +
                            ctx.state.getObject(target).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 855;
        d.def_id = R"RB(ven-068-166)RB";
        d.name = R"RB(Jayce, Brilliant Inventor)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-068/166)RB";
        d.collector_number = 68;
        d.artist = R"RB(League of Legends)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Jayce)RB", R"RB(Piltover)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When you play me or the first time you play a non-token gear each turn, you may ready something besides me that's exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/123e949085a63fdb6e1590ff0e519cab5758688b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_855(CardRegistry& r) {
    r.registerCard(855, std::make_unique<JayceBrilliantInventor>());
}

}  // namespace riftbound
