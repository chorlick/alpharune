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

class BellowsBreath : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Repeat] [1][B]" (CR 820): the Repeat cost, as data the engine charges.
    std::vector<RepeatCost> repeatCosts() const override {
        return {RepeatCost{.energy = 1, .power = 1, .power_domain = Domain::Mind}};
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 3, .must_be_unit = true, .optional = true};
    }
    // "up to three units at the same location": no unit at all (always a
    // choice, CR 355.13), then every set of one, two or three units that share
    // a location.
    std::optional<std::vector<std::vector<GameObjectId>>> enumerateLegalTargetSets(
        const GameState& state, PlayerId controller) const override {
        std::vector<std::pair<LocationId, std::vector<GameObjectId>>> by_location;
        for (auto id : enumerateLegalTargets(state, controller)) {
            const LocationId loc = *state.getObject(id).location;
            auto it = std::find_if(by_location.begin(), by_location.end(),
                                   [&](const auto& e) { return e.first == loc; });
            if (it == by_location.end()) by_location.push_back({loc, {id}});
            else it->second.push_back(id);
        }
        std::vector<std::vector<GameObjectId>> sets;
        sets.push_back({});
        for (const auto& group : by_location) {
            const auto& u = group.second;
            for (size_t a = 0; a < u.size(); ++a) {
                sets.push_back({u[a]});
                for (size_t b = a + 1; b < u.size(); ++b) {
                    sets.push_back({u[a], u[b]});
                    for (size_t c = b + 1; c < u.size(); ++c)
                        sets.push_back({u[a], u[b], u[c]});
                }
            }
        }
        return sets;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty()) return;
        // "at the same location" is a requirement on the targets as a group
        // (CR 355.11). The targets still legal are grouped by where they are
        // NOW: if they no longer share a location, the controller chooses
        // which legal subset is affected — all of the chosen units at one
        // location, never part of them (CR 355.11.b, 355.11.b.1, whose
        // example is this card). Units that moved together are still a group.
        std::vector<std::pair<LocationId, std::vector<GameObjectId>>> groups;
        for (auto t : targets) {
            if (!ctx.state.objectExists(t)) continue;
            const auto& obj = ctx.state.getObject(t);
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            const LocationId loc = *obj.location;
            auto it = std::find_if(groups.begin(), groups.end(),
                                   [&](const auto& g) { return g.first == loc; });
            if (it == groups.end()) { groups.push_back({loc, {t}}); continue; }
            // de-dup (action gen may repeat in degenerate cases)
            if (std::find(it->second.begin(), it->second.end(), t) == it->second.end())
                it->second.push_back(t);
        }
        if (groups.empty()) return;
        size_t use = 0;
        if (groups.size() > 1) {
            std::vector<std::string> labels;
            for (const auto& g : groups) {
                std::string names;
                for (auto t : g.second) {
                    if (!names.empty()) names += ", ";
                    names += ctx.state.getObject(t).name;
                }
                labels.push_back("Deal 1 to " + names);
            }
            const int which = pickMode(ctx, "Bellows Breath: units at the same location",
                                       static_cast<int>(groups.size()), labels);
            if (which == -1) return;   // waiting on the controller
            if (which >= 0 && which < static_cast<int>(groups.size()))
                use = static_cast<size_t>(which);
        }
        // Collect first, kill lethal after (AoE iterator-safety per CLAUDE.md).
        const std::vector<GameObjectId> hit = groups[use].second;
        for (auto t : hit) {
            if (ctx.state.objectExists(t)) ctx.executor.dealDamage(t, 1, ctx.source);
        }
        {   // the batch is for these kills only
            DeathBatch together(ctx.executor, hit);   // they die at the same time
            for (auto t : hit) {
                if (ctx.state.objectExists(t) &&
                    ctx.state.getObject(t).hasLethalDamage()) {
                    ctx.executor.killObject(t);
                }
            }
        }
        ctx.events.logTrace("BELLOWS BREATH: dealt 1 to " +
                             std::to_string(hit.size()) + " units at same location");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 402;
        d.def_id = R"RB(sfd-080-221)RB";
        d.name = R"RB(Bellows Breath)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-080/221)RB";
        d.collector_number = 80;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
[Repeat] [1][B] (You may pay the additional cost to repeat this spell's effect.)
Deal 1 to up to three units at the same location.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f0ae1c4bb788f6fadc251c9d9b36f3f92eef4c56-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_402(CardRegistry& r) {
    r.registerCard(402, std::make_unique<BellowsBreath>());
}

} // namespace riftbound
