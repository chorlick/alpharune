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

class ThrillOfTheHunt : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool needsPlayTimeTarget() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId unit_id = pickTarget(ctx, "Thrill of the Hunt", legal);
        if (unit_id == kInvalidId || !ctx.state.objectExists(unit_id)) return;
        auto owner = ctx.state.getObject(unit_id).owner;
        std::string uname = ctx.state.getObject(unit_id).name;
        const bool token = ctx.state.getObject(unit_id).isToken();

        // The battlefield is asked BEFORE anything changes: pickMode suspends
        // and this callback is re-entered from the top with the answer, so
        // the banish below must come after the last choice or it would run
        // once per pass. Use pickMode so the agent records the battlefield
        // choice (one option per battlefield on the board, not just
        // controlled ones).
        // Step 1: banish the friendly unit (its gear is shed). It comes back
        // as a new object, with no damage, buffs, statuses or combat role:
        // banishObject made it one as it left (CR 124.1,
        // EffectExecutor::leftBoard), so nothing is reset here.
        auto banish = [&]() { ctx.executor.banishObject(unit_id); };
        // CR 183.1: a banished token ceases to exist — nothing to play back.
        if (token) {
            banish();
            RIFT_TRACE(ctx.events, "THRILL: " + uname + " (a token) is banished and gone");
            return;
        }
        // "To any battlefield": any its OWNER may play a unit to, whoever
        // controls it — not one where "Units can't be played here." (Rockfall
        // Path) and none for an owner who can only play units to their base
        // (battlefieldTakesUnitsFrom, the one rule for where an effect plays
        // a unit; CR 054.1).
        std::vector<LocationId> locations;
        std::vector<std::string> labels;
        for (const auto& bf : ctx.state.battlefields) {
            if (!battlefieldTakesUnitsFrom(ctx.state, owner, bf)) continue;
            locations.push_back(LocationId{BattlefieldLocation{bf.id}});
            std::string bf_name = "BF#" + std::to_string(static_cast<int>(bf.id.raw()));
            if (ctx.state.objectExists(bf.card_object_id)) {
                bf_name = ctx.state.getObject(bf.card_object_id).name;
            }
            labels.push_back(bf_name);
        }
        if (locations.empty()) {
            // No battlefield it can be played to: the text names no other
            // place, so it is not played and stays banished (Vendetta FAQ,
            // Temporal Breach: "it isn't possible to play the unit ... The
            // unit will remain in Banishment").
            banish();
            RIFT_TRACE(ctx.events, "THRILL: " + uname +
                                " can't be played to any battlefield — it stays banished");
            return;
        }
        uint32_t legal_mask = (locations.size() >= 32)
            ? 0xFFFFFFFFu
            : ((1u << locations.size()) - 1);
        // At resolution: an effect play's location is chosen as it plays (CR 420.3.b.2; batch M1).
        int chosen = pickMode(ctx, "Thrill: which battlefield",
                              static_cast<int>(locations.size()), labels, legal_mask);
        if (chosen == -1) return;  // yielded for agent input
        if (chosen < 0 || static_cast<size_t>(chosen) >= locations.size()) chosen = 0;
        banish();
        // Step 2: owner plays it to the chosen battlefield, ignoring cost.
        ctx.executor.playIgnoringCost(owner, unit_id, locations[chosen]);
        RIFT_TRACE(ctx.events, "THRILL: " + uname + " played to " + labels[chosen] +
                             ", ignoring cost");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 745;
        d.def_id = R"RB(unl-184-219)RB";
        d.name = R"RB(Thrill of the Hunt)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-184/219)RB";
        d.collector_number = 184;
        d.artist = R"RB(华锐)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Body};
        d.tags = {R"RB(Rengar)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Banish a friendly unit, then its owner plays it to any battlefield, ignoring its cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2753c2b3ea4fd55e5225a4451a29736b5c3434a8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_745(CardRegistry& r) {
    r.registerCard(745, std::make_unique<ThrillOfTheHunt>());
}

} // namespace riftbound
