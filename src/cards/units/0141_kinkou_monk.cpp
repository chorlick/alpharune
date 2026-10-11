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

class KinkouMonk : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    // "Buff up to two OTHER friendly units." The units are targets, chosen as
    // the ability is finalized (CR 383.3), one prompt each. "Up to two"
    // includes none and one (CR 355.13): "Done" is on both prompts (listed
    // last), and with no other friendly unit nothing is asked. A unit that
    // already has a buff may be chosen and gets nothing: "each one that
    // doesn't have a buff gets a +1 [M] buff".
    //
    // Progress is in resume_data[kStage] (0 new / 1 first asked / 2 second
    // asked / 3 chosen) with the picks in [kFirst] and [kSecond]; slots 0..6
    // belong to the Card::pick* helpers, which this callback does not use.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto others = [&](GameObjectId except) {
            return chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                return o.id != ctx.source && o.id != except &&      // "other"
                       o.controller == ctx.controller;
            });
        };
        auto buff_if_unbuffed = [&](GameObjectId id) {
            if (id == kInvalidId || !ctx.state.objectExists(id)) return;
            if (ctx.state.getObject(id).buff_count > 0) return;  // already has a buff
            ctx.executor.buffUnit(id);
        };
        auto& ri_opt = ctx.state.chain.resuming;
        if (!ri_opt.has_value()) {
            // Direct invocation: nobody to ask, the first two are taken.
            const auto units = others(kInvalidId);
            for (size_t i = 0; i < units.size() && i < 2; ++i) buff_if_unbuffed(units[i]);
            return;
        }
        auto& ri = *ri_opt;
        if (ri.resume_data.size() <= kSecond) {
            ri.resume_data.resize(kSecond + 1, 0);
            ri.resume_data[kFirst] = static_cast<int32_t>(kInvalidId);
            ri.resume_data[kSecond] = static_cast<int32_t>(kInvalidId);
        }
        // Publishes one prompt over `units`; false when there is none to offer.
        auto ask = [&](const std::vector<GameObjectId>& units, int stage) {
            if (units.empty()) return false;
            std::vector<Intent> options;
            options.reserve(units.size() + 1);
            for (auto id : units) options.push_back(objChoice(ctx.controller, id));
            options.push_back(valChoice(ctx.controller, 0, "Done"));
            ctx.executor.requestChoice(ctx.controller, std::move(options),
                                       "Kinkou Monk: buff another friendly unit (up to two)");
            ri.resume_data[kStage] = stage;
            return true;
        };
        if (ri.resume_data[kStage] == 0) {
            if (ask(offerableChoices(ctx, others(kInvalidId)), 1)) return;
            ri.resume_data[kStage] = 3;
        } else if (ri.resume_data[kStage] == 1 || ri.resume_data[kStage] == 2) {
            const bool first = ri.resume_data[kStage] == 1;
            const auto prior = static_cast<GameObjectId>(ri.resume_data[kFirst]);
            const auto units = offerableChoices(ctx, others(first ? kInvalidId : prior));
            const GameObjectId picked = takenObject(ctx);
            ri.resume_data[kStage] = 3;
            if (std::find(units.begin(), units.end(), picked) != units.end()) {
                commitChosenObject(ctx, picked);
                ri.resume_data[first ? kFirst : kSecond] = static_cast<int32_t>(picked);
                if (first && ask(offerableChoices(ctx, others(picked)), 2)) return;
            }   // else "Done"
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        // A unit that is no longer another friendly unit on the board when
        // this resolves is not buffed (CR 359.3.e.5).
        const auto still = others(kInvalidId);
        for (size_t slot : {kFirst, kSecond}) {
            const auto id = static_cast<GameObjectId>(ri.resume_data[slot]);
            if (std::find(still.begin(), still.end(), id) != still.end()) buff_if_unbuffed(id);
        }
    }
private:
    static constexpr size_t kStage = 7;
    static constexpr size_t kFirst = 8;
    static constexpr size_t kSecond = 9;
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 141;
        d.def_id = R"RB(ogn-141-298)RB";
        d.name = R"RB(Kinkou Monk)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-141/298)RB";
        d.collector_number = 141;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, buff up to two other friendly units. (Each one that doesn't have a buff gets a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/648b22c6f419ee55723247bbf4c3ac10f0be9ab5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_141(CardRegistry& r) {
    r.registerCard(141, std::make_unique<KinkouMonk>());
}

} // namespace riftbound
