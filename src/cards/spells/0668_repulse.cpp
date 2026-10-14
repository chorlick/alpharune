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

class Repulse : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose a friendly unit at a battlefield. Counter an enemy spell or
    //  ability that chooses it and no other friendly unit."
    // Both are targets chosen as Repulse is played (CR 355.5, 355.9.a.2): a
    // friendly unit at a battlefield that some enemy item chooses alone, then
    // such an item — anywhere on the chain.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        // Only a unit some enemy item on the chain chooses alone.
        return friendlyUnitChoice(state, controller, object, [&](const GameObject& u) {
            return u.isAtBattlefield() && anyChainItem(state, [&](const ChainItem& ci) {
                return answers(state, ci, controller, u.id);
            });
        });
    }
    /// The one friendly unit `item` chooses, if it chooses exactly one and
    /// that unit is at a battlefield; kInvalidId otherwise.
    static GameObjectId soleFriendlyUnitChosen(const GameState& state, const ChainItem& item,
                                               PlayerId controller) {
        GameObjectId only = kInvalidId;
        auto scan = [&](const std::vector<GameObjectId>& ids) {
            for (auto tid : ids) {
                if (!state.objectExists(tid)) continue;
                const auto& t = state.getObject(tid);
                if (!t.isUnit() || t.controller != controller || !t.location.has_value())
                    continue;
                if (only == kInvalidId) only = tid;
                else if (only != tid) return false;      // "and no other friendly unit"
            }
            return true;
        };
        if (!scan(item.chosen_objects) || !scan(item.targets)) return kInvalidId;
        if (only == kInvalidId) return kInvalidId;
        if (!state.getObject(only).isAtBattlefield()) return kInvalidId;   // "at a battlefield"
        return only;
    }
    static bool answers(const GameState& state, const ChainItem& item, PlayerId controller,
                        GameObjectId unit) {
        if (item.controller == controller) return false;          // "an enemy ..."
        if (!item.is_spell && !item.is_ability) return false;     // "... spell or ability"
        const GameObjectId u = soleFriendlyUnitChosen(state, item, controller);
        return u != kInvalidId && (unit == kInvalidId || u == unit);
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return anyChainItem(state, [&](const ChainItem& ci) {
            return answers(state, ci, controller, kInvalidId);
        });
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // The units some enemy item on the chain chooses alone.
        std::vector<GameObjectId> units;
        for (const auto& ci : ctx.state.chain.items) {
            if (ci.status != ChainItemStatus::Finalized || !answers(ctx.state, ci, ctx.controller, kInvalidId))
                continue;
            const GameObjectId u = soleFriendlyUnitChosen(ctx.state, ci, ctx.controller);
            if (std::find(units.begin(), units.end(), u) == units.end()) units.push_back(u);
        }
        std::sort(units.begin(), units.end());
        const GameObjectId unit = pickTarget(ctx, "Repulse: choose a friendly unit", units);
        if (unit == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;   // waiting for the agent
        if (unit == kInvalidId && finalizingChoices(ctx)) return;   // nothing to choose
        const auto item = pickChainItem(
            ctx, "Repulse: counter an enemy spell or ability that chooses it",
            chainItemChoices(ctx, [&](const ChainItem& ci) {
                return unit != kInvalidId && answers(ctx.state, ci, ctx.controller, unit);
            }));
        if (!item.has_value()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (unit == kInvalidId || *item == kInvalidId) return;   // a mistarget
        counterChosenItem(ctx, *item);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 668;
        d.def_id = R"RB(unl-106-219)RB";
        d.name = R"RB(Repulse)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-106/219)RB";
        d.collector_number = 106;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a friendly unit at a battlefield. Counter an enemy spell or ability that chooses it and no other friendly unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ca56ad44ec24db67e10a4ee18f5c7f3756c83d94-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_668(CardRegistry& r) {
    r.registerCard(668, std::make_unique<Repulse>());
}

} // namespace riftbound
