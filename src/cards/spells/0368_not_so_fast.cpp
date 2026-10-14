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

class NotSoFast : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Counter an enemy spell or ability that chooses a friendly unit or gear."
    // The item is a target (CR 355.9.a.2), chosen as this is played among the
    // finalized enemy items on the chain that qualify — anywhere on it. What an
    // item chooses is ChainItem::chosen_objects together with its play-time
    // targets. A triggered ability's subject (ChainItem::triggering_subject)
    // is not a choice and is not read (CR 355: choosing is an action of the
    // spell or ability, not a property of the event that triggered it).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;
    }
    static bool choosesFriendlyUnitOrGear(const GameState& state, const ChainItem& item,
                                          PlayerId controller) {
        if (item.controller == controller) return false;            // "an enemy ..."
        if (!item.is_spell && !item.is_ability) return false;       // "... spell or ability"
        auto scan = [&](const std::vector<GameObjectId>& ids) {
            for (auto tid : ids) {
                if (!state.objectExists(tid)) continue;
                const auto& t = state.getObject(tid);
                if (t.controller == controller && t.location.has_value() &&
                    (t.isUnit() || t.isGear())) return true;
            }
            return false;
        };
        return scan(item.chosen_objects) || scan(item.targets);
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return anyChainItem(state, [&](const ChainItem& ci) {
            return choosesFriendlyUnitOrGear(state, ci, controller);
        });
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto item = pickChainItem(
            ctx, "Not So Fast: counter an enemy spell or ability",
            chainItemChoices(ctx, [&](const ChainItem& ci) {
                return choosesFriendlyUnitOrGear(ctx.state, ci, ctx.controller);
            }));
        if (!item.has_value()) return;
        if (finalizingChoices(ctx)) return;
        if (*item != kInvalidId) counterChosenItem(ctx, *item);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 368;
        d.def_id = R"RB(sfd-045-221)RB";
        d.name = R"RB(Not So Fast)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-045/221)RB";
        d.collector_number = 45;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Counter an enemy spell or ability that chooses a friendly unit or gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e576fefdce72ad862a7e374bf8c0fd509f359da9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_368(CardRegistry& r) {
    r.registerCard(368, std::make_unique<NotSoFast>());
}

} // namespace riftbound
