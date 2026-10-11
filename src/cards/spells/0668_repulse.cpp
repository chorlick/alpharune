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
    //
    // Peek-and-pop: Repulse has been popped by FEPR, so the item it answers is
    // the top of the chain; it never scans or flags the chain. What that item
    // chooses is ChainItem::chosen_objects (everything it has chosen) together
    // with its play-time targets. A triggered ability is never handed targets by
    // the engine: its event's subject is ChainItem::triggering_subject, which is
    // not a choice and is deliberately not read here.

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
    /// The friendly unit Repulse would choose against the chain top.
    static GameObjectId answerable(const GameState& state, PlayerId controller) {
        if (state.chain.items.empty()) return kInvalidId;
        const auto& top = state.chain.items.back();
        if (top.controller == controller) return kInvalidId;          // "an enemy ..."
        if (!top.is_spell && !top.is_ability) return kInvalidId;      // "... spell or ability"
        return soleFriendlyUnitChosen(state, top, controller);
    }

    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return answerable(state, controller) != kInvalidId;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "Choose a friendly unit at a battlefield": the only unit that can
        // make the rest of the text do anything is the one the enemy item
        // chooses, so that is the choice offered. pickTarget records it on the
        // chain item and announces it ("When you choose a friendly unit").
        std::vector<GameObjectId> legal;
        if (GameObjectId u = answerable(ctx.state, ctx.controller); u != kInvalidId)
            legal.push_back(u);
        GameObjectId unit = pickTarget(ctx, "Repulse: choose a friendly unit", legal);
        if (unit == kInvalidId) return;                 // waiting, or nothing to answer
        // Re-check at use: the chain top must still choose that unit alone.
        if (answerable(ctx.state, ctx.controller) != unit) return;
        auto& top = ctx.state.chain.items.back();
        if (top.is_spell) {
            counterChainTop(ctx);                       // asks isUncounterable itself
            return;
        }
        // An ability (CR 426): it is removed from the chain; there is no card
        // to move, its source stays where it is.
        if (isUncounterable(ctx, top)) {
            RIFT_TRACE(ctx.events, "REPULSE: that ability can't be countered");
            return;
        }
        std::string name = ctx.state.objectExists(top.source)
            ? ctx.state.getObject(top.source).name : std::string("?");
        ctx.state.chain.items.pop_back();
        RIFT_TRACE(ctx.events, "COUNTER: ability of " + name + " countered (Repulse)");
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
