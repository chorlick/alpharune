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

class MissFortuneCaptain : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    // "The first time I move each turn": judged when the move happens. Every
    // move bumps GameObject::moves_this_turn before its event is announced, so
    // the first move of the turn is the one that leaves it at 1. No flag is
    // written, so nothing has to be undone if the trigger is re-chosen.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return state.getObject(source).moves_this_turn == 1;
    }
    // "... you may ready something else that's exhausted."
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Anything exhausted other than me: units, gear and runes on the
        // board, and legends (exhausted in their Legend Zone, which is not a
        // location). The text is not limited to my controller's objects;
        // enemy ones that can't be chosen by enemy abilities are left out.
        auto find_targets = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source || !obj.is_exhausted) continue;
                const bool legend = obj.isLegend() && obj.zone == ZoneType::LegendZone;
                if (!obj.location.has_value() && !legend) continue;
                if (obj.controller != ctx.controller && obj.untargetable_by_enemy) continue;
                out.push_back(id);
            }
            std::sort(out.begin(), out.end());
            return out;
        };
        int answer = confirmOptional(ctx, "Miss Fortune ready another",
                                     [&]() { return !find_targets().empty(); });
        if (answer < 1) return;       // waiting / declined / nothing exhausted
        auto target = pickTarget(ctx, "Miss Fortune ready something exhausted",
                                  find_targets());
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (target == kInvalidId) return;     // none / mistarget / waiting
        ctx.executor.readyObject(target);
        ctx.events.logTrace("MISS FORTUNE: readied " +
                             ctx.state.getObject(target).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 162;
        d.def_id = R"RB(ogn-162-298)RB";
        d.name = R"RB(Miss Fortune, Captain)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-162/298)RB";
        d.collector_number = 162;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Pirate)RB", R"RB(Miss Fortune)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Accelerate);
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([Accelerate] (You may pay [1][O] as an additional cost to have me enter ready.)
[Ganking] (I can move from battlefield to battlefield.)
The first time I move each turn, you may ready something else that's exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3c7b219245cd6c6ee835974dd74771bc605289de-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_162(CardRegistry& r) {
    r.registerCard(162, std::make_unique<MissFortuneCaptain>());
}

} // namespace riftbound
